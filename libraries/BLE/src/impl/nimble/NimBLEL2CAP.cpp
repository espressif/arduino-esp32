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
 * @file NimBLEL2CAP.cpp
 * @brief NimBLE Connection-Oriented Channels (CoC) L2CAP: server, client connect, and channel I/O.
 */

#include "impl/BLEGuards.h"
#if BLE_L2CAP_SUPPORTED && BLE_NIMBLE

#include "NimBLEL2CAP.h"
#include "BLE.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// BLEL2CAPChannel
// --------------------------------------------------------------------------

/**
 * @brief Constructs an empty L2CAP channel handle (not connected).
 */
BLEL2CAPChannel::BLEL2CAPChannel() : _impl(nullptr) {}

/**
 * @brief Tests whether the channel object holds a valid implementation pointer.
 * @return True if bound to an implementation; false otherwise.
 */
BLEL2CAPChannel::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Queues an SDU for transmission on the L2CAP channel.
 * @param data Pointer to the payload to send.
 * @param len Byte length of the payload.
 * @return @c OK if queued or stalled with flow control; @c NoMemory or @c Fail on hard errors.
 */
BTStatus BLEL2CAPChannel::write(const uint8_t *data, size_t len) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected || !impl.chan) {
    return BTStatus::InvalidState;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
  if (!om) {
    log_e("Failed to allocate mbuf for L2CAP write");
    return BTStatus::NoMemory;
  }

  int rc = ble_l2cap_send(impl.chan, om);
  if (rc == BLE_HS_ESTALLED) {
    // Flow control stall — data is queued and will be sent when un-stalled
    return BTStatus::OK;
  }
  if (rc != 0) {
    log_e("ble_l2cap_send failed: rc=%d", rc);
    os_mbuf_free_chain(om);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Closes the underlying NimBLE L2CAP channel.
 * @return @c OK on success, or @c InvalidState / @c Fail when not connected.
 */
BTStatus BLEL2CAPChannel::disconnect() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.connected || !impl.chan) {
    log_w("L2CAP: disconnect called but not connected");
    return BTStatus::InvalidState;
  }

  int rc = ble_l2cap_disconnect(impl.chan);
  if (rc != 0) {
    log_e("L2CAP: ble_l2cap_disconnect rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Registers a callback for received CoC data on this channel.
 * @param handler User handler; replaces any previous data handler.
 * @return @c OK if the implementation is valid, otherwise @c InvalidState.
 */
BTStatus BLEL2CAPChannel::onData(DataHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDataCb = handler;
  return BTStatus::OK;
}

/**
 * @brief Registers a callback for channel disconnect events.
 * @param handler User handler; replaces any previous disconnect handler.
 * @return @c OK if the implementation is valid, otherwise @c InvalidState.
 */
BTStatus BLEL2CAPChannel::onDisconnect(DisconnectHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = handler;
  return BTStatus::OK;
}

/**
 * @brief Clears the data and disconnect handlers on the channel.
 * @return @c OK if the implementation is valid, otherwise @c InvalidState.
 */
BTStatus BLEL2CAPChannel::resetCallbacks() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDataCb = nullptr;
  impl.onDisconnectCb = nullptr;
  return BTStatus::OK;
}

/**
 * @brief Reports whether the channel is connected.
 * @return True when the implementation is present and connected.
 */
bool BLEL2CAPChannel::isConnected() const {
  return _impl && _impl->connected;
}

/**
 * @brief Returns the PSM the channel is using.
 * @return Protocol/Service Multiplexer value, or 0 if no implementation.
 */
uint16_t BLEL2CAPChannel::getPSM() const {
  return _impl ? _impl->psm : 0;
}

/**
 * @brief Returns the GAP connection handle for the underlying link.
 * @return Connection handle, or @c BLE_HS_CONN_HANDLE_NONE if unknown.
 */
uint16_t BLEL2CAPChannel::getConnHandle() const {
  return _impl ? _impl->connHandle : BLE_HS_CONN_HANDLE_NONE;
}

/**
 * @brief Returns the negotiated/observed L2CAP MTU for the channel.
 * @return Our MTU in bytes, or 0 if the channel is not available.
 */
uint16_t BLEL2CAPChannel::getMTU() const {
  if (!_impl || !_impl->chan) {
    return 0;
  }
  return ble_l2cap_get_our_mtu(_impl->chan);
}

// --------------------------------------------------------------------------
// BLEL2CAPServer
// --------------------------------------------------------------------------

/**
 * @brief Constructs an empty L2CAP server object (not registered with the stack).
 */
BLEL2CAPServer::BLEL2CAPServer() : _impl(nullptr) {}

/**
 * @brief Tests whether the server object is bound to an implementation.
 * @return True if an implementation exists; false otherwise.
 */
BLEL2CAPServer::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Registers a callback for accepted incoming L2CAP channels.
 * @param handler Invoked for each new channel after connect succeeds.
 * @return @c OK if the implementation is valid, otherwise @c InvalidState.
 */
BTStatus BLEL2CAPServer::onAccept(AcceptHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onAcceptCb = handler;
  return BTStatus::OK;
}

/**
 * @brief Sets a default data handler for channels accepted from this server (inherited on connect).
 * @param handler Receives data for new channels; per-channel override may be set on the client side after accept.
 * @return @c OK if the implementation is valid, otherwise @c InvalidState.
 */
BTStatus BLEL2CAPServer::onData(DataHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDataCb = handler;
  return BTStatus::OK;
}

/**
 * @brief Sets a default disconnect handler for channels from this server.
 * @param handler User callback; inherited by new channels.
 * @return @c OK if the implementation is valid, otherwise @c InvalidState.
 */
BTStatus BLEL2CAPServer::onDisconnect(DisconnectHandler handler) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onDisconnectCb = handler;
  return BTStatus::OK;
}

/**
 * @brief Clears the accept, data, and disconnect server-level callbacks.
 * @return @c OK if the implementation is valid, otherwise @c InvalidState.
 */
BTStatus BLEL2CAPServer::resetCallbacks() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx);
  impl.onAcceptCb = nullptr;
  impl.onDataCb = nullptr;
  impl.onDisconnectCb = nullptr;
  return BTStatus::OK;
}

/**
 * @brief Returns the PSM registered for this L2CAP server.
 * @return PSM, or 0 if no server implementation.
 */
uint16_t BLEL2CAPServer::getPSM() const {
  return _impl ? _impl->psm : 0;
}

/**
 * @brief Returns the configured CoC/SDU size for the server.
 * @return Configured MTU, or 0 if no server implementation.
 */
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
        cb(BLEL2CAPChannel(chanImpl));
      }
      break;
    }

    case BLE_L2CAP_EVENT_COC_DISCONNECTED:
    {
      BLEL2CAPChannel::DisconnectHandler cb;
      BLEL2CAPChannel handle;
      {
        BLELockGuard lock(server->mtx);
        for (auto it = server->channels.begin(); it != server->channels.end(); ++it) {
          if ((*it)->chan == event->disconnect.chan) {
            (*it)->connected = false;
            (*it)->chan = nullptr;

            cb = (*it)->onDisconnectCb;
            handle = BLEL2CAPChannel(*it);
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

      uint16_t dataLen = OS_MBUF_PKTLEN(rxBuf);
      std::vector<uint8_t> data(dataLen);
      os_mbuf_copydata(rxBuf, 0, dataLen, data.data());

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
        cb(BLEL2CAPChannel(chanImpl), data.data(), data.size());
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
      // Could signal a waiting writer; for now just log
      log_d("L2CAP TX un-stalled");
      break;
  }

  return 0;
}

// --------------------------------------------------------------------------
// BLEClass factory methods -- L2CAP
// --------------------------------------------------------------------------

/**
 * @brief Creates and registers an L2CAP CoC server with a dedicated SDU pool and callbacks.
 * @param psm Protocol/Service Multiplexer to listen on.
 * @param mtu Configured local SDU/MTU size.
 * @return A server handle, or an empty @c BLEL2CAPServer on memory or stack errors.
 */
BLEL2CAPServer BLEClass::createL2CAPServer(uint16_t psm, uint16_t mtu) {
  if (!_initialized) {
    return BLEL2CAPServer();
  }

  auto server = std::make_shared<BLEL2CAPServer::Impl>();
  server->psm = psm;
  server->mtu = mtu;

  // Allocate SDU memory pool
  // Each buffer = mtu + BLE L2CAP SDU header. Allocate enough for a few concurrent channels.
  const int numBufs = CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM + 1;
  const uint16_t bufSize = OS_ALIGN(mtu + 4, OS_ALIGNMENT);  // +4 for L2CAP header overhead
  const size_t memSize = OS_MEMPOOL_SIZE(numBufs, bufSize) * sizeof(os_membuf_t);

  server->sduMem = static_cast<uint8_t *>(malloc(memSize));
  if (!server->sduMem) {
    log_e("Failed to allocate L2CAP SDU memory pool");
    return BLEL2CAPServer();
  }

  int rc = os_mempool_init(&server->sduMemPool, numBufs, bufSize, server->sduMem, "l2cap_sdu");
  if (rc != 0) {
    log_e("os_mempool_init failed: rc=%d", rc);
    free(server->sduMem);
    server->sduMem = nullptr;
    return BLEL2CAPServer();
  }

  rc = os_mbuf_pool_init(&server->sduPool, &server->sduMemPool, bufSize, numBufs);
  if (rc != 0) {
    log_e("os_mbuf_pool_init failed: rc=%d", rc);
    free(server->sduMem);
    server->sduMem = nullptr;
    return BLEL2CAPServer();
  }

  rc = ble_l2cap_create_server(psm, mtu, BLEL2CAPServer::Impl::l2capEvent, server.get());
  if (rc != 0) {
    log_e("ble_l2cap_create_server failed: rc=%d", rc);
    free(server->sduMem);
    server->sduMem = nullptr;
    return BLEL2CAPServer();
  }

  return BLEL2CAPServer(server);
}

/**
 * @brief Initiates a client L2CAP CoC connection to a PSM on an active ACL link.
 * @param connHandle GAP connection handle to the peer.
 * @param psm Target PSM on the peer.
 * @param mtu Requested/used SDU size.
 * @return A channel object; may be non-connected until @c COC_CONNECTED fires asynchronously.
 */
BLEL2CAPChannel BLEClass::connectL2CAP(uint16_t connHandle, uint16_t psm, uint16_t mtu) {
  if (!_initialized) {
    return BLEL2CAPChannel();
  }

  auto chanImpl = std::make_shared<BLEL2CAPChannel::Impl>();
  chanImpl->psm = psm;
  chanImpl->connHandle = connHandle;

  // Allocate an SDU buffer for receive
  struct os_mbuf *sdu = ble_hs_mbuf_from_flat(nullptr, 0);
  if (!sdu) {
    sdu = os_msys_get_pkthdr(mtu, 0);
  }
  if (!sdu) {
    log_e("Failed to allocate SDU for L2CAP connect");
    return BLEL2CAPChannel();
  }

  // Static event handler for client-side L2CAP channel
  struct ClientCtx {
    std::shared_ptr<BLEL2CAPChannel::Impl> impl;
  };

  // We use a persistent allocation for the callback context
  auto *ctx = new ClientCtx{chanImpl};

  /**
   * @brief Client-side L2CAP CoC event handler: connect, disconnect, receive, and TX-unstall.
   * @param event NimBLE L2CAP event.
   * @param arg Opaque @c ClientCtx (owns shared channel impl; deleted on terminal events).
   * @return Zero (NimBLE callback convention).
   */
  auto clientEventFn = [](struct ble_l2cap_event *event, void *arg) -> int {
    auto *ctx = static_cast<ClientCtx *>(arg);
    if (!ctx) {
      return 0;
    }
    auto &impl = ctx->impl;

    switch (event->type) {
      case BLE_L2CAP_EVENT_COC_CONNECTED:
        if (event->connect.status == 0) {
          impl->chan = event->connect.chan;
          impl->connected = true;
          impl->connHandle = event->connect.conn_handle;
        } else {
          log_w("L2CAP client connect failed: status=%d", event->connect.status);
          delete ctx;
        }
        break;

      case BLE_L2CAP_EVENT_COC_DISCONNECTED:
      {
        impl->connected = false;
        impl->chan = nullptr;
        BLEL2CAPChannel::DisconnectHandler cb;
        {
          BLELockGuard lock(impl->mtx);
          cb = impl->onDisconnectCb;
        }
        if (cb) {
          cb(BLEL2CAPChannel(impl));
        }
        delete ctx;
        break;
      }

      case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
      {
        struct os_mbuf *rxBuf = event->receive.sdu_rx;
        if (!rxBuf) {
          break;
        }
        uint16_t dataLen = OS_MBUF_PKTLEN(rxBuf);
        std::vector<uint8_t> data(dataLen);
        os_mbuf_copydata(rxBuf, 0, dataLen, data.data());

        // Re-provide receive buffer
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
          cb(BLEL2CAPChannel(impl), data.data(), data.size());
        }
        break;
      }

      case BLE_L2CAP_EVENT_COC_TX_UNSTALLED: break;
    }
    return 0;
  };

  int rc = ble_l2cap_connect(connHandle, psm, mtu, sdu, clientEventFn, ctx);
  if (rc != 0) {
    log_e("ble_l2cap_connect failed: rc=%d", rc);
    delete ctx;
    return BLEL2CAPChannel();
  }

  chanImpl->mtu = mtu;
  return BLEL2CAPChannel(chanImpl);
}

#endif /* BLE_L2CAP_SUPPORTED && BLE_NIMBLE */
