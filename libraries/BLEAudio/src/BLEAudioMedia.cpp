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

#include "BLEAudioMedia.h"
#include "BLEAudioControlLink.h"
#include "esp32-hal-log.h"

namespace {

BTStatus commandStatus(int err, uint8_t result) {
  if (err) {
    return bleAudioStatus(err);
  }
  switch (result) {
    case BLE_AUDIO_MCP_CMD_SUCCESS: return BTStatus::OK;
    case 2:                         return BTStatus::NotSupported;  // MCS: opcode not supported.
    case 3:                         return BTStatus::InvalidState;  // MCS: player inactive.
    case 4:                         return BTStatus::Fail;          // MCS: cannot be completed.
    default:                        return BTStatus::Fail;
  }
}

}  // namespace

// --------------------------------------------------------------------------
// Player
// --------------------------------------------------------------------------

struct BLEAudioMediaPlayer::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  String playerName;
  String trackTitle;
  std::atomic<BLEAudioMediaState> state{BLEAudioMediaState::Inactive};
  StateCallback onStateChangedCb;
  CommandCallback onCommandResultCb;

  BTStatus setText(bool title, const String &text) {
    if (audio->started.load()) {
      const BTStatus st = bleAudioStatus(bleAudioMcpSrvSetText(title, text.c_str()));
      if (!st) {
        return st; /* Stack refused: keep the previous text. */
      }
    }
    BLELockGuard lock(mtx.get());
    (title ? trackTitle : playerName) = text;
    return BTStatus::OK;
  }

  BTStatus apply() {
    int err = bleAudioMcpSrvInit();
    if (err != 0) {
      return bleAudioStatus(err);
    }
    String pName;
    String tTitle;
    {
      BLELockGuard lock(mtx.get());
      pName = playerName;
      tTitle = trackTitle;
    }
    if (pName.length() && bleAudioMcpSrvSetText(false, pName.c_str()) != 0) {
      log_w("Media: player name \"%s\" rejected by the media proxy", pName.c_str());
    }
    if (tTitle.length() && bleAudioMcpSrvSetText(true, tTitle.c_str()) != 0) {
      log_w("Media: track title \"%s\" rejected by the media proxy", tTitle.c_str());
    }
    return BTStatus::OK;
  }

  void handle(const ble_audio_evt_t &e) {
    if (e.type == BLE_AUDIO_EVT_MCP_SRV_STATE && !e.err && e.data) {
      const auto st = static_cast<BLEAudioMediaState>(*static_cast<const uint8_t *>(e.data));
      state.store(st, std::memory_order_relaxed);
      if (auto cb = bleAudioCopyCallback(*this, &Impl::onStateChangedCb)) {
        cb(st);
      }
    } else if (e.type == BLE_AUDIO_EVT_MCP_SRV_RESULT && e.data) {
      const auto *r = static_cast<const ble_audio_mcp_result_t *>(e.data);
      if (auto cb = bleAudioCopyCallback(*this, &Impl::onCommandResultCb)) {
        cb(static_cast<BLEAudioMediaCommand>(r->opcode), commandStatus(e.err, r->result));
      }
    }
  }
};

BLEAudioMediaPlayer::BLEAudioMediaPlayer() = default;

BLEAudioMediaPlayer::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioMediaPlayer::setPlayerName(const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.setText(false, name);
}

BTStatus BLEAudioMediaPlayer::setTrackTitle(const String &title) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.setText(true, title);
}

BTStatus BLEAudioMediaPlayer::sendCommand(BLEAudioMediaCommand command) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return bleAudioStatus(bleAudioMcpSrvCommand(static_cast<uint8_t>(command)));
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
  BLE_CHECK_IMPL(BLEAudioMediaState::Inactive);
  return impl.state.load(std::memory_order_relaxed);
}

void BLEAudioMediaPlayer::onStateChanged(StateCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onStateChangedCb = std::move(cb);
}

void BLEAudioMediaPlayer::onCommandResult(CommandCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onCommandResultCb = std::move(cb);
}

void BLEAudioMediaPlayer::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onStateChangedCb = nullptr;
  impl.onCommandResultCb = nullptr;
}

#if BLE_AUDIO_MCP_SERVER_SUPPORTED
BLEAudioMediaPlayer BLEAudio::createMediaPlayer() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioMediaPlayer();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioMediaPlayer>(_impl, "createMediaPlayer", BLE_AUDIO_GRP_MCP_SERVER, [](BLEAudioMediaPlayer::Impl &s) {
    return s.apply();
  });
}
#endif

// --------------------------------------------------------------------------
// Controller
// --------------------------------------------------------------------------

struct BLEAudioMediaController::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;
  BLEAudioControlLink link;
  std::atomic<BLEAudioMediaState> state{BLEAudioMediaState::Inactive};
  DiscoveredCallback onDiscoveredCb;
  StateCallback onStateChangedCb;
  CommandCallback onCommandResultCb;
  TextCallback onPlayerNameCb;
  TextCallback onTrackTitleCb;

  BTStatus read(uint8_t what) const {
    const uint16_t c = link.activeConn();
    return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioMcpCliRead(c, what)) : BTStatus::InvalidState;
  }

  void handle(const ble_audio_evt_t &e) {
    if (BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CORE) {
      if (e.type == BLE_AUDIO_EVT_ACL_DISCONNECTED && e.conn_handle == link.getConn()) {
        state.store(BLEAudioMediaState::Inactive, std::memory_order_relaxed);
      }
      const int err = link.onCore(e, bleAudioMcpCliDiscover);
      if (err) {
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(err));
        }
      }
      return;
    }
    if (e.conn_handle != link.getConn()) {
      return;
    }
    switch (e.type) {
      case BLE_AUDIO_EVT_MCP_CLI_DISCOVERED:
        if (auto cb = bleAudioCopyCallback(*this, &Impl::onDiscoveredCb)) {
          cb(bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_MCP_CLI_STATE:
        if (e.err || !e.data) {
          if (e.err) {
            log_w("MediaController: state read/notification failed (err=%d); keeping the last known state", e.err);
          }
          break;
        }
        {
          const auto st = static_cast<BLEAudioMediaState>(*static_cast<const uint8_t *>(e.data));
          state.store(st, std::memory_order_relaxed);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onStateChangedCb)) {
            cb(st);
          }
        }
        break;
      case BLE_AUDIO_EVT_MCP_CLI_RESULT:
        if (e.data) {
          const auto *r = static_cast<const ble_audio_mcp_result_t *>(e.data);
          if (auto cb = bleAudioCopyCallback(*this, &Impl::onCommandResultCb)) {
            cb(static_cast<BLEAudioMediaCommand>(r->opcode), commandStatus(e.err, r->result));
          }
        }
        break;
      case BLE_AUDIO_EVT_MCP_CLI_PLAYER_NAME:
      case BLE_AUDIO_EVT_MCP_CLI_TRACK_TITLE:
      {
        const TextCallback cb = bleAudioCopyCallback(*this, e.type == BLE_AUDIO_EVT_MCP_CLI_PLAYER_NAME ? &Impl::onPlayerNameCb : &Impl::onTrackTitleCb);
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
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.link.discover(*impl.audio, connHandle, bleAudioMcpCliDiscover);
}

uint16_t BLEAudioMediaController::getConnHandle() const {
  BLE_CHECK_IMPL(BLE_AUDIO_CONN_NONE);
  return impl.link.getConn();
}

BTStatus BLEAudioMediaController::sendCommand(BLEAudioMediaCommand command) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  const uint16_t c = impl.link.activeConn();
  return c != BLE_AUDIO_CONN_NONE ? bleAudioStatus(bleAudioMcpCliCommand(c, static_cast<uint8_t>(command))) : BTStatus::InvalidState;
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
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.read(BLE_AUDIO_MCP_READ_STATE);
}

BTStatus BLEAudioMediaController::readPlayerName() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.read(BLE_AUDIO_MCP_READ_PLAYER_NAME);
}

BTStatus BLEAudioMediaController::readTrackTitle() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.read(BLE_AUDIO_MCP_READ_TRACK_TITLE);
}

BLEAudioMediaState BLEAudioMediaController::getState() const {
  BLE_CHECK_IMPL(BLEAudioMediaState::Inactive);
  return impl.state.load(std::memory_order_relaxed);
}

void BLEAudioMediaController::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = std::move(cb);
}

void BLEAudioMediaController::onStateChanged(StateCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onStateChangedCb = std::move(cb);
}

void BLEAudioMediaController::onCommandResult(CommandCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onCommandResultCb = std::move(cb);
}

void BLEAudioMediaController::onPlayerName(TextCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onPlayerNameCb = std::move(cb);
}

void BLEAudioMediaController::onTrackTitle(TextCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onTrackTitleCb = std::move(cb);
}

void BLEAudioMediaController::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.onDiscoveredCb = nullptr;
  impl.onStateChangedCb = nullptr;
  impl.onCommandResultCb = nullptr;
  impl.onPlayerNameCb = nullptr;
  impl.onTrackTitleCb = nullptr;
}

#if BLE_AUDIO_MCP_CLIENT_SUPPORTED
BLEAudioMediaController BLEAudio::createMediaController() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioMediaController();  // begin() already logged the cause
  }
  return BLEAudioImplCommon::createStagedRole<BLEAudioMediaController>(
    _impl, "createMediaController", BLE_AUDIO_GRP_MCP_CLIENT,
    [](BLEAudioMediaController::Impl &) {
      return bleAudioStatus(bleAudioMcpCliInit());
    }
  );
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_MCP_SERVER_SUPPORTED
BLEAudioMediaPlayer BLEAudio::createMediaPlayer() {
  log_e("Media player is not enabled in this build (CONFIG_BT_MCS and CONFIG_BT_MCTL_LOCAL_PLAYER_CONTROL)");
  return BLEAudioMediaPlayer();
}
#endif

#if !BLE_AUDIO_MCP_CLIENT_SUPPORTED
BLEAudioMediaController BLEAudio::createMediaController() {
  log_e("Media controller is not enabled in this build (CONFIG_BT_MCC)");
  return BLEAudioMediaController();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
