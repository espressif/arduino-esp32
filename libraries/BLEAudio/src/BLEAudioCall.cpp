/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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
 * @file BLEAudioCall.cpp
 * @brief Call Control Profile (`BLEAudioCallServer` / `BLEAudioCallController`)
 *        public handle implementations and feature-disabled factory stubs.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "BLEAudioCall.h"
#include "BLEAudioControlLink.h"
#include "esp32-hal-log.h"

static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Originate) == BLE_AUDIO_CCP_OP_ORIGINATE, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Accept) == BLE_AUDIO_CCP_OP_ACCEPT, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Terminate) == BLE_AUDIO_CCP_OP_TERMINATE, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Hold) == BLE_AUDIO_CCP_OP_HOLD, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Retrieve) == BLE_AUDIO_CCP_OP_RETRIEVE, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallState::Ended) == BLE_AUDIO_CCP_CALL_ENDED, "call state mismatch");

// ==========================================================================
// 1. Call Control Server (TBS / GTBS)
// ==========================================================================

struct BLEAudioCallServer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  String providerName = "ESP Phone";
  String uci = "un000";
  String uriSchemes = "tel";
  OriginateCallback onOriginateCb;
  TerminatedCallback onTerminatedCb;
  CallControlCallback onCallControlCb;

  BTStatus apply() {
    String provider;
    String localUci;
    String schemes;
    {
      BLELockGuard lock(mtx.get());
      provider = providerName;
      localUci = uci;
      schemes = uriSchemes;
    }
    return bleAudioStatus(bleAudioCcpSrvInit(provider.c_str(), localUci.c_str(), schemes.c_str()));
  }

  static BTStatus op(uint8_t o, uint8_t callIndex) {
    return bleAudioStatus(bleAudioCcpSrvCallOp(o, callIndex));
  }

  void handle(const ble_audio_evt_t &e) {
    if (!e.data) {
      return;
    }
    if (e.type == BLE_AUDIO_EVT_CCP_SRV_ORIGINATE) {
      const auto *o = static_cast<const ble_audio_ccp_originate_t *>(e.data);
      auto cb = bleAudioCopyCallback(*this, &Impl::onOriginateCb);
      if (cb) {
        *o->accept = cb(o->call_index, String(o->uri ? o->uri : ""));
      }
    } else if (e.type == BLE_AUDIO_EVT_CCP_SRV_CALL) {
      const auto *c = static_cast<const ble_audio_ccp_call_t *>(e.data);
      if (c->op == BLE_AUDIO_CCP_OP_TERMINATE) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onTerminatedCb)) {
          cb(c->call_index, c->reason);
        }
      } else if (auto cb = bleAudioCopyCallback(*this, &Impl::onCallControlCb)) {
        cb(static_cast<BLEAudioCallOperation>(c->op), c->call_index);
      }
    }
  }
};

BLEAudioCallServer::BLEAudioCallServer() = default;

BLEAudioCallServer::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioCallServer::setProviderName(const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio->started) {
    return BTStatus::InvalidState;
  }
  if (name.length() == 0) {
    log_e("CallServer: provider name must not be empty");
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.providerName = name;
  return BTStatus::OK;
}

BTStatus BLEAudioCallServer::setUci(const String &uci) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio->started) {
    return BTStatus::InvalidState;
  }
  if (uci.length() == 0) {
    log_e("CallServer: UCI must not be empty");
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.uci = uci;
  return BTStatus::OK;
}

BTStatus BLEAudioCallServer::setUriSchemes(const String &schemes) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio->started) {
    return BTStatus::InvalidState;
  }
  bool usable = false;
  for (size_t i = 0; i < schemes.length(); i++) {
    const char c = schemes.charAt(i);
    if (c != ',' && c != ' ' && c != '\t') {
      usable = true;
      break;
    }
  }
  if (!usable) {
    log_e("CallServer: URI scheme list must contain at least one scheme");
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.uriSchemes = schemes;
  return BTStatus::OK;
}

BTStatus BLEAudioCallServer::incomingCall(const String &from, uint8_t &callIndex, const String &friendlyName) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioCcpSrvIncoming(from.c_str(), friendlyName.length() ? friendlyName.c_str() : nullptr, &callIndex));
}

BTStatus BLEAudioCallServer::accept(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_CCP_OP_ACCEPT, callIndex);
}

BTStatus BLEAudioCallServer::hold(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_CCP_OP_HOLD, callIndex);
}

BTStatus BLEAudioCallServer::retrieve(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_CCP_OP_RETRIEVE, callIndex);
}

BTStatus BLEAudioCallServer::terminate(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_CCP_OP_TERMINATE, callIndex);
}

BTStatus BLEAudioCallServer::remoteAnswered(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_CCP_OP_REMOTE_ANSWER, callIndex);
}

BTStatus BLEAudioCallServer::remoteTerminated(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return Impl::op(BLE_AUDIO_CCP_OP_REMOTE_TERMINATE, callIndex);
}

BTStatus BLEAudioCallServer::updateProviderName(const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus st = bleAudioStatus(bleAudioCcpSrvSetProviderName(name.c_str()));
  if (st) {
    BLELockGuard lock(impl.mtx.get());
    impl.providerName = name;
  }
  return st;
}

void BLEAudioCallServer::onOriginate(OriginateCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onOriginateCb = std::move(cb);
}

void BLEAudioCallServer::onTerminated(TerminatedCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onTerminatedCb = std::move(cb);
}

void BLEAudioCallServer::onCallControl(CallControlCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onCallControlCb = std::move(cb);
}

void BLEAudioCallServer::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onOriginateCb = nullptr;
  impl.onTerminatedCb = nullptr;
  impl.onCallControlCb = nullptr;
}

#if BLE_AUDIO_CCP_SERVER_SUPPORTED
BLEAudioCallServer BLEAudio::createCallServer() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioCallServer();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioCallServer>(_impl, "createCallServer", BLE_AUDIO_GRP_CCP_SERVER, [](BLEAudioCallServer::Impl &p) {
    return p.apply();
  });
}
#endif

// ==========================================================================
// 2. Call Control Client
// ==========================================================================

struct BLEAudioCallController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  BLEAudioControlLink link;
  DiscoveredCallback onDiscoveredCb;
  ResultCallback onResultCb;
  CallStateCallback onCallStateCb;

  BTStatus op(uint8_t o, uint8_t callIndex) const {
    const uint16_t c = link.activeConn();
    return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioCcpCliCallOp(c, o, callIndex)) : BTStatus::InvalidState;
  }

  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioCcpCliDiscover);
      if (err) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(err), false);
        }
      }
      return;
    }
    if (e.conn_handle != link.getConn()) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_CCP_CLI_DISCOVERED:
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          const auto *d = static_cast<const ble_audio_ccp_discovered_t *>(e.data);
          cb(bleAudioStatus(e.err), d && d->gtbs);
        }
        break;
      case BLE_AUDIO_EVT_CCP_CLI_RESULT:
        if (e.data) {
          const auto *c = static_cast<const ble_audio_ccp_call_t *>(e.data);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onResultCb)) {
            cb(static_cast<BLEAudioCallOperation>(c->op), bleAudioStatus(e.err), c->call_index);
          }
        }
        break;
      case BLE_AUDIO_EVT_CCP_CLI_CALL:
        if (!e.err && e.data) {
          const auto *s = static_cast<const ble_audio_ccp_call_state_t *>(e.data);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onCallStateCb)) {
            cb(s->call_index, static_cast<BLEAudioCallState>(s->state));
          }
        }
        break;
      default: break;
    }
  }
};

BLEAudioCallController::BLEAudioCallController() = default;

BLEAudioCallController::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioCallController::discover(uint16_t connHandle) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.link.discover(*impl.audio, connHandle, bleAudioCcpCliDiscover);
}

uint16_t BLEAudioCallController::getConnHandle() const {
  BLE_CHECK_IMPL(BLE_AUDIO_CONN_NONE);
  return impl.link.getConn();
}

BTStatus BLEAudioCallController::originate(const String &uri) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioCcpCliOriginate(c, uri.c_str())) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallController::accept(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_CCP_OP_ACCEPT, callIndex);
}

BTStatus BLEAudioCallController::terminate(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_CCP_OP_TERMINATE, callIndex);
}

BTStatus BLEAudioCallController::hold(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_CCP_OP_HOLD, callIndex);
}

BTStatus BLEAudioCallController::retrieve(uint8_t callIndex) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.op(BLE_AUDIO_CCP_OP_RETRIEVE, callIndex);
}

BTStatus BLEAudioCallController::readCalls() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioCcpCliReadCalls(c)) : BTStatus::InvalidState;
}

void BLEAudioCallController::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
}

void BLEAudioCallController::onResult(ResultCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onResultCb = std::move(cb);
}

void BLEAudioCallController::onCallState(CallStateCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onCallStateCb = std::move(cb);
}

void BLEAudioCallController::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
  impl.onResultCb = nullptr;
  impl.onCallStateCb = nullptr;
}

#if BLE_AUDIO_CCP_CLIENT_SUPPORTED
BLEAudioCallController BLEAudio::createCallController() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioCallController();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioCallController>(
    _impl, "createCallController", BLE_AUDIO_GRP_CCP_CLIENT,
    [](BLEAudioCallController::Impl &) {
      return bleAudioStatus(bleAudioCcpCliInit());
    }
  );
}
#endif

// ==========================================================================
// 3. Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_CCP_SERVER_SUPPORTED
BLEAudioCallServer BLEAudio::createCallServer() {
  log_e("Call server is not enabled in this build (CONFIG_BT_CCP_CALL_CONTROL_SERVER)");
  return BLEAudioCallServer();
}
#endif

#if !BLE_AUDIO_CCP_CLIENT_SUPPORTED
BLEAudioCallController BLEAudio::createCallController() {
  log_e("Call controller is not enabled in this build (CONFIG_BT_CCP_CALL_CONTROL_CLIENT)");
  return BLEAudioCallController();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
