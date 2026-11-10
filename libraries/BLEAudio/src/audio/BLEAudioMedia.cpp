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

#include "audio/BLEAudioMedia.h"
#include "audio/BLEAudioControlLink.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioMedia.cpp
 * @brief MCP media player (local player behind MCS/GMCS) and media controller role handles.
 *
 * Both roles are thin wrappers over the control engine unit
 * (BLEAudioEngineControl): the factory registers an event handler for the
 * role's group and a deferred init that runs inside BLEAudio::start(). The
 * public media state and command enums share their values with MCS.
 *
 * API contract is documented on the declarations in `BLEAudioMedia.h`; the
 * definitions below carry implementation notes only.
 */

namespace {

/** Status of a media command: the transport error first, then the player's MCS result code. */
BTStatus commandStatus(int err, uint8_t result) {
  if (err) {
    return bleAudioStatus(err);
  }
  return result == BLE_AUDIO_MCP_CMD_SUCCESS ? BTStatus::OK : BTStatus::Fail;
}

}  // namespace

// --------------------------------------------------------------------------
// Player
// --------------------------------------------------------------------------

struct BLEAudioMediaPlayer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  String playerName;  ///< Kept so it can be applied at start.
  String trackTitle;  ///< Kept so it can be applied at start.
  BLEAudioMediaState state = BLEAudioMediaState::Inactive;  ///< Last state reported by the proxy.
  StateCallback stateCb;
  CommandCallback commandCb;

  /** Store a name/title; pushed to the proxy now when started, else by apply(). */
  BTStatus setText(bool title, const String &text) {
    (title ? trackTitle : playerName) = text;
    return audio->started ? bleAudioStatus(bleAudioMcpSrvSetText(title, text.c_str())) : BTStatus::OK;
  }

  /** Deferred init run by BLEAudio::start(): register the player, then apply the stored texts (best effort). */
  BTStatus apply() {
    int err = bleAudioMcpSrvInit();
    if (err != 0) {
      return bleAudioStatus(err);
    }
    if (playerName.length() && bleAudioMcpSrvSetText(false, playerName.c_str()) != 0) {
      log_w("Media: player name \"%s\" rejected by the media proxy", playerName.c_str());
    }
    if (trackTitle.length() && bleAudioMcpSrvSetText(true, trackTitle.c_str()) != 0) {
      log_w("Media: track title \"%s\" rejected by the media proxy", trackTitle.c_str());
    }
    return BTStatus::OK;
  }

  /** Engine event for this group (reported only with local player control; see the engine). */
  void handle(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_MCP_SRV_STATE && !e.err && e.data) {
      state = static_cast<BLEAudioMediaState>(*static_cast<const uint8_t *>(e.data));
      if (stateCb) {
        stateCb(state);
      }
    } else if (e.type == BLE_AUDIO_EVT_MCP_SRV_RESULT && e.data && commandCb) {
      const auto *r = static_cast<const ble_audio_mcp_result_t *>(e.data);
      commandCb(static_cast<BLEAudioMediaCommand>(r->opcode), commandStatus(e.err, r->result));
    }
  }
};

BLEAudioMediaPlayer::BLEAudioMediaPlayer() = default;

BLEAudioMediaPlayer::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioMediaPlayer::setPlayerName(const String &name) {
  return _impl ? _impl->setText(false, name) : BTStatus::InvalidState;
}

BTStatus BLEAudioMediaPlayer::setTrackTitle(const String &title) {
  return _impl ? _impl->setText(true, title) : BTStatus::InvalidState;
}

BTStatus BLEAudioMediaPlayer::sendCommand(BLEAudioMediaCommand command) {
  return _impl ? bleAudioStatus(bleAudioMcpSrvCommand(static_cast<uint8_t>(command))) : BTStatus::InvalidState;
}

BTStatus BLEAudioMediaPlayer::play() {
  return sendCommand(BLEAudioMediaCommand::Play);
}

BTStatus BLEAudioMediaPlayer::pause() {
  return sendCommand(BLEAudioMediaCommand::Pause);
}

BTStatus BLEAudioMediaPlayer::stop() {
  return sendCommand(BLEAudioMediaCommand::Stop);
}

BTStatus BLEAudioMediaPlayer::nextTrack() {
  return sendCommand(BLEAudioMediaCommand::NextTrack);
}

BTStatus BLEAudioMediaPlayer::previousTrack() {
  return sendCommand(BLEAudioMediaCommand::PreviousTrack);
}

BLEAudioMediaState BLEAudioMediaPlayer::getState() const {
  return _impl ? _impl->state : BLEAudioMediaState::Inactive;
}

BLEAudioMediaPlayer &BLEAudioMediaPlayer::onStateChanged(StateCallback cb) {
  if (_impl) {
    _impl->stateCb = std::move(cb);
  }
  return *this;
}

BLEAudioMediaPlayer &BLEAudioMediaPlayer::onCommandResult(CommandCallback cb) {
  if (_impl) {
    _impl->commandCb = std::move(cb);
  }
  return *this;
}

void BLEAudioMediaPlayer::resetCallbacks() {
  if (_impl) {
    _impl->stateCb = nullptr;
    _impl->commandCb = nullptr;
  }
}

BLEAudioMediaPlayer BLEAudio::createMediaPlayer() {
#if BLE_AUDIO_MCP_SERVER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createMediaPlayer(): call between audio.begin() and audio.start()");
    return BLEAudioMediaPlayer();
  }
  auto p = std::make_shared<BLEAudioMediaPlayer::Impl>();
  p->audio = _impl;
  _impl->roles.push_back(p);
  /* `roles` owns the role until end(); strong captures here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioMediaPlayer::Impl> weak = p;
  _impl->setHandler(BLE_AUDIO_GRP_MCP_SERVER, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([weak]() -> BTStatus {
    auto s = weak.lock();
    return s ? s->apply() : BTStatus::OK;
  });
  return BLEAudioMediaPlayer(p);
#else
  log_e("Media player is not enabled in this build (CONFIG_BT_MCS)");
  return BLEAudioMediaPlayer();
#endif
}

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioMediaController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLEAudioControlLink link;  ///< The media player being controlled.
  BLEAudioMediaState state = BLEAudioMediaState::Inactive;  ///< Last state reported by the peer.
  DiscoveredCallback discoveredCb;
  StateCallback stateCb;
  CommandCallback commandCb;
  TextCallback playerNameCb;
  TextCallback trackTitleCb;

  /** Read one characteristic (BLE_AUDIO_MCP_READ_*) from the bound peer. */
  BTStatus read(uint8_t what) const {
    return link.bound() ? bleAudioStatus(bleAudioMcpCliRead(link.conn, what)) : BTStatus::InvalidState;
  }

  /**
   * Engine event: core events drive the link (deferred discovery, disconnect);
   * role events are delivered only for the bound peer.
   */
  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      const int err = link.onCore(e, bleAudioMcpCliDiscover);
      if (err && discoveredCb) {
        discoveredCb(bleAudioStatus(err));
      }
      return;
    }
    if (e.conn_handle != link.conn) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_MCP_CLI_DISCOVERED:
        if (discoveredCb) {
          discoveredCb(bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_MCP_CLI_STATE:
        if (!e.err && e.data) {
          state = static_cast<BLEAudioMediaState>(*static_cast<const uint8_t *>(e.data));
          if (stateCb) {
            stateCb(state);
          }
        }
        break;
      case BLE_AUDIO_EVT_MCP_CLI_RESULT:
        if (e.data && commandCb) {
          const auto *r = static_cast<const ble_audio_mcp_result_t *>(e.data);
          commandCb(static_cast<BLEAudioMediaCommand>(r->opcode), commandStatus(e.err, r->result));
        }
        break;
      case BLE_AUDIO_EVT_MCP_CLI_PLAYER_NAME:
      case BLE_AUDIO_EVT_MCP_CLI_TRACK_TITLE: {
        const TextCallback &cb = e.type == BLE_AUDIO_EVT_MCP_CLI_PLAYER_NAME ? playerNameCb : trackTitleCb;
        if (!e.err && e.data && cb) {
          cb(String(static_cast<const char *>(e.data)));
        }
        break;
      }
      default: break;
    }
  }
};

BLEAudioMediaController::BLEAudioMediaController() = default;

BLEAudioMediaController::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioMediaController::discover(uint16_t connHandle) {
  return _impl ? _impl->link.discover(*_impl->audio, connHandle, bleAudioMcpCliDiscover) : BTStatus::InvalidState;
}

uint16_t BLEAudioMediaController::getConnHandle() const {
  return _impl ? _impl->link.conn : BLE_AUDIO_CONN_NONE;
}

BTStatus BLEAudioMediaController::sendCommand(BLEAudioMediaCommand command) {
  if (!_impl || !_impl->link.bound()) {
    return BTStatus::InvalidState;
  }
  return bleAudioStatus(bleAudioMcpCliCommand(_impl->link.conn, static_cast<uint8_t>(command)));
}

BTStatus BLEAudioMediaController::play() {
  return sendCommand(BLEAudioMediaCommand::Play);
}

BTStatus BLEAudioMediaController::pause() {
  return sendCommand(BLEAudioMediaCommand::Pause);
}

BTStatus BLEAudioMediaController::stop() {
  return sendCommand(BLEAudioMediaCommand::Stop);
}

BTStatus BLEAudioMediaController::nextTrack() {
  return sendCommand(BLEAudioMediaCommand::NextTrack);
}

BTStatus BLEAudioMediaController::previousTrack() {
  return sendCommand(BLEAudioMediaCommand::PreviousTrack);
}

BTStatus BLEAudioMediaController::readState() {
  return _impl ? _impl->read(BLE_AUDIO_MCP_READ_STATE) : BTStatus::InvalidState;
}

BTStatus BLEAudioMediaController::readPlayerName() {
  return _impl ? _impl->read(BLE_AUDIO_MCP_READ_PLAYER_NAME) : BTStatus::InvalidState;
}

BTStatus BLEAudioMediaController::readTrackTitle() {
  return _impl ? _impl->read(BLE_AUDIO_MCP_READ_TRACK_TITLE) : BTStatus::InvalidState;
}

BLEAudioMediaState BLEAudioMediaController::getState() const {
  return _impl ? _impl->state : BLEAudioMediaState::Inactive;
}

BLEAudioMediaController &BLEAudioMediaController::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioMediaController &BLEAudioMediaController::onStateChanged(StateCallback cb) {
  if (_impl) {
    _impl->stateCb = std::move(cb);
  }
  return *this;
}

BLEAudioMediaController &BLEAudioMediaController::onCommandResult(CommandCallback cb) {
  if (_impl) {
    _impl->commandCb = std::move(cb);
  }
  return *this;
}

BLEAudioMediaController &BLEAudioMediaController::onPlayerName(TextCallback cb) {
  if (_impl) {
    _impl->playerNameCb = std::move(cb);
  }
  return *this;
}

BLEAudioMediaController &BLEAudioMediaController::onTrackTitle(TextCallback cb) {
  if (_impl) {
    _impl->trackTitleCb = std::move(cb);
  }
  return *this;
}

void BLEAudioMediaController::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->stateCb = nullptr;
    _impl->commandCb = nullptr;
    _impl->playerNameCb = nullptr;
    _impl->trackTitleCb = nullptr;
  }
}

BLEAudioMediaController BLEAudio::createMediaController() {
#if BLE_AUDIO_MCP_CLIENT_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("createMediaController(): call between audio.begin() and audio.start()");
    return BLEAudioMediaController();
  }
  auto c = std::make_shared<BLEAudioMediaController::Impl>();
  c->audio = _impl;
  _impl->roles.push_back(c);
  /* `roles` owns the role until end(); a strong capture here would form a cycle with Impl::audio. */
  std::weak_ptr<BLEAudioMediaController::Impl> weak = c;
  _impl->setHandler(BLE_AUDIO_GRP_MCP_CLIENT, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioMcpCliInit());
  });
  return BLEAudioMediaController(c);
#else
  log_e("Media controller is not enabled in this build (CONFIG_BT_MCC)");
  return BLEAudioMediaController();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
