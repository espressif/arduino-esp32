/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file BLEL2CAP.nimble.cpp
 * @brief NimBLE Connection-Oriented Channels (CoC) L2CAP: server, client connect, and channel I/O.
 */

#include "core/BLEGuards.h"
#if BLE_L2CAP_SUPPORTED && BLE_NIMBLE

#include "l2cap/BLEL2CAP.nimble.h"
#include "BLE.h"
#include "BLE.nimble.h"
#include "l2cap/BLEL2CAPImpl.h"
#include "core/BLEImplHelpers.h"
#include "core/BLESync.h"
#include "esp32-hal-log.h"

#include <algorithm>

// RX flatten hot-path threshold — NOT the channel CoC MTU. Channel MTU is
// whatever the caller passed to createL2CAPServer / connectL2CAP (commonly
// 128, 256, 512, …). Most SDUs fit in a small stack buffer; larger ones spill
// to a heap vector in flattenRxSdu(). Keeping this fixed avoids a VLA / per-
// packet MTU-sized stack allocation on the NimBLE host task.
static const size_t kL2capRxStackBuf = 256;

// Flatten a received SDU into contiguous bytes for the data callback. Prefers
// the caller-provided stack buffer, spilling to @p heapBuf only when the SDU is
// larger than the stack buffer, which avoids a heap allocation on the RX hot
// path for typical payloads. The returned pointer stays valid while both
// buffers are in scope.
static const uint8_t *flattenRxSdu(struct os_mbuf *rxBuf, uint8_t *stackBuf, size_t stackCap, std::vector<uint8_t> &heapBuf, uint16_t &outLen) {
  outLen = OS_MBUF_PKTLEN(rxBuf);
  uint8_t *dst = stackBuf;
  if (outLen > stackCap) {
    heapBuf.resize(outLen);
    dst = heapBuf.data();
  }
  os_mbuf_copydata(rxBuf, 0, outLen, dst);
  return dst;
}

// --------------------------------------------------------------------------
// BLEL2CAPChannel
// --------------------------------------------------------------------------

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

BLEL2CAPChannel::BLEL2CAPChannel() : _impl(nullptr) {}

BLEL2CAPChannel::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEL2CAPChannel::write(const uint8_t *data, size_t len) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected || !impl.chan || !data) {
    return BTStatus::InvalidState;
  }
  if (len == 0) {
    return BTStatus::OK;
  }

  // NimBLE does not segment oversized SDUs — each ble_l2cap_send must be
  // ≤ peer_coc_mtu. Chunk here so the public API matches its documented
  // "larger than getMTU() will be segmented" contract.
  struct ble_l2cap_chan_info info;
  if (ble_l2cap_get_chan_info(impl.chan, &info) != 0 || info.peer_coc_mtu == 0) {
    log_e("L2CAP write: failed to read peer CoC MTU");
    return BTStatus::Fail;
  }
  const size_t chunkMax = info.peer_coc_mtu;

  size_t offset = 0;
  while (offset < len) {
    if (!impl.connected || !impl.chan) {
      return BTStatus::InvalidState;
    }
    const size_t chunk = std::min(chunkMax, len - offset);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, chunk);
    if (!om) {
      log_e("Failed to allocate mbuf for L2CAP write");
      return BTStatus::NoMemory;
    }

    // Retry on ESTALLED / EBUSY: the stack owns at most one in-flight SDU, so
    // a multi-chunk write must wait for COC_TX_UNSTALLED between chunks.
    for (;;) {
      impl.txSync.take();
      int rc = ble_l2cap_send(impl.chan, om);
      if (rc == 0) {
        // Sent immediately — no unstall event will arrive for this SDU.  // codespell:ignore unstall
        impl.txSync.give(BTStatus::OK);
        break;
      }
      if (rc == BLE_HS_ESTALLED) {
        // Controller took ownership; wait for credits before the next chunk.
        BTStatus st = impl.txSync.wait(10000);
        if (st != BTStatus::OK) {
          log_e("L2CAP write: timed out waiting for TX unstall");  // codespell:ignore unstall
          return st == BTStatus::Timeout ? BTStatus::Timeout : st;
        }
        break;
      }
      if (rc == BLE_HS_EBUSY) {
        // Previous SDU still draining — wait and retry the same mbuf.
        BTStatus st = impl.txSync.wait(10000);
        if (st != BTStatus::OK) {
          log_e("L2CAP write: timed out waiting for TX unstall (EBUSY)");  // codespell:ignore unstall
          os_mbuf_free_chain(om);
          return st == BTStatus::Timeout ? BTStatus::Timeout : st;
        }
        continue;
      }
      log_e("ble_l2cap_send failed: rc=%d", rc);
      impl.txSync.give(BTStatus::Fail);
      os_mbuf_free_chain(om);
      return BTStatus::Fail;
    }
    offset += chunk;
  }
  return BTStatus::OK;
}

BTStatus BLEL2CAPChannel::disconnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected || !impl.chan) {
    log_w("L2CAP: disconnect called but not connected");
    return BTStatus::InvalidState;
  }

  bool wait = !nimbleIsHostTask();
  if (wait) {
    impl.disconnectSync.take();
  }
  int rc = ble_l2cap_disconnect(impl.chan);
  if (rc != 0) {
    if (wait) {
      impl.disconnectSync.give(BTStatus::Fail);
    }
    log_e("L2CAP: ble_l2cap_disconnect rc=%d", rc);
    return BTStatus::Fail;
  }
  if (wait && impl.connected) {
    BTStatus st = impl.disconnectSync.wait(5000);
    if (st != BTStatus::OK) {
      log_e("L2CAP: disconnect timed out waiting for COC_DISCONNECTED");
      return st;
    }
  } else if (wait) {
    impl.disconnectSync.give(BTStatus::OK);
  }
  return BTStatus::OK;
}

BTStatus BLEL2CAPChannel::onData(DataHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDataCb = handler;
  return BTStatus::OK;
}

BTStatus BLEL2CAPChannel::onDisconnect(DisconnectHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = handler;
  return BTStatus::OK;
}

BTStatus BLEL2CAPChannel::resetCallbacks() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDataCb = nullptr;
  impl.onDisconnectCb = nullptr;
  return BTStatus::OK;
}

bool BLEL2CAPChannel::isConnected() const {
  return _impl && _impl->connected;
}

uint16_t BLEL2CAPChannel::getPSM() const {
  return _impl ? _impl->psm : 0;
}

uint16_t BLEL2CAPChannel::getConnHandle() const {
  return _impl ? _impl->connHandle : BLE_HS_CONN_HANDLE_NONE;
}

uint16_t BLEL2CAPChannel::getMTU() const {
  if (!_impl || !_impl->chan) {
    return 0;
  }
  // NimBLE dropped ble_l2cap_get_our_mtu(); the effective CoC MTU is now read
  // from the channel info struct (our_coc_mtu is the local SDU limit).
  struct ble_l2cap_chan_info info;
  if (ble_l2cap_get_chan_info(_impl->chan, &info) != 0) {
    return 0;
  }
  return info.our_coc_mtu;
}

// --------------------------------------------------------------------------
// BLEL2CAPServer
// --------------------------------------------------------------------------

BLEL2CAPServer::BLEL2CAPServer() : _impl(nullptr) {}

BLEL2CAPServer::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEL2CAPServer::onAccept(AcceptHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onAcceptCb = handler;
  return BTStatus::OK;
}

BTStatus BLEL2CAPServer::onData(DataHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDataCb = handler;
  return BTStatus::OK;
}

BTStatus BLEL2CAPServer::onDisconnect(DisconnectHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = handler;
  return BTStatus::OK;
}

BTStatus BLEL2CAPServer::resetCallbacks() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onAcceptCb = nullptr;
  impl.onDataCb = nullptr;
  impl.onDisconnectCb = nullptr;
  return BTStatus::OK;
}

uint16_t BLEL2CAPServer::getPSM() const {
  return _impl ? _impl->psm : 0;
}

uint16_t BLEL2CAPServer::getMTU() const {
  return _impl ? _impl->mtu : 0;
}

// --------------------------------------------------------------------------
// L2CAP event handler
// --------------------------------------------------------------------------

/**
 * @brief Allocates a packet mbuf from the server's SDU memory pool.
 * @return A new mbuf, or nullptr if allocation fails.
 */
struct os_mbuf *BLEL2CAPServer::Impl::allocSdu() {
  struct os_mbuf *sdu = os_mbuf_get_pkthdr(&sduPool, 0);
  return sdu;
}

/**
 * @brief Stack callback for L2CAP CoC: accept, data, connect, disconnect, and flow-control events.
 * @param event NimBLE L2CAP event structure.
 * @param arg Opaque @c BLEL2CAPServer::Impl pointer.
 * @return Zero in normal success paths; @c BLE_HS_ENOMEM on allocation failure; stack-defined otherwise.
 */
int BLEL2CAPServer::Impl::l2capEvent(struct ble_l2cap_event *event, void *arg) {
  auto *server = static_cast<BLEL2CAPServer::Impl *>(arg);
  if (!server) {
    return 0;
  }

  switch (event->type) {
    case BLE_L2CAP_EVENT_COC_CONNECTED:
    {
      if (event->connect.status != 0) {
        log_w("L2CAP CoC connect failed: status=%d", event->connect.status);
        return 0;
      }

      auto chanImpl = std::make_shared<BLEL2CAPChannel::Impl>();
      chanImpl->chan = event->connect.chan;
      chanImpl->psm = server->psm;
      chanImpl->connHandle = event->connect.conn_handle;
      chanImpl->connected = true;

      {
        BLELockGuard lock(server->mtx);
        // Inherit server-level data/disconnect callbacks
        chanImpl->onDataCb = server->onDataCb;
        chanImpl->onDisconnectCb = server->onDisconnectCb;
        server->channels.push_back(chanImpl);
      }

      BLEL2CAPServer::AcceptHandler cb;
      {
        BLELockGuard lock(server->mtx);
        cb = server->onAcceptCb;
      }
      if (cb) {
        BLEL2CAPChannel channel = BLEL2CAPChannelImplCommon::makeHandle(chanImpl);
        cb(channel);
      }
      break;
    }

    case BLE_L2CAP_EVENT_COC_DISCONNECTED:
    {
      BLEL2CAPChannel::DisconnectHandler cb;
      BLEL2CAPChannel handle;
      std::shared_ptr<BLEL2CAPChannel::Impl> discImpl;
      {
        BLELockGuard lock(server->mtx);
        for (auto it = server->channels.begin(); it != server->channels.end(); ++it) {
          if ((*it)->chan == event->disconnect.chan) {
            discImpl = *it;
            discImpl->connected = false;
            discImpl->chan = nullptr;
            // Unblock any write() waiting between SDUs.
            discImpl->txSync.give(BTStatus::Fail);

            cb = discImpl->onDisconnectCb;
            handle = BLEL2CAPChannelImplCommon::makeHandle(discImpl);
            server->channels.erase(it);
            break;
          }
        }
      }
      if (cb) {
        cb(handle);
      }
      if (discImpl) {
        discImpl->disconnectSync.give(BTStatus::OK);
      }
      break;
    }

    case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
    {
      struct os_mbuf *rxBuf = event->receive.sdu_rx;
      if (!rxBuf) {
        break;
      }

      uint8_t stackBuf[kL2capRxStackBuf];
      std::vector<uint8_t> heapBuf;
      uint16_t dataLen;
      const uint8_t *data = flattenRxSdu(rxBuf, stackBuf, sizeof(stackBuf), heapBuf, dataLen);
      // App owns the SDU after DATA_RECEIVED — free it or the CoC pool drains
      // and peer credits stop (multi-SDU write then stalls on TX_UNSTALLED).
      os_mbuf_free_chain(rxBuf);

      // Provide a new SDU buffer for the next receive
      struct os_mbuf *nextBuf = server->allocSdu();
      if (nextBuf) {
        ble_l2cap_recv_ready(event->receive.chan, nextBuf);
      }

      // Find the channel and invoke callback
      BLEL2CAPChannel::DataHandler cb;
      std::shared_ptr<BLEL2CAPChannel::Impl> chanImpl;
      {
        BLELockGuard lock(server->mtx);
        for (auto &ch : server->channels) {
          if (ch->chan == event->receive.chan) {
            chanImpl = ch;
            cb = ch->onDataCb;
            break;
          }
        }
      }
      if (cb && chanImpl) {
        BLEL2CAPChannel channel = BLEL2CAPChannelImplCommon::makeHandle(chanImpl);
        cb(channel, data, dataLen);
      }
      break;
    }

    case BLE_L2CAP_EVENT_COC_ACCEPT:
    {
      struct os_mbuf *sdu = server->allocSdu();
      if (!sdu) {
        log_e("Failed to allocate SDU for L2CAP accept");
        return BLE_HS_ENOMEM;
      }
      event->accept.conn_handle = event->accept.conn_handle;
      event->accept.peer_sdu_size = server->mtu;
      struct ble_l2cap_chan_info info;
      ble_l2cap_get_chan_info(event->accept.chan, &info);
      return ble_l2cap_recv_ready(event->accept.chan, sdu) == 0 ? 0 : BLE_HS_ENOMEM;
    }

    case BLE_L2CAP_EVENT_COC_TX_UNSTALLED:
      // Peer credits replenished; wake any multi-chunk write() waiting between
      // SDUs. A single-chunk write that returned ESTALLED also waits here so the
      // next write() does not race the single-SDU tx buffer.
      log_d("L2CAP TX un-stalled status=%d", event->tx_unstalled.status);
      {
        BLELockGuard lock(server->mtx);
        for (auto &ch : server->channels) {
          if (ch->chan == event->tx_unstalled.chan) {
            ch->txSync.give(event->tx_unstalled.status == 0 ? BTStatus::OK : BTStatus::Fail);
            break;
          }
        }
      }
      break;
  }

  return 0;
}

// --------------------------------------------------------------------------
// L2CAP setup helpers (used by BLEClass factories in NimBLECore.cpp)
// --------------------------------------------------------------------------

struct L2CAPClientCtx {
  std::shared_ptr<BLEL2CAPChannel::Impl> impl;
  // Set when connectSync waits time out on the user task: the stack still
  // holds this ctx as its event arg, so it must not be deleted then. The
  // COC_CONNECTED handler checks this and disconnects immediately instead.
  bool abandoned = false;
};

static SemaphoreHandle_t sClientCtxMtx = xSemaphoreCreateRecursiveMutex();
static std::vector<L2CAPClientCtx *> sClientContexts;
// Server Impls are owned by user handles; keep weak refs so nimbleCleanupL2CAP
// can invalidate their accepted channels at BLE.end().
static std::vector<std::weak_ptr<BLEL2CAPServer::Impl>> sL2CAPServers;

bool nimbleSetupL2CAPServer(const std::shared_ptr<BLEL2CAPServer::Impl> &server, uint16_t psm, uint16_t mtu) {
  server->psm = psm;
  server->mtu = mtu;

  const int numBufs = CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM + 1;
  const uint16_t bufSize = OS_ALIGN(mtu + 4, OS_ALIGNMENT);
  const size_t memSize = OS_MEMPOOL_SIZE(numBufs, bufSize) * sizeof(os_membuf_t);

  server->sduMem = static_cast<uint8_t *>(malloc(memSize));
  if (!server->sduMem) {
    log_e("Failed to allocate L2CAP SDU memory pool");
    return false;
  }

  int rc = os_mempool_init(&server->sduMemPool, numBufs, bufSize, server->sduMem, "l2cap_sdu");
  if (rc != 0) {
    log_e("os_mempool_init failed: rc=%d", rc);
    free(server->sduMem);
    server->sduMem = nullptr;
    return false;
  }

  rc = os_mbuf_pool_init(&server->sduPool, &server->sduMemPool, bufSize, numBufs);
  if (rc != 0) {
    log_e("os_mbuf_pool_init failed: rc=%d", rc);
    free(server->sduMem);
    server->sduMem = nullptr;
    return false;
  }

  rc = ble_l2cap_create_server(psm, mtu, BLEL2CAPServer::Impl::l2capEvent, server.get());
  if (rc != 0) {
    log_e("ble_l2cap_create_server failed: rc=%d", rc);
    free(server->sduMem);
    server->sduMem = nullptr;
    return false;
  }

  {
    BLELockGuard lock(sClientCtxMtx);
    sL2CAPServers.push_back(server);
  }
  return true;
}

static void removeClientCtx(L2CAPClientCtx *ctx) {
  {
    BLELockGuard lock(sClientCtxMtx);
    for (auto it = sClientContexts.begin(); it != sClientContexts.end(); ++it) {
      if (*it == ctx) {
        sClientContexts.erase(it);
        break;
      }
    }
  }
  delete ctx;
}

void nimbleCleanupL2CAP() {
  std::vector<L2CAPClientCtx *> pending;
  std::vector<std::weak_ptr<BLEL2CAPServer::Impl>> servers;
  {
    BLELockGuard lock(sClientCtxMtx);
    pending.swap(sClientContexts);
    servers.swap(sL2CAPServers);
  }
  for (auto *ctx : pending) {
    if (ctx->impl) {
      ctx->impl->connected = false;
      ctx->impl->chan = nullptr;
      ctx->impl->txSync.give(BTStatus::Fail);
      ctx->impl->connectSync.give(BTStatus::Fail);
      ctx->impl->disconnectSync.give(BTStatus::OK);
    }
    delete ctx;
  }
  // Server-side channels: the host is already stopped when this runs (called
  // after nimble_port_deinit in BLEClass::end()), so no stack disconnect is
  // possible -- invalidate the channels so their dangling ble_l2cap_chan*
  // cannot be used after a restart and waiters are released.
  for (auto &weak : servers) {
    auto server = weak.lock();
    if (!server) {
      continue;
    }
    BLELockGuard lock(server->mtx);
    for (auto &ch : server->channels) {
      ch->connected = false;
      ch->chan = nullptr;
      ch->txSync.give(BTStatus::Fail);
      ch->connectSync.give(BTStatus::Fail);
      ch->disconnectSync.give(BTStatus::OK);
    }
    server->channels.clear();
  }
}

bool nimbleSetupL2CAPChannel(const std::shared_ptr<BLEL2CAPChannel::Impl> &chanImpl, uint16_t connHandle, uint16_t psm, uint16_t mtu) {
  chanImpl->psm = psm;
  chanImpl->connHandle = connHandle;
  chanImpl->mtu = mtu;

  // On the NimBLE host task we cannot block waiting for COC_CONNECTED, and
  // returning a handle that only becomes connected later contradicts the
  // synchronous connect contract -- fail so the caller gets an empty handle.
  if (nimbleIsHostTask()) {
    log_e("L2CAP: connectL2CAP cannot block on the NimBLE host task; call it from another task");
    return false;
  }

  struct os_mbuf *sdu = ble_hs_mbuf_from_flat(nullptr, 0);
  if (!sdu) {
    sdu = os_msys_get_pkthdr(mtu, 0);
  }
  if (!sdu) {
    log_e("Failed to allocate SDU for L2CAP connect");
    return false;
  }

  auto *ctx = new L2CAPClientCtx{chanImpl};
  {
    BLELockGuard lock(sClientCtxMtx);
    sClientContexts.push_back(ctx);
  }

  auto clientEventFn = [](struct ble_l2cap_event *event, void *arg) -> int {
    auto *ctx = static_cast<L2CAPClientCtx *>(arg);
    if (!ctx) {
      return 0;
    }
    auto impl = ctx->impl;

    switch (event->type) {
      case BLE_L2CAP_EVENT_COC_CONNECTED:
        if (event->connect.status == 0) {
          bool abandoned;
          {
            BLELockGuard lock(sClientCtxMtx);
            abandoned = ctx->abandoned;
          }
          if (abandoned) {
            // The user-side connect already timed out and returned an empty
            // handle; tear this orphan channel down immediately.
            log_w("L2CAP client connect completed after timeout; disconnecting orphan channel");
            impl->connected = false;
            impl->chan = nullptr;
            ble_l2cap_disconnect(event->connect.chan);
            removeClientCtx(ctx);
            break;
          }
          impl->chan = event->connect.chan;
          impl->connected = true;
          impl->connHandle = event->connect.conn_handle;
          impl->connectSync.give(BTStatus::OK);
        } else {
          log_w("L2CAP client connect failed: status=%d", event->connect.status);
          impl->connectSync.give(BTStatus::Fail);
          removeClientCtx(ctx);
        }
        break;

      case BLE_L2CAP_EVENT_COC_DISCONNECTED:
      {
        impl->connected = false;
        impl->chan = nullptr;
        impl->txSync.give(BTStatus::Fail);
        BLEL2CAPChannel::DisconnectHandler cb;
        {
          BLELockGuard lock(impl->mtx);
          cb = impl->onDisconnectCb;
        }
        if (cb) {
          BLEL2CAPChannel channel = BLEL2CAPChannelImplCommon::makeHandle(impl);
          cb(channel);
        }
        impl->disconnectSync.give(BTStatus::OK);
        removeClientCtx(ctx);
        break;
      }

      case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
      {
        struct os_mbuf *rxBuf = event->receive.sdu_rx;
        if (!rxBuf) {
          break;
        }
        uint8_t stackBuf[kL2capRxStackBuf];
        std::vector<uint8_t> heapBuf;
        uint16_t dataLen;
        const uint8_t *data = flattenRxSdu(rxBuf, stackBuf, sizeof(stackBuf), heapBuf, dataLen);
        os_mbuf_free_chain(rxBuf);

        struct os_mbuf *nextBuf = os_msys_get_pkthdr(impl->mtu, 0);
        if (nextBuf) {
          ble_l2cap_recv_ready(event->receive.chan, nextBuf);
        }

        BLEL2CAPChannel::DataHandler cb;
        {
          BLELockGuard lock(impl->mtx);
          cb = impl->onDataCb;
        }
        if (cb) {
          BLEL2CAPChannel channel = BLEL2CAPChannelImplCommon::makeHandle(impl);
          cb(channel, data, dataLen);
        }
        break;
      }

      case BLE_L2CAP_EVENT_COC_TX_UNSTALLED: impl->txSync.give(event->tx_unstalled.status == 0 ? BTStatus::OK : BTStatus::Fail); break;
    }
    return 0;
  };

  bool wait = !nimbleIsHostTask();
  if (wait) {
    chanImpl->connectSync.take();
  }
  int rc = ble_l2cap_connect(connHandle, psm, mtu, sdu, clientEventFn, ctx);
  if (rc != 0) {
    if (wait) {
      chanImpl->connectSync.give(BTStatus::Fail);
    }
    log_e("ble_l2cap_connect failed: rc=%d", rc);
    removeClientCtx(ctx);
    return false;
  }

  if (wait) {
    BTStatus st = chanImpl->connectSync.wait(5000);
    if (st != BTStatus::OK) {
      // Do not delete ctx: the stack still holds it as the event arg and the
      // connect may still complete. Mark it abandoned so COC_CONNECTED
      // disconnects the orphan channel immediately (and so
      // nimbleCleanupL2CAP can reclaim ctx at BLE.end()).
      {
        BLELockGuard lock(sClientCtxMtx);
        ctx->abandoned = true;
      }
      if (chanImpl->chan) {
        ble_l2cap_disconnect(chanImpl->chan);
        chanImpl->chan = nullptr;
        chanImpl->connected = false;
      }
      log_e("L2CAP: connect timed out; channel will be torn down if it completes");
      return false;
    }
  }
  return true;
}

#endif /* BLE_L2CAP_SUPPORTED && BLE_NIMBLE */
