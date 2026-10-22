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

#include <nimble/nimble_port.h>
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

struct LazyL2CAPMutex {
  SemaphoreHandle_t handle;
  StaticSemaphore_t storage;

  bool init() {
    if (!handle) {
      handle = xSemaphoreCreateRecursiveMutexStatic(&storage);
    }
    return handle != nullptr;
  }

  operator SemaphoreHandle_t() {
    return handle;
  }
};

static LazyL2CAPMutex sL2CAPMtx = {};
// Strong ownership spans the complete host callback. Completion removes the
// registry entry, while operationEventCallback keeps a local copy until return.
static std::vector<std::shared_ptr<BLEL2CAPChannel::Impl>> &operationChannels() {
  static std::vector<std::shared_ptr<BLEL2CAPChannel::Impl>> channels;
  return channels;
}
static bool sL2CAPStopping = true;

class L2CAPOperationGuard {
public:
  L2CAPOperationGuard(SemaphoreHandle_t mtx, bool block) : _mtx(mtx), _locked(mtx && xSemaphoreTakeRecursive(mtx, block ? portMAX_DELAY : 0) == pdTRUE) {}
  ~L2CAPOperationGuard() {
    if (_locked) {
      xSemaphoreGiveRecursive(_mtx);
    }
  }
  explicit operator bool() const {
    return _locked;
  }

private:
  SemaphoreHandle_t _mtx;
  bool _locked;
};

static BTStatus l2capStatusFromRc(int rc) {
  if (rc == BLE_HS_EBUSY || rc == BLE_HS_ESTALLED) {
    return BTStatus::Busy;
  }
  if (rc == BLE_HS_ENOTCONN) {
    return BTStatus::NotConnected;
  }
  return BTStatus::Fail;
}

static bool l2capChannelSnapshot(BLEL2CAPChannel::Impl *impl, struct ble_l2cap_chan *&chan) {
  BLELockGuard lock(impl->mtx);
  chan = impl->chan;
  return impl->connected && chan;
}

static void deinitOperationEventLocked(BLEL2CAPChannel::Impl *impl) {
  if (impl->operationEventInited) {
    if (ble_npl_event_is_queued(&impl->operationEvent)) {
      ble_npl_eventq_remove(nimble_port_get_dflt_eventq(), &impl->operationEvent);
    }
    // ESP-IDF's npl_freertos_event_run() does not touch the backing event after
    // fn() returns, so the final callback may safely return its pool block here.
    ble_npl_event_deinit(&impl->operationEvent);
    impl->operationEventInited = false;
  }
  auto &channels = operationChannels();
  channels.erase(
    std::remove_if(
      channels.begin(), channels.end(),
      [impl](const std::shared_ptr<BLEL2CAPChannel::Impl> &entry) { return entry.get() == impl; }
    ),
    channels.end()
  );
}

static void completeOperationLocked(BLEL2CAPChannel::Impl *impl, BTStatus status) {
  if (impl->operation == BLEL2CAPChannel::Impl::Operation::None) {
    return;
  }
  if (impl->writePendingMbuf) {
    os_mbuf_free_chain(impl->writePendingMbuf);
    impl->writePendingMbuf = nullptr;
  }
  impl->operation = BLEL2CAPChannel::Impl::Operation::None;
  impl->operationCancelled = false;
  impl->writeData = nullptr;
  impl->writeLen = 0;
  impl->writeOffset = 0;
  impl->writeChunkLen = 0;
  impl->writeChunkMax = 0;
  // Only queued or in-flight operations retain scarce NPL event backing.
  // Release it before the caller wakes, including on disconnect and timeout.
  deinitOperationEventLocked(impl);
  impl->operationSync.give(status);
}

static void completeOperation(BLEL2CAPChannel::Impl *impl, BTStatus status) {
  BLELockGuard lock(sL2CAPMtx);
  completeOperationLocked(impl, status);
}

static void processQueuedWrite(BLEL2CAPChannel::Impl *impl) {
  struct ble_l2cap_chan *chan = nullptr;
  if (!l2capChannelSnapshot(impl, chan)) {
    completeOperation(impl, BTStatus::NotConnected);
    return;
  }

  if (impl->writeChunkMax == 0) {
    struct ble_l2cap_chan_info info;
    if (ble_l2cap_get_chan_info(chan, &info) != 0 || info.peer_coc_mtu == 0) {
      completeOperation(impl, BTStatus::Fail);
      return;
    }
    impl->writeChunkMax = info.peer_coc_mtu;
  }

  while (impl->writeOffset < impl->writeLen) {
    if (impl->operationCancelled.load()) {
      return;
    }

    struct os_mbuf *om = nullptr;
    size_t chunk = 0;
    {
      BLELockGuard lock(sL2CAPMtx);
      if (impl->operation != BLEL2CAPChannel::Impl::Operation::Write) {
        return;
      }
      if (impl->writePendingMbuf) {
        om = impl->writePendingMbuf;
        chunk = impl->writeChunkLen;
        impl->writePendingMbuf = nullptr;
      } else {
        chunk = std::min(impl->writeChunkMax, impl->writeLen - impl->writeOffset);
      }
    }

    if (!om) {
      om = ble_hs_mbuf_from_flat(impl->writeData + impl->writeOffset, chunk);
      if (!om) {
        completeOperation(impl, BTStatus::NoMemory);
        return;
      }
    }

    int rc = ble_l2cap_send(chan, om);
    BLELockGuard lock(sL2CAPMtx);
    if (impl->operation != BLEL2CAPChannel::Impl::Operation::Write) {
      if (rc != 0 && rc != BLE_HS_ESTALLED) {
        os_mbuf_free_chain(om);
      }
      return;
    }
    if (rc == 0 || rc == BLE_HS_ESTALLED) {
      impl->writeOffset += chunk;
      impl->writeChunkLen = 0;
      if (rc == BLE_HS_ESTALLED && impl->writeOffset < impl->writeLen) {
        return;
      }
      continue;
    }
    if (rc == BLE_HS_EBUSY) {
      impl->writePendingMbuf = om;
      impl->writeChunkLen = chunk;
      return;
    }
    os_mbuf_free_chain(om);
    completeOperationLocked(impl, l2capStatusFromRc(rc));
    return;
  }

  completeOperation(impl, BTStatus::OK);
}

static void runQueuedOperation(BLEL2CAPChannel::Impl *impl) {
  {
    BLELockGuard lock(sL2CAPMtx);
    if (impl->operation == BLEL2CAPChannel::Impl::Operation::None) {
      return;
    }
    impl->operationHostRunning = true;
  }

  if (!impl->operationCancelled.load()) {
    switch (impl->operation) {
      case BLEL2CAPChannel::Impl::Operation::Write:
        processQueuedWrite(impl);
        break;

      case BLEL2CAPChannel::Impl::Operation::GetMTU:
      {
        struct ble_l2cap_chan *chan = nullptr;
        struct ble_l2cap_chan_info info;
        BTStatus status = BTStatus::NotConnected;
        if (l2capChannelSnapshot(impl, chan)) {
          status = ble_l2cap_get_chan_info(chan, &info) == 0 ? BTStatus::OK : BTStatus::Fail;
        }
        if (status == BTStatus::OK) {
          impl->operationMTU = info.our_coc_mtu;
        }
        completeOperation(impl, status);
        break;
      }

      case BLEL2CAPChannel::Impl::Operation::Disconnect:
      {
        struct ble_l2cap_chan *chan = nullptr;
        if (!l2capChannelSnapshot(impl, chan)) {
          completeOperation(impl, BTStatus::NotConnected);
          break;
        }
        int rc = ble_l2cap_disconnect(chan);
        if (rc != 0) {
          completeOperation(impl, l2capStatusFromRc(rc));
        }
        break;
      }

      case BLEL2CAPChannel::Impl::Operation::None:
        break;
    }
  }

  BLELockGuard lock(sL2CAPMtx);
  if (impl->operationCancelled.load()) {
    completeOperationLocked(impl, BTStatus::Timeout);
  }
  impl->operationHostRunning = false;
}

static void operationEventCallback(struct ble_npl_event *event) {
  auto *impl = static_cast<BLEL2CAPChannel::Impl *>(ble_npl_event_get_arg(event));
  std::shared_ptr<BLEL2CAPChannel::Impl> owner;
  {
    BLELockGuard lock(sL2CAPMtx);
    for (auto &channel : operationChannels()) {
      if (channel.get() == impl) {
        owner = channel;
        break;
      }
    }
  }
  if (owner) {
    runQueuedOperation(owner.get());
  }
}

static bool queueOperation(const std::shared_ptr<BLEL2CAPChannel::Impl> &impl, BLEL2CAPChannel::Impl::Operation operation) {
  impl->operationSync.take();
  BLELockGuard lock(sL2CAPMtx);
  if (sL2CAPStopping || impl->operation != BLEL2CAPChannel::Impl::Operation::None) {
    impl->operationSync.give(sL2CAPStopping ? BTStatus::InvalidState : BTStatus::Busy);
    return false;
  }
  if (!impl->operationEventInited) {
    ble_npl_event_init(&impl->operationEvent, operationEventCallback, impl.get());
    impl->operationEventInited = true;
    operationChannels().push_back(impl);
  }
  impl->operation = operation;
  impl->operationCancelled = false;
  impl->operationMTU = 0;
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &impl->operationEvent);
  return true;
}

static BTStatus waitForOperation(BLEL2CAPChannel::Impl *impl, uint32_t timeoutMs) {
  BTStatus status = impl->operationSync.wait(timeoutMs);
  if (status != BTStatus::Timeout) {
    return status;
  }

  {
    BLELockGuard lock(sL2CAPMtx);
    if (impl->operation != BLEL2CAPChannel::Impl::Operation::None) {
      impl->operationCancelled = true;
      if (!sL2CAPStopping && !impl->operationHostRunning && !ble_npl_event_is_queued(&impl->operationEvent)) {
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &impl->operationEvent);
      }
    }
  }
  // Keep the caller's data and channel shell alive until the host acknowledges
  // cancellation (or shutdown removes the event and releases this waiter).
  (void)impl->operationSync.wait(portMAX_DELAY);
  return BTStatus::Timeout;
}

static BTStatus writeDirectFromHost(BLEL2CAPChannel::Impl *impl, const uint8_t *data, size_t len) {
  struct ble_l2cap_chan *chan = nullptr;
  if (!l2capChannelSnapshot(impl, chan)) {
    return BTStatus::NotConnected;
  }

  struct ble_l2cap_chan_info info;
  if (ble_l2cap_get_chan_info(chan, &info) != 0 || info.peer_coc_mtu == 0) {
    return BTStatus::Fail;
  }

  size_t offset = 0;
  while (offset < len) {
    size_t chunk = std::min(static_cast<size_t>(info.peer_coc_mtu), len - offset);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset, chunk);
    if (!om) {
      return BTStatus::NoMemory;
    }
    int rc = ble_l2cap_send(chan, om);
    if (rc == 0 || rc == BLE_HS_ESTALLED) {
      offset += chunk;
      if (rc == BLE_HS_ESTALLED && offset < len) {
        return BTStatus::Busy;
      }
      continue;
    }
    os_mbuf_free_chain(om);
    return l2capStatusFromRc(rc);
  }
  return BTStatus::OK;
}

static void finishOperationForDisconnect(BLEL2CAPChannel::Impl *impl) {
  BLELockGuard lock(sL2CAPMtx);
  BTStatus status = impl->operation == BLEL2CAPChannel::Impl::Operation::Disconnect ? BTStatus::OK : BTStatus::NotConnected;
  completeOperationLocked(impl, status);
}

static void resumeWriteOperation(BLEL2CAPChannel::Impl *impl, int status) {
  if (status != 0) {
    completeOperation(impl, BTStatus::Fail);
    return;
  }
  runQueuedOperation(impl);
}

void nimbleReleaseL2CAPOperationEvent(BLEL2CAPChannel::Impl *impl) {
  BLELockGuard lock(sL2CAPMtx);
  deinitOperationEventLocked(impl);
  completeOperationLocked(impl, BTStatus::InvalidState);
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
  if (!data) {
    return BTStatus::InvalidState;
  }
  const bool hostTask = nimbleIsHostTask();
  L2CAPOperationGuard operationLock(impl.operationMtx, !hostTask);
  if (!operationLock) {
    return BTStatus::Busy;
  }
  struct ble_l2cap_chan *chan = nullptr;
  if (!l2capChannelSnapshot(&impl, chan)) {
    return BTStatus::NotConnected;
  }
  if (len == 0) {
    return BTStatus::OK;
  }

  if (hostTask) {
    return writeDirectFromHost(&impl, data, len);
  }

  impl.writeData = data;
  impl.writeLen = len;
  impl.writeOffset = 0;
  impl.writeChunkLen = 0;
  impl.writeChunkMax = 0;
  impl.writePendingMbuf = nullptr;
  if (!queueOperation(_impl, BLEL2CAPChannel::Impl::Operation::Write)) {
    return impl.operationSync.wait(0);
  }
  return waitForOperation(&impl, 10000);
}

BTStatus BLEL2CAPChannel::disconnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const bool hostTask = nimbleIsHostTask();
  L2CAPOperationGuard operationLock(impl.operationMtx, !hostTask);
  if (!operationLock) {
    return BTStatus::Busy;
  }
  struct ble_l2cap_chan *chan = nullptr;
  if (!l2capChannelSnapshot(&impl, chan)) {
    return BTStatus::NotConnected;
  }
  if (hostTask) {
    int rc = ble_l2cap_disconnect(chan);
    return rc == 0 ? BTStatus::OK : l2capStatusFromRc(rc);
  }

  if (!queueOperation(_impl, BLEL2CAPChannel::Impl::Operation::Disconnect)) {
    return impl.operationSync.wait(0);
  }
  return waitForOperation(&impl, 5000);
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
  if (!_impl) {
    return false;
  }
  BLELockGuard lock(_impl->mtx);
  return _impl->connected;
}

uint16_t BLEL2CAPChannel::getPSM() const {
  return _impl ? _impl->psm : 0;
}

uint16_t BLEL2CAPChannel::getConnHandle() const {
  return _impl ? _impl->connHandle : BLE_HS_CONN_HANDLE_NONE;
}

uint16_t BLEL2CAPChannel::getMTU() const {
  if (!_impl) {
    return 0;
  }
  const bool hostTask = nimbleIsHostTask();
  L2CAPOperationGuard operationLock(_impl->operationMtx, !hostTask);
  if (!operationLock) {
    return 0;
  }
  struct ble_l2cap_chan *chan = nullptr;
  if (!l2capChannelSnapshot(_impl.get(), chan)) {
    return 0;
  }
  if (hostTask) {
    struct ble_l2cap_chan_info info;
    return ble_l2cap_get_chan_info(chan, &info) == 0 ? info.our_coc_mtu : 0;
  }
  if (!queueOperation(_impl, BLEL2CAPChannel::Impl::Operation::GetMTU)) {
    (void)_impl->operationSync.wait(0);
    return 0;
  }
  return waitForOperation(_impl.get(), 5000) == BTStatus::OK ? _impl->operationMTU : 0;
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
            {
              BLELockGuard channelLock(discImpl->mtx);
              discImpl->connected = false;
              discImpl->chan = nullptr;
            }
            finishOperationForDisconnect(discImpl.get());

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
            resumeWriteOperation(ch.get(), event->tx_unstalled.status);
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
  struct ble_l2cap_chan *abandonedChan = nullptr;
  bool disconnectQueued = false;
  TickType_t disconnectStartedAt = 0;
};

static std::vector<std::shared_ptr<L2CAPClientCtx>> &clientContexts() {
  static std::vector<std::shared_ptr<L2CAPClientCtx>> contexts;
  return contexts;
}

// NimBLE retains the server callback argument for the host session, so the
// registry must keep each registered Impl alive even if the sketch drops its
// last public handle. Cleanup releases this ownership after host shutdown.
static std::vector<std::shared_ptr<BLEL2CAPServer::Impl>> &l2capServers() {
  static std::vector<std::shared_ptr<BLEL2CAPServer::Impl>> servers;
  return servers;
}

static struct ble_npl_callout sAbandonedDisconnectCallout;
static bool sAbandonedDisconnectCalloutInited = false;

static void forceAbandonedLinkCleanup(uint16_t connHandle) {
  int rc = ble_gap_terminate(connHandle, BLE_ERR_REM_USER_CONN_TERM);
  if (rc != 0 && rc != BLE_HS_ENOTCONN) {
    log_e("L2CAP: failed to terminate link holding orphan channel: rc=%d", rc);
  }
}

static bool scheduleAbandonedDisconnect() {
  BLELockGuard lock(sL2CAPMtx);
  if (sL2CAPStopping) {
    return false;
  }
  if (!sAbandonedDisconnectCalloutInited) {
    int initRc = ble_npl_callout_init(&sAbandonedDisconnectCallout, nimble_port_get_dflt_eventq(), [](struct ble_npl_event *) {
      std::vector<std::pair<std::shared_ptr<L2CAPClientCtx>, struct ble_l2cap_chan *>> pending;
      {
        BLELockGuard lock(sL2CAPMtx);
        for (auto &ctx : clientContexts()) {
          if (ctx->disconnectQueued && ctx->abandonedChan) {
            ctx->disconnectQueued = false;
            pending.emplace_back(ctx, ctx->abandonedChan);
          }
        }
      }
      bool retry = false;
      std::vector<std::shared_ptr<L2CAPClientCtx>> terminateLinks;
      for (auto &[ctx, chan] : pending) {
        int rc = ble_l2cap_disconnect(chan);
        if (rc != 0) {
          BLELockGuard lock(sL2CAPMtx);
          if (!sL2CAPStopping && xTaskGetTickCount() - ctx->disconnectStartedAt < pdMS_TO_TICKS(30000)) {
            ctx->disconnectQueued = true;
            retry = true;
          } else {
            log_e("L2CAP: deferred orphan disconnect failed: rc=%d", rc);
            terminateLinks.push_back(ctx);
          }
        }
      }
      for (auto &ctx : terminateLinks) {
        forceAbandonedLinkCleanup(ctx->impl->connHandle);
      }
      if (retry) {
        int resetRc = ble_npl_callout_reset(&sAbandonedDisconnectCallout, ble_npl_time_ms_to_ticks32(100));
        if (resetRc != 0) {
          log_e("L2CAP: orphan disconnect retry scheduling failed: rc=%d", resetRc);
          std::vector<std::shared_ptr<L2CAPClientCtx>> unscheduled;
          {
            BLELockGuard lock(sL2CAPMtx);
            for (auto &ctx : clientContexts()) {
              if (ctx->disconnectQueued) {
                unscheduled.push_back(ctx);
              }
            }
          }
          for (auto &ctx : unscheduled) {
            forceAbandonedLinkCleanup(ctx->impl->connHandle);
          }
        }
      }
    }, nullptr);
    if (initRc != 0) {
      log_e("L2CAP: orphan disconnect callout init failed: rc=%d", initRc);
      return false;
    }
    sAbandonedDisconnectCalloutInited = true;
  }
  // A one-tick deferral runs after COC_CONNECTED returns and NimBLE releases
  // its connect signaling procedure. Retries use a longer backoff below.
  int resetRc = ble_npl_callout_reset(&sAbandonedDisconnectCallout, 1);
  if (resetRc != 0) {
    log_e("L2CAP: orphan disconnect scheduling failed: rc=%d", resetRc);
    return false;
  }
  return true;
}

void nimbleStartL2CAPOperations() {
  BLELockGuard lock(sL2CAPMtx);
  sL2CAPStopping = false;
}

void nimblePrepareL2CAPShutdown() {
  BLELockGuard lock(sL2CAPMtx);
  sL2CAPStopping = true;
}

void nimbleResetL2CAPDeferredEvent() {
  BLELockGuard lock(sL2CAPMtx);
  auto trackedChannels = std::move(operationChannels());
  operationChannels().clear();
  for (auto &channel : trackedChannels) {
    channel->operationHostRunning = false;
    completeOperationLocked(channel.get(), BTStatus::InvalidState);
    deinitOperationEventLocked(channel.get());
  }

  if (sAbandonedDisconnectCalloutInited) {
    ble_npl_callout_stop(&sAbandonedDisconnectCallout);
    ble_npl_callout_deinit(&sAbandonedDisconnectCallout);
    sAbandonedDisconnectCalloutInited = false;
  }
}

BLEL2CAPServer BLEClass::createL2CAPServer(uint16_t psm, uint16_t mtu) {
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
    log_e("createL2CAPServer: BLE not initialized");
    return BLEL2CAPServer();
  }
  if (!sL2CAPMtx.init()) {
    log_e("createL2CAPServer: failed to allocate L2CAP mutex");
    return BLEL2CAPServer();
  }
  _impl->storeHook(_impl->l2capBeginHook, nimbleStartL2CAPOperations);
  _impl->storeHook(_impl->l2capPreEndHook, nimblePrepareL2CAPShutdown);
  _impl->storeHook(_impl->l2capHostStoppedHook, nimbleResetL2CAPDeferredEvent);
  _impl->storeHook(_impl->l2capPostEndHook, nimbleCleanupL2CAP);
  nimbleStartL2CAPOperations();
  auto impl = std::make_shared<BLEL2CAPServer::Impl>();
  if (!nimbleSetupL2CAPServer(impl, psm, mtu)) {
    return BLEL2CAPServer();
  }
  return BLEL2CAPServer(impl);
}

BLEL2CAPChannel BLEClass::connectL2CAP(uint16_t connHandle, uint16_t psm, uint16_t mtu) {
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
    log_e("connectL2CAP: BLE not initialized");
    return BLEL2CAPChannel();
  }
  if (!sL2CAPMtx.init()) {
    log_e("connectL2CAP: failed to allocate L2CAP mutex");
    return BLEL2CAPChannel();
  }
  _impl->storeHook(_impl->l2capBeginHook, nimbleStartL2CAPOperations);
  _impl->storeHook(_impl->l2capPreEndHook, nimblePrepareL2CAPShutdown);
  _impl->storeHook(_impl->l2capHostStoppedHook, nimbleResetL2CAPDeferredEvent);
  _impl->storeHook(_impl->l2capPostEndHook, nimbleCleanupL2CAP);
  nimbleStartL2CAPOperations();
  auto impl = std::make_shared<BLEL2CAPChannel::Impl>();
  if (!nimbleSetupL2CAPChannel(impl, connHandle, psm, mtu)) {
    return BLEL2CAPChannel();
  }
  return BLEL2CAPChannel(impl);
}

bool nimbleSetupL2CAPServer(const std::shared_ptr<BLEL2CAPServer::Impl> &server, uint16_t psm, uint16_t mtu) {
  {
    BLELockGuard lock(sL2CAPMtx);
    if (sL2CAPStopping) {
      return false;
    }
  }
  uint32_t alignedBufSize = OS_ALIGN(static_cast<uint32_t>(mtu) + 4U, OS_ALIGNMENT);
  if (alignedBufSize > UINT16_MAX) {
    log_e("L2CAP server MTU %u exceeds allocatable SDU size", mtu);
    return false;
  }
  server->psm = psm;
  server->mtu = mtu;

  const int numBufs = CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM + 1;
  const uint16_t bufSize = static_cast<uint16_t>(alignedBufSize);
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
    os_mempool_deinit(&server->sduMemPool);
    free(server->sduMem);
    server->sduMem = nullptr;
    return false;
  }

  rc = ble_l2cap_create_server(psm, mtu, BLEL2CAPServer::Impl::l2capEvent, server.get());
  if (rc != 0) {
    log_e("ble_l2cap_create_server failed: rc=%d", rc);
    os_mempool_deinit(&server->sduMemPool);
    free(server->sduMem);
    server->sduMem = nullptr;
    return false;
  }

  {
    BLELockGuard lock(sL2CAPMtx);
    l2capServers().push_back(server);
  }
  return true;
}

static void removeClientCtx(L2CAPClientCtx *ctx) {
  BLELockGuard lock(sL2CAPMtx);
  auto &contexts = clientContexts();
  for (auto it = contexts.begin(); it != contexts.end(); ++it) {
    if (it->get() == ctx) {
      contexts.erase(it);
      break;
    }
  }
}

void nimbleCleanupL2CAP() {
  std::vector<std::shared_ptr<L2CAPClientCtx>> pending;
  std::vector<std::shared_ptr<BLEL2CAPServer::Impl>> servers;
  {
    BLELockGuard lock(sL2CAPMtx);
    pending.swap(clientContexts());
    servers.swap(l2capServers());
  }
  for (auto &ctx : pending) {
    if (ctx->impl) {
      {
        BLELockGuard lock(ctx->impl->mtx);
        ctx->impl->connected = false;
        ctx->impl->chan = nullptr;
      }
      ctx->impl->connectSync.give(BTStatus::Fail);
      completeOperation(ctx->impl.get(), BTStatus::InvalidState);
    }
  }
  // Server-side channels: the host is stopped and operation events have been
  // removed, so invalidate every raw native pointer before the next begin().
  for (auto &server : servers) {
    BLELockGuard lock(server->mtx);
    for (auto &ch : server->channels) {
      {
        BLELockGuard channelLock(ch->mtx);
        ch->connected = false;
        ch->chan = nullptr;
      }
      ch->connectSync.give(BTStatus::Fail);
      completeOperation(ch.get(), BTStatus::InvalidState);
    }
    server->channels.clear();
    server->onAcceptCb = nullptr;
    server->onDataCb = nullptr;
    server->onDisconnectCb = nullptr;
    if (server->sduMem) {
      os_mempool_deinit(&server->sduMemPool);
      free(server->sduMem);
      server->sduMem = nullptr;
    }
    server->sduPool = {};
    server->sduMemPool = {};
  }
}

bool nimbleSetupL2CAPChannel(const std::shared_ptr<BLEL2CAPChannel::Impl> &chanImpl, uint16_t connHandle, uint16_t psm, uint16_t mtu) {
  {
    BLELockGuard lock(sL2CAPMtx);
    if (sL2CAPStopping) {
      return false;
    }
  }
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

  auto ctx = std::make_shared<L2CAPClientCtx>();
  ctx->impl = chanImpl;
  {
    BLELockGuard lock(sL2CAPMtx);
    clientContexts().push_back(ctx);
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
            BLELockGuard lock(sL2CAPMtx);
            abandoned = ctx->abandoned;
            BLELockGuard channelLock(impl->mtx);
            if (abandoned) {
              ctx->abandonedChan = event->connect.chan;
              ctx->disconnectQueued = true;
              ctx->disconnectStartedAt = xTaskGetTickCount();
              impl->connected = false;
              impl->chan = nullptr;
            } else {
              impl->chan = event->connect.chan;
              impl->connected = true;
              impl->connHandle = event->connect.conn_handle;
            }
          }
          if (abandoned) {
            // The user-side connect already timed out and returned an empty
            // handle. Defer disconnect until this callback returns because
            // NimBLE still owns its sole signaling procedure while invoking
            // COC_CONNECTED; an inline disconnect would fail with ENOMEM.
            log_w("L2CAP client connect completed after timeout; scheduling orphan disconnect");
            if (!scheduleAbandonedDisconnect()) {
              forceAbandonedLinkCleanup(impl->connHandle);
            }
            break;
          }
          impl->connectSync.give(BTStatus::OK);
        } else {
          log_w("L2CAP client connect failed: status=%d", event->connect.status);
          impl->connectSync.give(BTStatus::Fail);
          removeClientCtx(ctx);
        }
        break;

      case BLE_L2CAP_EVENT_COC_DISCONNECTED:
      {
        {
          BLELockGuard lock(sL2CAPMtx);
          ctx->abandonedChan = nullptr;
          ctx->disconnectQueued = false;
          BLELockGuard channelLock(impl->mtx);
          impl->connected = false;
          impl->chan = nullptr;
        }
        finishOperationForDisconnect(impl.get());
        BLEL2CAPChannel::DisconnectHandler cb;
        {
          BLELockGuard lock(impl->mtx);
          cb = impl->onDisconnectCb;
        }
        if (cb) {
          BLEL2CAPChannel channel = BLEL2CAPChannelImplCommon::makeHandle(impl);
          cb(channel);
        }
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

      case BLE_L2CAP_EVENT_COC_TX_UNSTALLED: resumeWriteOperation(impl.get(), event->tx_unstalled.status); break;
    }
    return 0;
  };

  bool wait = !nimbleIsHostTask();
  if (wait) {
    chanImpl->connectSync.take();
  }
  int rc = ble_l2cap_connect(connHandle, psm, mtu, sdu, clientEventFn, ctx.get());
  if (rc != 0) {
    if (wait) {
      chanImpl->connectSync.give(BTStatus::Fail);
    }
    log_e("ble_l2cap_connect failed: rc=%d", rc);
    removeClientCtx(ctx.get());
    return false;
  }

  if (wait) {
    BTStatus st = chanImpl->connectSync.wait(5000);
    if (st == BTStatus::Timeout) {
      // Do not delete ctx: the stack still holds it as the event arg and the
      // connect may still complete. Mark it abandoned so COC_CONNECTED
      // schedules the orphan channel for disconnect (and so
      // nimbleCleanupL2CAP can reclaim ctx at BLE.end()).
      bool scheduleDisconnect = false;
      {
        BLELockGuard lock(sL2CAPMtx);
        ctx->abandoned = true;
        BLELockGuard channelLock(chanImpl->mtx);
        if (chanImpl->connected && chanImpl->chan) {
          ctx->abandonedChan = chanImpl->chan;
          ctx->disconnectQueued = true;
          ctx->disconnectStartedAt = xTaskGetTickCount();
          scheduleDisconnect = true;
        }
        chanImpl->chan = nullptr;
        chanImpl->connected = false;
      }
      if (scheduleDisconnect) {
        if (!scheduleAbandonedDisconnect()) {
          forceAbandonedLinkCleanup(chanImpl->connHandle);
        }
      }
      log_e("L2CAP: connect timed out; channel will be torn down if it completes");
      return false;
    }
    if (st != BTStatus::OK) {
      return false;
    }
  }
  return true;
}

#endif /* BLE_L2CAP_SUPPORTED && BLE_NIMBLE */
