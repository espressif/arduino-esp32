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

#include <vector>
#include "BLEAudioUnicastClient.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioControlLink.h"
#include "BLEAudioStreamImpl.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioUnicastClient.cpp
 * @brief BAP Unicast Client role on top of the BAP engine unit.
 *
 * The role keeps a small peer table. A peer is discovered with
 * `bleAudioUcDiscover` as soon as both conditions hold: the sketch added it
 * and the controller reports its GATT discovery done (`isGattReady`), in
 * whichever order they happen. start() turns the discovered endpoints into
 * one request per stream and hands the whole set to `bleAudioUcStart`, which
 * runs the ASCS sequence one control-point operation at a time and reports
 * the aggregate through the UNICAST_CLIENT event group.
 *
 * API contract is documented on the declarations in `BLEAudioUnicastClient.h`;
 * the definitions below carry implementation notes only.
 */

namespace {

/** @brief One server in the peer table. */
struct Peer {
  BLEAudioUnicastPeerInfo info;  ///< Discovery result (connHandle set when added).
  bool discovering = false;      ///< bleAudioUcDiscover() in flight.
  bool discovered = false;       ///< PACS/ASCS known; usable by start().
  bool autoStart = false;        ///< connect(): start() once discovered.
};

}  // namespace

// --------------------------------------------------------------------------
// Impl
// --------------------------------------------------------------------------

struct BLEAudioUnicastClient::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;  ///< Owning controller (defaults, link table).
  BLESharedMutex mtx;                     ///< Protects this role's callbacks and shared state.
  BLEAudioCodecPreset preset = BLEAudioCodecPreset::LC3_16_2_1;
  BLEAudioQos qos;  ///< Used instead of the preset QoS when customQos.
  bool customQos = false;
  BLEAudioContext context = BLEAudioContext::Media;
  uint8_t sinkPerPeer = 1;    ///< Tx streams requested per peer.
  uint8_t sourcePerPeer = 0;  ///< Rx streams requested per peer.
  bool starting = false;      ///< bleAudioUcStart() call in flight.
  bool running = false;       ///< bleAudioUcStart() accepted, not stopped yet.
  bool streaming = false;     ///< UC_STARTED received.
  std::vector<Peer> peers;
  std::vector<BLEAudioStream> streams;  ///< Streams of the current setup, in request order.

  DiscoveredCallback discoveredCb;
  Callback startedCb;
  Callback stoppedCb;
  ErrorCallback errorCb;

  /** @brief Peer table entry of @p conn, or nullptr. The caller holds mtx. */
  Peer *peer(uint16_t conn) {
    for (auto &p : peers) {
      if (p.info.connHandle == conn) {
        return &p;
      }
    }
    return nullptr;
  }

  /**
   * @brief Start PACS/ASCS discovery of @p p when possible.
   *
   * Returns OK without doing anything while the link's GATT discovery is
   * still running; the GATT_DISCOVERED event calls it again.
   */
  BTStatus discover(uint16_t conn) {
    {
      BLELockGuard lock(mtx.get());
      Peer *p = peer(conn);
      if (!p || p->discovering || p->discovered || !audio->isGattReady(conn)) {
        return BTStatus::OK;
      }
      p->discovering = true;
      p->info.sinkPresets = 0;
      p->info.sourcePresets = 0;
    }
    int err = bleAudioUcDiscover(conn);
    if (err != 0) {
      BLELockGuard lock(mtx.get());
      if (Peer *p = peer(conn)) {
        p->discovering = false;
      }
      log_e("UnicastClient: discovery of conn %u failed to start (err=%d)", conn, err);
    } else {
      log_d("UnicastClient: discovering conn %u", conn);
    }
    return bleAudioStatus(err);
  }

  /**
   * @brief Build one request per stream and start the ASCS sequence.
   *
   * Per peer: `min(requested, endpoints)` streams per direction, Tx first.
   * Each stream gets the next location bit the peer published for that
   * direction, so two sink ASEs on a stereo earbud become left + right.
   * The stream handles are reused when the request count is unchanged, so
   * callbacks the sketch attached survive a stop()/start() cycle.
   */
  BTStatus startStreams() {
    std::vector<ble_audio_uc_stream_req_t> reqs;
    {
      BLELockGuard lock(mtx.get());
      if (running || starting) {
        log_w("UnicastClient: start() while already running");
        return BTStatus::InvalidState;
      }
      ble_audio_codec_t codec;
      ble_audio_qos_t q;
      if (!bleAudioPresetGet(static_cast<uint8_t>(preset), false, &codec, &q)) {
        log_e("UnicastClient: invalid preset %u", (unsigned)preset);
        return BTStatus::InvalidParam;
      }
      if (customQos) {
        q = bleAudioQosToEngine(qos);
      } else {
        q.pd_us = audio->presentationDelayUs;
      }

      for (const auto &p : peers) {
        if (!p.discovered) {
          continue;
        }
        const uint8_t nTx = sinkPerPeer < p.info.sinkEndpoints ? sinkPerPeer : p.info.sinkEndpoints;
        const uint8_t nRx = sourcePerPeer < p.info.sourceEndpoints ? sourcePerPeer : p.info.sourceEndpoints;
        // Only a hint: the server has the final word when it answers Config Codec.
        const uint32_t bit = BLEAudioPresetBit(preset);
        if ((nTx && !(p.info.sinkPresets & bit)) || (nRx && !(p.info.sourcePresets & bit))) {
          log_w("UnicastClient: conn %u does not publish preset %u in its PACS; Config Codec may be rejected", p.info.connHandle, (unsigned)preset);
        }
        for (uint8_t i = 0; i < nTx + nRx; i++) {
          const bool tx = i < nTx;
          const uint8_t idx = tx ? i : i - nTx;
          ble_audio_uc_stream_req_t r = {};
          r.conn_handle = p.info.connHandle;
          r.dir = tx ? BLE_AUDIO_DIR_SINK : BLE_AUDIO_DIR_SOURCE;
          r.ep_index = idx;
          r.codec = codec;
          r.codec.chan_alloc = bleAudioNthLocation(static_cast<uint32_t>(tx ? p.info.sinkLocation : p.info.sourceLocation), idx);
          r.qos = q;
          r.qos.max_sdu = r.codec.octets_per_frame * bleAudioChannelCount(r.codec.chan_alloc) * r.codec.frames_per_sdu;
          r.context = static_cast<uint16_t>(context);
          reqs.push_back(r);
        }
      }
      if (reqs.empty()) {
        log_e("UnicastClient: start() with no discovered peer endpoint matching the requested streams");
        return BTStatus::InvalidState;
      }

      if (streams.size() != reqs.size()) {
        streams.clear();
        for (size_t i = 0; i < reqs.size(); i++) {
          BLEAudioStream s = BLEAudioStreamAccess::create(BLE_AUDIO_STREAM_UNICAST_CLIENT);
          if (!s) {
            streams.clear();
            return BTStatus::NoMemory;
          }
          streams.push_back(s);
        }
      }
      for (size_t i = 0; i < reqs.size(); i++) {
        reqs[i].slot = BLEAudioStreamAccess::slot(streams[i]);
      }
      starting = true;
    }
    int err = bleAudioUcStart(reqs.data(), (uint8_t)reqs.size());
    {
      BLELockGuard lock(mtx.get());
      if (err != 0) {
        starting = false;
      } else if (starting) {
        // The engine can emit UC_STOPPED synchronously when a request fails.
        running = true;
        starting = false;
      }
    }
    if (err != 0) {
      log_e("UnicastClient: stream setup failed to start (%u streams, err=%d)", (unsigned)reqs.size(), err);
      return bleAudioStatus(err);
    }
    log_d("UnicastClient: setting up %u streams", (unsigned)reqs.size());
    return BTStatus::OK;
  }

  /** @brief Event handler: core link events plus the UNICAST_CLIENT group (host task). */
  void handle(const ble_audio_evt_t &e) {
    switch (e.type) {
      case BLE_AUDIO_EVT_GATT_DISCOVERED:
      {
        bool known;
        {
          BLELockGuard lock(mtx.get());
          known = peer(e.conn_handle) != nullptr;
        }
        // A peer added before its GATT discovery finished can be discovered now.
        if (known) {
          (void)discover(e.conn_handle);
        }
        break;
      }
      case BLE_AUDIO_EVT_ACL_DISCONNECTED:
      {
        BLELockGuard lock(mtx.get());
        for (auto it = peers.begin(); it != peers.end(); ++it) {
          if (it->info.connHandle == e.conn_handle) {
            peers.erase(it);
            break;
          }
        }
        break;
      }
      case BLE_AUDIO_EVT_UC_PAC:
      {
        // One per remote LC3 PAC record, before UC_DISCOVERED; a peer may publish several per direction.
        if (!e.data) {
          break;
        }
        const auto *r = static_cast<const ble_audio_uc_pac_t *>(e.data);
        BLELockGuard lock(mtx.get());
        Peer *p = peer(e.conn_handle);
        if (!p) {
          break;
        }
        uint32_t &mask = r->dir == BLE_AUDIO_DIR_SINK ? p->info.sinkPresets : p->info.sourcePresets;
        mask |= bleAudioPacPresets(&r->pac);
        break;
      }
      case BLE_AUDIO_EVT_UC_DISCOVERED:
      {
        BLEAudioUnicastPeerInfo info;
        DiscoveredCallback cb;
        bool autoStart = false;
        bool report = false;
        {
          BLELockGuard lock(mtx.get());
          Peer *p = peer(e.conn_handle);
          if (!p) {
            break;
          }
          p->discovering = false;
          if (e.err != 0 || !e.data) {
            log_w("UnicastClient: discovery of conn %u failed (err=%d)", e.conn_handle, e.err);
            break;
          }
          const auto *d = static_cast<const ble_audio_uc_discovered_t *>(e.data);
          p->discovered = true;
          p->info.sinkEndpoints = d->sink_eps;
          p->info.sourceEndpoints = d->source_eps;
          p->info.sinkLocation = static_cast<BLEAudioLocation>(d->sink_loc);
          p->info.sourceLocation = static_cast<BLEAudioLocation>(d->source_loc);
          p->info.sinkContexts = static_cast<BLEAudioContext>(d->sink_ctx);
          p->info.sourceContexts = static_cast<BLEAudioContext>(d->source_ctx);
          info = p->info;
          autoStart = p->autoStart;
          cb = discoveredCb;
          report = true;
        }
        if (!report) {
          break;
        }
        log_i(
          "UnicastClient: conn %u has %u sink / %u source ASEs (presets sink 0x%04lx, source 0x%04lx)", info.connHandle, info.sinkEndpoints,
          info.sourceEndpoints, (unsigned long)info.sinkPresets, (unsigned long)info.sourcePresets
        );
        if (cb) {
          cb(info);
        }
        if (autoStart) {
          (void)startStreams();
        }
        break;
      }
      case BLE_AUDIO_EVT_UC_STARTED:
      {
        Callback cb;
        {
          BLELockGuard lock(mtx.get());
          streaming = true;
          cb = startedCb;
        }
        log_i("UnicastClient: all streams streaming");
        if (cb) {
          cb();
        }
        break;
      }
      case BLE_AUDIO_EVT_UC_STOPPED:
      {
        Callback cb;
        {
          BLELockGuard lock(mtx.get());
          starting = false;
          running = false;
          streaming = false;
          cb = stoppedCb;
        }
        log_i("UnicastClient: streams released");
        if (cb) {
          cb();
        }
        break;
      }
      case BLE_AUDIO_EVT_UC_ERROR:
        // The engine already logged the failure and is releasing the streams.
        if (e.data) {
          const auto *er = static_cast<const ble_audio_uc_error_t *>(e.data);
          ErrorCallback cb;
          {
            BLELockGuard lock(mtx.get());
            cb = errorCb;
          }
          if (cb) {
            cb(static_cast<Step>(er->op), er->rsp_code, er->reason);
          }
        }
        break;
      default: break;
    }
  }
};

// --------------------------------------------------------------------------
// Configuration
// --------------------------------------------------------------------------

BLEAudioUnicastClient::BLEAudioUnicastClient() = default;

BLEAudioUnicastClient::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioUnicastClient::setPreset(BLEAudioCodecPreset preset) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  if (!bleAudioPresetGet(static_cast<uint8_t>(preset), false, &c, &q)) {
    log_e("UnicastClient: invalid preset %u", (unsigned)preset);
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  if (impl.running || impl.starting) {
    log_e("UnicastClient: setPreset() while running");
    return BTStatus::InvalidState;
  }
  impl.preset = preset;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastClient::setQos(const BLEAudioQos &qos) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.running || impl.starting) {
    log_e("UnicastClient: setQos() while running");
    return BTStatus::InvalidState;
  }
  impl.qos = qos;
  impl.customQos = true;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastClient::setContext(BLEAudioContext context) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.running || impl.starting) {
    log_e("UnicastClient: setContext() while running");
    return BTStatus::InvalidState;
  }
  impl.context = context;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastClient::setSinkStreams(uint8_t perPeer) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.running || impl.starting) {
    log_e("UnicastClient: setSinkStreams() while running");
    return BTStatus::InvalidState;
  }
  impl.sinkPerPeer = perPeer;
  return BTStatus::OK;
}

BTStatus BLEAudioUnicastClient::setSourceStreams(uint8_t perPeer) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLELockGuard lock(impl.mtx.get());
  if (impl.running || impl.starting) {
    log_e("UnicastClient: setSourceStreams() while running");
    return BTStatus::InvalidState;
  }
  impl.sourcePerPeer = perPeer;
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Procedures
// --------------------------------------------------------------------------

BTStatus BLEAudioUnicastClient::addPeer(uint16_t connHandle) {
  if (!_impl || !_impl->audio->started) {
    log_e("UnicastClient: addPeer() before audio.begin()");
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->peer(connHandle)) {
      Peer np;
      np.info.connHandle = connHandle;
      _impl->peers.push_back(np);
    }
  }
  return _impl->discover(connHandle);
}

BTStatus BLEAudioUnicastClient::start() {
  return _impl ? _impl->startStreams() : BTStatus::InvalidState;
}

/** A peer that is already discovered starts immediately; otherwise on UC_DISCOVERED. */
BTStatus BLEAudioUnicastClient::connectAndStart(uint16_t connHandle) {
  BTStatus st = addPeer(connHandle);
  if (!st) {
    return st;
  }
  bool discovered;
  {
    BLELockGuard lock(_impl->mtx.get());
    Peer *p = _impl->peer(connHandle);
    if (!p) {
      return BTStatus::InvalidState;
    }
    discovered = p->discovered;
    p->autoStart = true;
  }
  return discovered ? _impl->startStreams() : BTStatus::OK;
}

BTStatus BLEAudioUnicastClient::stop() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->running && !_impl->starting) {
      return BTStatus::OK;
    }
  }
  int err = bleAudioUcStop();
  if (err != 0) {
    log_e("UnicastClient: stop failed (err=%d)", err);
  }
  return bleAudioStatus(err);
}

bool BLEAudioUnicastClient::isStreaming() const {
  if (!_impl) {
    return false;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->streaming;
}

// --------------------------------------------------------------------------
// Streams
// --------------------------------------------------------------------------

size_t BLEAudioUnicastClient::streamCount() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->streams.size();
}

BLEAudioStream BLEAudioUnicastClient::stream(size_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  BLELockGuard lock(_impl->mtx.get());
  return index < _impl->streams.size() ? _impl->streams[index] : BLEAudioStream();
}

BLEAudioStream BLEAudioUnicastClient::stream(BLEAudioStream::Direction dir, uint8_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  BLELockGuard lock(_impl->mtx.get());
  for (const auto &s : _impl->streams) {
    if (s.direction() == dir && index-- == 0) {
      return s;
    }
  }
  return BLEAudioStream();
}

// --------------------------------------------------------------------------
// Callbacks
// --------------------------------------------------------------------------

void BLEAudioUnicastClient::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = std::move(cb);
}

void BLEAudioUnicastClient::onStarted(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.startedCb = std::move(cb);
}

void BLEAudioUnicastClient::onStopped(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.stoppedCb = std::move(cb);
}

void BLEAudioUnicastClient::onError(ErrorCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.errorCb = std::move(cb);
}

void BLEAudioUnicastClient::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = nullptr;
  impl.startedCb = nullptr;
  impl.stoppedCb = nullptr;
  impl.errorCb = nullptr;
}

// --------------------------------------------------------------------------
// Factory
// --------------------------------------------------------------------------

/**
 * The role registers for the UNICAST_CLIENT group (core link events reach it
 * too) and stages the engine's client init for BLEAudio::begin(). Streams are
 * only allocated at start(), once the peers' endpoints are known.
 */
#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
BLEAudioUnicastClient BLEAudio::createUnicastClient() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioUnicastClient();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("UnicastClient: create before audio.begin() commits");
    return BLEAudioUnicastClient();
  }
  auto cli = std::make_shared<BLEAudioUnicastClient::Impl>();
  cli->audio = _impl;
  _impl->roles.push_back(cli);
  std::weak_ptr<BLEAudioUnicastClient::Impl> weak = cli;
  _impl->setHandler(BLE_AUDIO_GRP_UNICAST_CLIENT, [weak](const ble_audio_evt_t &e) {
    if (auto c = weak.lock()) {
      c->handle(e);
    }
  });
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioUcInit());
  });
  return BLEAudioUnicastClient(cli);
}
#endif

// ==========================================================================
// Feature-disabled factory stubs
// ==========================================================================

#if !BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
BLEAudioUnicastClient BLEAudio::createUnicastClient() {
  log_e("UnicastClient: not enabled in this build (CONFIG_BT_BAP_UNICAST_CLIENT)");
  return BLEAudioUnicastClient();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
