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

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioCall.h"
#include "audio/BLEAudioControlLink.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioCall.cpp
 * @brief CCP call server (GTBS bearer) and call controller role handles.
 *
 * Both roles are thin wrappers over the control engine unit
 * (BLEAudioEngineControl): the factory registers an event handler for the
 * role's group and a deferred init that runs inside BLEAudio::start(). The
 * public enums share their values with the engine's, checked below, so they
 * are cast straight through.
 *
 * API contract is documented on the declarations in `BLEAudioCall.h`; the
 * definitions below carry implementation notes only.
 */

static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Originate) == BLE_AUDIO_CCP_OP_ORIGINATE, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Accept) == BLE_AUDIO_CCP_OP_ACCEPT, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Terminate) == BLE_AUDIO_CCP_OP_TERMINATE, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Hold) == BLE_AUDIO_CCP_OP_HOLD, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallOperation::Retrieve) == BLE_AUDIO_CCP_OP_RETRIEVE, "call op mismatch");
static_assert(static_cast<uint8_t>(BLEAudioCallState::Ended) == BLE_AUDIO_CCP_CALL_ENDED, "call state mismatch");

// --------------------------------------------------------------------------
// Server
// --------------------------------------------------------------------------

struct BLEAudioCallServer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  String providerName = "ESP Phone";  ///< Bearer Provider Name.
  String uci = "un000";               ///< Bearer UCI ("un000" = unknown).
  String uriSchemes = "tel";          ///< Comma-separated supported URI schemes.
  OriginateCallback originateCb;
  TerminatedCallback terminatedCb;
  CallControlCallback callControlCb;

  /** Apply a BLE_AUDIO_CCP_OP_* to a local call. */
  static BTStatus op(uint8_t o, uint8_t callIndex) {
    return bleAudioStatus(bleAudioCcpSrvCallOp(o, callIndex));
  }

  /**
   * Engine event for this group. ORIGINATE is dispatched synchronously: the
   * callback's return value is written back so the stack accepts or rejects
   * the call (accepted when no callback is set).
   */
  void handle(const ble_audio_evt_t &e) {
    if (!e.data) {
      return;
    }
    if (e.type == BLE_AUDIO_EVT_CCP_SRV_ORIGINATE) {
      const auto *o = static_cast<const ble_audio_ccp_originate_t *>(e.data);
      if (originateCb) {
        *o->accept = originateCb(o->call_index, String(o->uri ? o->uri : ""));
      }
    } else if (e.type == BLE_AUDIO_EVT_CCP_SRV_CALL) {
      const auto *c = static_cast<const ble_audio_ccp_call_t *>(e.data);
      if (c->op == BLE_AUDIO_CCP_OP_TERMINATE) {
        if (terminatedCb) {
          terminatedCb(c->call_index, c->reason);
        }
      } else if (callControlCb) {
        callControlCb(static_cast<BLEAudioCallOperation>(c->op), c->call_index);
      }
    }
  }
};

BLEAudioCallServer::BLEAudioCallServer() = default;

BLEAudioCallServer::operator bool() const {
  return _impl != nullptr;
}

BLEAudioCallServer &BLEAudioCallServer::setProviderName(const String &name) {
  if (_impl) {
    _impl->providerName = name;
  }
  return *this;
}

BLEAudioCallServer &BLEAudioCallServer::setUci(const String &uci) {
  if (_impl) {
    _impl->uci = uci;
  }
  return *this;
}

BLEAudioCallServer &BLEAudioCallServer::setUriSchemes(const String &schemes) {
  if (_impl) {
    _impl->uriSchemes = schemes;
  }
  return *this;
}

BTStatus BLEAudioCallServer::incomingCall(const String &from, uint8_t &callIndex) {
  return _impl ? bleAudioStatus(bleAudioCcpSrvIncoming(from.c_str(), nullptr, &callIndex)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallServer::accept(uint8_t callIndex) {
  return _impl ? Impl::op(BLE_AUDIO_CCP_OP_ACCEPT, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallServer::hold(uint8_t callIndex) {
  return _impl ? Impl::op(BLE_AUDIO_CCP_OP_HOLD, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallServer::retrieve(uint8_t callIndex) {
  return _impl ? Impl::op(BLE_AUDIO_CCP_OP_RETRIEVE, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallServer::terminate(uint8_t callIndex) {
  return _impl ? Impl::op(BLE_AUDIO_CCP_OP_TERMINATE, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallServer::remoteAnswered(uint8_t callIndex) {
  return _impl ? Impl::op(BLE_AUDIO_CCP_OP_REMOTE_ANSWER, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallServer::remoteTerminated(uint8_t callIndex) {
  return _impl ? Impl::op(BLE_AUDIO_CCP_OP_REMOTE_TERMINATE, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallServer::updateProviderName(const String &name) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  BTStatus st = bleAudioStatus(bleAudioCcpSrvSetProviderName(name.c_str()));
  if (st) {
    _impl->providerName = name;
  }
  return st;
}

BLEAudioCallServer &BLEAudioCallServer::onOriginate(OriginateCallback cb) {
  if (_impl) {
    _impl->originateCb = std::move(cb);
  }
  return *this;
}

BLEAudioCallServer &BLEAudioCallServer::onTerminated(TerminatedCallback cb) {
  if (_impl) {
    _impl->terminatedCb = std::move(cb);
  }
  return *this;
}

BLEAudioCallServer &BLEAudioCallServer::onCallControl(CallControlCallback cb) {
  if (_impl) {
    _impl->callControlCb = std::move(cb);
  }
  return *this;
}

void BLEAudioCallServer::resetCallbacks() {
  if (_impl) {
    _impl->originateCb = nullptr;
    _impl->terminatedCb = nullptr;
    _impl->callControlCb = nullptr;
  }
}

BLEAudioCallServer BLEAudio::createCallServer() {
#if BLE_AUDIO_CCP_SERVER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createCallServer(): call between audio.begin() and audio.start()");
    return BLEAudioCallServer();
  }
  auto s = std::make_shared<BLEAudioCallServer::Impl>();
  s->audio = _impl;
  _impl->roles.push_back(s);
  /* `roles` owns the role until end(); strong captures here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioCallServer::Impl> weak = s;
  _impl->setHandler(BLE_AUDIO_GRP_CCP_SERVER, [weak](const ble_audio_evt_t &e) {
    if (auto p = weak.lock()) {
      p->handle(e);
    }
  });
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto p = weak.lock();
    return p ? bleAudioStatus(bleAudioCcpSrvInit(p->providerName.c_str(), p->uci.c_str(), p->uriSchemes.c_str())) : BTStatus::OK;
  });
  return BLEAudioCallServer(s);
#else
  log_e("Call server is not enabled in this build (CONFIG_BT_CCP_CALL_CONTROL_SERVER)");
  return BLEAudioCallServer();
#endif
}

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioCallController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLEAudioControlLink link;  ///< The call server (phone) being controlled.
  DiscoveredCallback discoveredCb;
  ResultCallback resultCb;
  CallStateCallback callStateCb;

  /** Run a call operation on the bound peer's GTBS. */
  BTStatus op(uint8_t o, uint8_t callIndex) const {
    return link.bound() ? bleAudioStatus(bleAudioCcpCliCallOp(link.conn, o, callIndex)) : BTStatus::InvalidState;
  }

  /**
   * Engine event: core events drive the link (deferred discovery, disconnect);
   * role events are delivered only for the bound peer.
   */
  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioCcpCliDiscover);
      if (err && discoveredCb) {
        discoveredCb(bleAudioStatus(err), false);
      }
      return;
    }
    if (e.conn_handle != link.conn) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_CCP_CLI_DISCOVERED:
        if (discoveredCb) {
          const auto *d = static_cast<const ble_audio_ccp_discovered_t *>(e.data);
          discoveredCb(bleAudioStatus(e.err), d && d->gtbs);
        }
        break;
      case BLE_AUDIO_EVT_CCP_CLI_RESULT:
        if (e.data && resultCb) {
          const auto *c = static_cast<const ble_audio_ccp_call_t *>(e.data);
          resultCb(static_cast<BLEAudioCallOperation>(c->op), bleAudioStatus(e.err), c->call_index);
        }
        break;
      case BLE_AUDIO_EVT_CCP_CLI_CALL:
        if (!e.err && e.data && callStateCb) {
          const auto *s = static_cast<const ble_audio_ccp_call_state_t *>(e.data);
          callStateCb(s->call_index, static_cast<BLEAudioCallState>(s->state));
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
  return _impl ? _impl->link.discover(*_impl->audio, connHandle, bleAudioCcpCliDiscover) : BTStatus::InvalidState;
}

uint16_t BLEAudioCallController::getConnHandle() const {
  return _impl ? _impl->link.conn : BLE_AUDIO_CONN_NONE;
}

BTStatus BLEAudioCallController::originate(const String &uri) {
  if (!_impl || !_impl->link.bound()) {
    return BTStatus::InvalidState;
  }
  return bleAudioStatus(bleAudioCcpCliOriginate(_impl->link.conn, uri.c_str()));
}

BTStatus BLEAudioCallController::accept(uint8_t callIndex) {
  return _impl ? _impl->op(BLE_AUDIO_CCP_OP_ACCEPT, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallController::terminate(uint8_t callIndex) {
  return _impl ? _impl->op(BLE_AUDIO_CCP_OP_TERMINATE, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallController::hold(uint8_t callIndex) {
  return _impl ? _impl->op(BLE_AUDIO_CCP_OP_HOLD, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallController::retrieve(uint8_t callIndex) {
  return _impl ? _impl->op(BLE_AUDIO_CCP_OP_RETRIEVE, callIndex) : BTStatus::InvalidState;
}

BTStatus BLEAudioCallController::readCalls() {
  if (!_impl || !_impl->link.bound()) {
    return BTStatus::InvalidState;
  }
  return bleAudioStatus(bleAudioCcpCliReadCalls(_impl->link.conn));
}

BLEAudioCallController &BLEAudioCallController::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioCallController &BLEAudioCallController::onResult(ResultCallback cb) {
  if (_impl) {
    _impl->resultCb = std::move(cb);
  }
  return *this;
}

BLEAudioCallController &BLEAudioCallController::onCallState(CallStateCallback cb) {
  if (_impl) {
    _impl->callStateCb = std::move(cb);
  }
  return *this;
}

void BLEAudioCallController::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->resultCb = nullptr;
    _impl->callStateCb = nullptr;
  }
}

BLEAudioCallController BLEAudio::createCallController() {
#if BLE_AUDIO_CCP_CLIENT_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createCallController(): call between audio.begin() and audio.start()");
    return BLEAudioCallController();
  }
  auto c = std::make_shared<BLEAudioCallController::Impl>();
  c->audio = _impl;
  _impl->roles.push_back(c);
  /* `roles` owns the role until end(); a strong capture here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioCallController::Impl> weak = c;
  _impl->setHandler(BLE_AUDIO_GRP_CCP_CLIENT, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioCcpCliInit());
  });
  return BLEAudioCallController(c);
#else
  log_e("Call controller is not enabled in this build (CONFIG_BT_CCP_CALL_CONTROL_CLIENT)");
  return BLEAudioCallController();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
