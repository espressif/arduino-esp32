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

#include <string.h>
#include <errno.h>
#include <vector>
#include "BLEAudioCap.h"
#include "BLEAudioAuracast.h"
#include "core/BLEBackend.h"
#include "BLEAudioBackend.h"
#include "core/BLEImplHelpers.h"
#include "BLEAudioControlLink.h"
#include "BLEAudioProfiles.h"
#include "BLEAudioStreamImpl.h"
#include "engine/BLEAudioEngineCap.h"
#include "BLE.h"
#include "advertising/BLEAdvertising.h"
#include "advertising/BLEAdvertisementData.h"
#include "esp_random.h"
#include "esp32-hal-log.h"
#include "core/BLESync.h"
#include "core/BLETask.h"

/**
 * @file BLEAudioCap.cpp
 * @brief CAP Initiator and Commander roles on the CAP engine unit.
 *
 * The initiator builds unicast stream requests exactly like
 * `BLEAudioUnicastClient` and the broadcast advertising like
 * `BLEAudioBroadcastSource`; the engine runs the CAP procedures over them.
 * Acceptors that ask for the periodic advertising over PAST get it here,
 * through the host GAP API, because the audio stack has no PAST call.
 *
 * The acceptor factory lives in BLEAudioCoordinatedSet.cpp: the acceptor is
 * a handle on a CAS-included coordinated set member.
 */

namespace {

constexpr uint8_t kAdTypeServiceData16 = BLEAudioAuracast::kAdTypeServiceData16;
constexpr uint16_t kPaInterval = BLEAudioAuracast::kPaInterval;  ///< 80 ms; also sent to acceptors, so min = max.

/** An acceptor added with addAcceptor(). */
struct Peer {
  BLEAudioCapPeerInfo info;  ///< What discovery found (endpoints, locations, contexts).
  bool discovering = false;  ///< CAP discovery in flight.
  bool discovered = false;   ///< Discovery succeeded; the peer takes part in startUnicast().
};

/** @brief Send the SyncInfo of advertising set @p adv to the acceptor on @p conn (service data = source ID). */
void sendPast(uint8_t adv, uint16_t conn, const ble_audio_cap_past_req_t &r) {
  const uint16_t serviceData = (uint16_t)r.src_id << 8;
  int err = -1;
#if BLE_PAST_SUPPORTED
  err = bleAudioEngineSendPast(adv, conn, serviceData, r.addr, r.addr_type);
#else
  (void)adv;
  (void)conn;
  (void)serviceData;
  (void)r;
#endif
  if (err != 0) {
    log_w("CapInitiator: PAST to conn %u failed (err=%d); the acceptor must scan for the broadcast", conn, err);
  }
}

}  // namespace

// --------------------------------------------------------------------------
// Acceptor
// --------------------------------------------------------------------------

BLEAudioCapAcceptor::operator bool() const {
  return _impl != nullptr;
}

BLEAudioCoordinatedSetMember BLEAudioCapAcceptor::coordinatedSet() const {
  return BLEAudioCoordinatedSetMember(_impl);
}

BTStatus BLEAudioCapAcceptor::setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]) {
  return coordinatedSet().setSirk(sirk);
}

BTStatus BLEAudioCapAcceptor::setSetSize(uint8_t size) {
  return coordinatedSet().setSetSize(size);
}

BTStatus BLEAudioCapAcceptor::setRank(uint8_t rank) {
  return coordinatedSet().setRank(rank);
}

BTStatus BLEAudioCapAcceptor::setLockable(bool lockable) {
  return coordinatedSet().setLockable(lockable);
}

// --------------------------------------------------------------------------
// Initiator
// --------------------------------------------------------------------------

struct BLEAudioCapInitiator::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;  ///< Protects this role's callbacks and shared state.
  BLEAudioCodecPreset preset = BLEAudioCodecPreset::LC3_16_2_1;
  BLEAudioQos qos;  ///< Used instead of the preset QoS when customQos.
  bool customQos = false;
  BLEAudioContext context = BLEAudioContext::Media;
  uint8_t sinkPerPeer = 1;                         ///< Sink ASEs to use per acceptor (capped by what it has).
  uint8_t sourcePerPeer = 0;                       ///< Source ASEs to use per acceptor.
  uint32_t broadcastId = esp_random() & 0xFFFFFF;  ///< 24-bit Broadcast_ID, random per handle.
  String code;                                     ///< Broadcast Code; empty = unencrypted.
  String name;                                     ///< Broadcast Name; empty = device name.
  std::vector<Peer> peers;
  std::vector<BLEAudioStream> ucStreams;  ///< Unicast streams in request order; they also carry a handed-over broadcast.
  std::vector<BLEAudioStream> bcStreams;  ///< startBroadcast() streams, one per BIS.
  bool ucStarting = false;                ///< Unicast start call in flight.
  bool ucRunning = false;                 ///< Unicast start accepted, not stopped yet.
  bool ucStreaming = false;
  bool bcStarting = false;  ///< A broadcast start call is in flight.
  bool bcStreaming = false;
  bool plainBroadcast = false;      ///< startBroadcast() session (bcStreams).
  bool handedOver = false;          ///< The unicast sink streams run as a broadcast.
  bool hoBusy = false;              ///< Handover procedure in flight.
  int16_t advInstance = -1;         ///< Advertising instance of the broadcast, -1 when none.
  BLESync stopSync{"cap-bc-stop"};  ///< Wait for CAP_BC_STARTED / CAP_BC_STOPPED.

  DiscoveredCallback discoveredCb;
  StatusCallback ucStartedCb;
  StatusCallback ucUpdatedCb;
  Callback ucStoppedCb;
  Callback bcStartedCb;
  StoppedCallback bcStoppedCb;
  HandoverCallback handoverCb;

  /** @return the acceptor on @p conn, or nullptr. The caller holds mtx. */
  Peer *peer(uint16_t conn) {
    for (auto &p : peers) {
      if (p.info.connHandle == conn) {
        return &p;
      }
    }
    return nullptr;
  }

  /** @brief Start discovery of @p conn; deferred to GATT_DISCOVERED while the link is not ready. */
  BTStatus discover(uint16_t conn) {
    {
      BLELockGuard lock(mtx.get());
      Peer *p = peer(conn);
      if (!p) {
        Peer np;
        np.info.connHandle = conn;
        peers.push_back(np);
        p = &peers.back();
      }
      if (p->discovering || p->discovered || !audio->isGattReady(conn)) {
        return BTStatus::OK;
      }
      p->discovering = true;
    }
    int err = bleAudioCapDiscover(conn);
    if (err != 0) {
      BLELockGuard lock(mtx.get());
      if (Peer *p = peer(conn)) {
        p->discovering = false;
      }
      log_e("CapInitiator: discovery of conn %u failed to start (err=%d)", conn, err);
    }
    return bleAudioStatus(err);
  }

  /** Codec and QoS of the preset (unicast or broadcast variant), with the custom QoS or presentation delay applied. */
  void codecQos(bool broadcast, ble_audio_codec_t &c, ble_audio_qos_t &q) const {
    bleAudioPresetGet(static_cast<uint8_t>(preset), broadcast, &c, &q);
    if (customQos) {
      q = bleAudioQosToEngine(qos);
    } else {
      q.pd_us = audio->presentationDelayUs;
    }
  }

  /** @brief One request per stream, per discovered peer: sink streams first, then (unless @p sinkOnly) source. */
  std::vector<ble_audio_uc_stream_req_t> requests(bool sinkOnly) {
    BLELockGuard lock(mtx.get());
    ble_audio_codec_t codec;
    ble_audio_qos_t q;
    codecQos(false, codec, q);
    std::vector<ble_audio_uc_stream_req_t> reqs;
    for (const auto &p : peers) {
      if (!p.discovered) {
        continue;
      }
      const uint8_t nTx = sinkPerPeer < p.info.sinkEndpoints ? sinkPerPeer : p.info.sinkEndpoints;
      const uint8_t nRx = sinkOnly ? 0 : (sourcePerPeer < p.info.sourceEndpoints ? sourcePerPeer : p.info.sourceEndpoints);
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
    return reqs;
  }

  /** Engine description of advertising instance @p inst (SID = instance, own address). */
  ble_audio_cap_adv_t advInfo(uint8_t inst) const {
    ble_audio_cap_adv_t a = {};
    a.handle = inst;
    a.sid = inst & 0x0F;
    const BTAddress own = BLE.getAddress();
    a.addr_type = bleAudioAddrType(own.type());
    memcpy(a.addr, own.data(), sizeof(a.addr));
    a.pa_interval = kPaInterval;
    {
      BLELockGuard lock(mtx.get());
      a.broadcast_id = broadcastId;
    }
    return a;
  }

  /** @brief Put the engine's BASE in the periodic advertising data of @p inst. */
  BTStatus publishBase(uint8_t inst) {
    uint8_t field[2 + 128];
    uint16_t len = 0;
    int err = bleAudioCapBroadcastGetBase(field + 2, sizeof(field) - 2, &len);
    if (err != 0) {
      log_e("CapInitiator: BASE encoding failed (err=%d)", err);
      return bleAudioStatus(err);
    }
    field[0] = (uint8_t)(1 + len);
    field[1] = kAdTypeServiceData16;
    BLEAdvertisementData per(BLEAdvertisementData::EXTENDED_MAX_PAYLOAD);
    per.addRaw(field, 2 + len);
    return BLE.getAdvertising().setPeriodicAdvData(inst, per);
  }

  /**
   * @brief Start the broadcast advertising on @p inst (Broadcast Audio
   *        Announcement + name); with @p withBase also the periodic train.
   */
  BTStatus advertise(uint8_t inst, bool withBase) {
    BLEAdvertising adv = BLE.getAdvertising();
    adv.setExtType(inst, BLEAdvType::NonConnectable);
    adv.setExtPhy(inst, BLEPhy::PHY_1M, BLEPhy::PHY_1M);
    adv.setExtSID(inst, inst & 0x0F);
    String cfgName;
    String cfgCode;
    uint32_t bcastId;
    uint16_t ctx;
    ble_audio_codec_t c;
    ble_audio_qos_t q;
    {
      BLELockGuard lock(mtx.get());
      cfgName = name;
      cfgCode = code;
      bcastId = broadcastId;
      ctx = static_cast<uint16_t>(context);
      codecQos(true, c, q);
    }
    const String advName = cfgName.length() ? cfgName : String(BLE.getDeviceName().c_str());
    BLEAdvertisementData ext(BLEAdvertisementData::EXTENDED_MAX_PAYLOAD);
#if BLE_AUDIO_PBP_SUPPORTED
    // PBP features: High Quality for 48 kHz presets, Standard otherwise, plus
    // Encryption; the PBA carries the same context and title as a plain
    // broadcast source.
    uint8_t pbaFeatures = c.sample_rate_hz == 48000 ? BLEAudioPublicBroadcast::HighQuality : BLEAudioPublicBroadcast::StandardQuality;
    if (cfgCode.length()) {
      pbaFeatures |= BLEAudioPublicBroadcast::Encryption;
    }
    BLEAudioAuracast::buildExt(ext, bcastId, advName, true, ctx, pbaFeatures);
#else
    (void)c;
    (void)q;
    (void)cfgCode;
    (void)ctx;
    BLEAudioAuracast::buildExt(ext, bcastId, advName, false, 0, 0);
#endif
    BTStatus st = adv.setExtAdvertisementData(inst, ext);
    if (st) {
      adv.setPeriodicAdvInterval(inst, kPaInterval, kPaInterval);
      if (withBase) {
        st = publishBase(inst);
      }
    }
    if (st) {
      st = adv.startExtended(inst, 0, 0);
    }
    if (st && withBase) {
      st = adv.startPeriodicAdv(inst);
    }
    if (!st) {
      log_e("CapInitiator: advertising start failed on instance %u (%s)", inst, st.toString());
      (void)adv.stopExtended(inst);
      return st;
    }
    BLELockGuard lock(mtx.get());
    advInstance = inst;
    return BTStatus::OK;
  }

  /** Stop the periodic and extended advertising started by advertise(), if any. */
  void stopAdvertising() {
    int16_t instance;
    {
      BLELockGuard lock(mtx.get());
      instance = advInstance;
      if (instance < 0) {
        return;
      }
      advInstance = -1;
    }
    BLEAdvertising adv = BLE.getAdvertising();
    (void)adv.stopPeriodicAdv((uint8_t)instance);
    (void)adv.stopExtended((uint8_t)instance);
  }

  BTStatus waitStopped(uint32_t timeoutMs = 5000) {
    return bleAudioWaitUntilIdle(
      mtx, stopSync, "CapInitiator",
      [this]() {
        return bcStreaming || bcStarting;
      },
      timeoutMs
    );
  }

  /** @brief Engine source gone: release the advertising and reset the broadcast state. */
  void broadcastEnded() {
    {
      BLELockGuard lock(mtx.get());
      plainBroadcast = false;
      handedOver = false;
      bcStreaming = false;
      bcStarting = false;
    }
    bleAudioCapBroadcastDelete();
    stopAdvertising();
  }

  /**
   * @brief broadcastEnded() only when the engine really let the source go.
   *
   * source_delete is refused while the BIG streams; clearing the C++ flags
   * anyway wedges the next start (create refuses with the engine source still
   * set). Callers must keep the flags and report failure when this returns
   * false so stopBroadcast()/end() can retry.
   */
  bool broadcastEndedIfGone() {
    if (bleAudioCapBroadcastExists()) {
      return false;
    }
    broadcastEnded();
    return true;
  }

  /**
   * Engine event (core and initiator/handover groups): runs deferred
   * discoveries, tracks acceptors, and maps procedure completions to the
   * application callbacks.
   */
  void handle(const ble_audio_evt_t &e) {
    switch (e.type) {
      case BLE_AUDIO_EVT_GATT_DISCOVERED:
      {
        bool known;
        {
          BLELockGuard lock(mtx.get());
          known = peer(e.conn_handle) != nullptr;
        }
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
      case BLE_AUDIO_EVT_CAP_DISCOVERED:
      {
        BLEAudioCapPeerInfo info;
        DiscoveredCallback cb;
        {
          BLELockGuard lock(mtx.get());
          Peer *p = peer(e.conn_handle);
          if (!p) {
            break;
          }
          p->discovering = false;
          if (e.err == 0 && e.data) {
            const auto *d = static_cast<const ble_audio_cap_discovered_t *>(e.data);
            p->discovered = true;
            p->info.coordinatedSet = d->coordinated;
            p->info.sinkEndpoints = d->eps.sink_eps;
            p->info.sourceEndpoints = d->eps.source_eps;
            p->info.sinkLocation = static_cast<BLEAudioLocation>(d->eps.sink_loc);
            p->info.sourceLocation = static_cast<BLEAudioLocation>(d->eps.source_loc);
            p->info.sinkContexts = static_cast<BLEAudioContext>(d->eps.sink_ctx);
            p->info.sourceContexts = static_cast<BLEAudioContext>(d->eps.source_ctx);
            log_i(
              "CapInitiator: conn %u has %u sink / %u source ASEs%s", e.conn_handle, d->eps.sink_eps, d->eps.source_eps, d->coordinated ? " (set member)" : ""
            );
          } else {
            log_w("CapInitiator: discovery of conn %u failed (err=%d)", e.conn_handle, e.err);
          }
          info = p->info;
          cb = discoveredCb;
        }
        if (cb) {
          cb(bleAudioStatus(e.err), info);
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_UC_STARTED:
      {
        StatusCallback cb;
        {
          BLELockGuard lock(mtx.get());
          ucStarting = false;
          ucRunning = (e.err == 0);
          ucStreaming = (e.err == 0);
          cb = ucStartedCb;
        }
        if (e.err != 0) {
          log_w("CapInitiator: unicast start failed (conn %u, err=%d)", e.conn_handle, e.err);
        }
        if (cb) {
          cb(bleAudioStatus(e.err));
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_UC_UPDATED:
        if (auto cb = bleAudioCopyCallback(*this, &Impl::ucUpdatedCb)) {
          cb(bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_CAP_UC_STOPPED:
      {
        Callback cb;
        {
          BLELockGuard lock(mtx.get());
          if (e.err != 0) {
            // The stack could not release the unicast group; keep the session
            // marked running so a later stopUnicast() can retry.
            log_e("CapInitiator: unicast stop failed (err=%d)", e.err);
            break;
          }
          ucStarting = false;
          ucRunning = false;
          ucStreaming = false;
          cb = ucStoppedCb;
        }
        if (cb) {
          cb();
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_BC_STARTED:
      {
        Callback cb;
        {
          BLELockGuard lock(mtx.get());
          bcStarting = false;
          bcStreaming = true;
          cb = bcStartedCb;
        }
        log_i("CapInitiator: broadcast 0x%06lX streaming", (unsigned long)broadcastId);
        // A stopBroadcast() issued while start was in flight waits on this.
        stopSync.give(BTStatus::OK);
        if (cb) {
          cb();
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_BC_STOPPED:
      {
        bool duringHandover;
        StoppedCallback cb;
        {
          BLELockGuard lock(mtx.get());
          duringHandover = hoBusy;
          bcStarting = false;
          if (duringHandover) {
            bcStreaming = false;
          } else {
            cb = bcStoppedCb;
          }
        }
        if (duringHandover) {
          break;
        }
        broadcastEnded();
        stopSync.give(BTStatus::OK);
        if (cb) {
          cb((uint8_t)e.err);
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_PAST_REQ:
      {
        int16_t instance;
        {
          BLELockGuard lock(mtx.get());
          instance = advInstance;
        }
        if (instance >= 0 && e.data) {
          sendPast((uint8_t)instance, e.conn_handle, *static_cast<const ble_audio_cap_past_req_t *>(e.data));
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_HO_CREATED:
      {
        int16_t instance;
        {
          BLELockGuard lock(mtx.get());
          instance = advInstance;
        }
        BTStatus st = instance >= 0 ? publishBase((uint8_t)instance) : BTStatus::InvalidState;
        if (st) {
          st = BLE.getAdvertising().startPeriodicAdv((uint8_t)instance);
        }
        if (!st) {
          log_e("CapInitiator: periodic advertising for the handover failed (%s)", st.toString());
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_HO_TO_BROADCAST:
      {
        HandoverCallback cb;
        {
          BLELockGuard lock(mtx.get());
          hoBusy = false;
          if (e.err == 0) {
            ucRunning = false;
            ucStreaming = false;
            handedOver = true;
            bcStreaming = true;
          }
          cb = handoverCb;
        }
        if (e.err == 0) {
          log_i("CapInitiator: unicast handed over to broadcast 0x%06lX", (unsigned long)broadcastId);
        } else {
          log_w("CapInitiator: handover to broadcast failed (err=%d)", e.err);
          // This runs on the host task, so there is no waiting here: a
          // successful stop completes asynchronously through CAP_BC_STOPPED
          // (hoBusy is already false, so it runs the full broadcastEnded()).
          // Only a synchronously failed stop needs immediate cleanup, and
          // only when the engine really let the source go.
          if (bleAudioCapBroadcastStop() != 0 && !broadcastEndedIfGone()) {
            log_e("CapInitiator: broadcast source still live after the failed handover; retry stopBroadcast()");
          } else if (!bleAudioCapBroadcastExists()) {
            stopAdvertising();
          }
          /* The procedure may already have stopped the unicast group. End it
           * deterministically so startUnicast() can rebuild the session; the
           * failure is terminal for this session (UnicastStop runs with
           * release=true), so the role is observably down as soon as the
           * failure reports and UC_STOPPED only confirms it. */
          int stopErr = bleAudioCapUnicastStop();
          BLELockGuard lock(mtx.get());
          if (stopErr != 0) {
            log_w("CapInitiator: unicast cleanup after the failed handover failed (err=%d); stopUnicast() can retry", stopErr);
          } else {
            ucRunning = false;
            ucStreaming = false;
          }
          handedOver = false;
        }
        if (cb) {
          cb(bleAudioStatus(e.err), true);
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_HO_TO_UNICAST:
      {
        HandoverCallback cb;
        {
          BLELockGuard lock(mtx.get());
          hoBusy = false;
          if (e.err == 0) {
            handedOver = false;
            bcStreaming = false;
            ucRunning = true;
            ucStreaming = true;
          }
          cb = handoverCb;
        }
        if (e.err == 0) {
          stopAdvertising();
          log_i("CapInitiator: broadcast handed back to unicast");
        } else {
          log_w("CapInitiator: handover to unicast failed (err=%d)", e.err);
        }
        if (cb) {
          cb(bleAudioStatus(e.err), false);
        }
        break;
      }
      default: break;
    }
  }

  // Returned by value: the streams vectors are mutated by startUnicast()/
  // startBroadcast() on the caller task, so returning a reference after the
  // lock is released would hand the caller a dangling reference.
  std::vector<BLEAudioStream> streams() const {
    BLELockGuard lock(mtx.get());
    return plainBroadcast ? bcStreams : ucStreams;
  }
};

BLEAudioCapInitiator::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioCapInitiator::setPreset(BLEAudioCodecPreset preset) {
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setPreset() before audio.begin()");
    return BTStatus::InvalidState;
  }
  if (!bleAudioPresetGet(static_cast<uint8_t>(preset), false, &c, &q)) {
    log_e("CapInitiator: invalid preset %u", (unsigned)preset);
    return BTStatus::InvalidParam;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.preset = preset;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::setQos(const BLEAudioQos &qos) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setQos() before audio.begin()");
    return BTStatus::InvalidState;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.qos = qos;
  impl.customQos = true;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::setContext(BLEAudioContext context) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setContext() before audio.begin()");
    return BTStatus::InvalidState;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.context = context;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::setSinkStreams(uint8_t perPeer) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setSinkStreams() before audio.begin()");
    return BTStatus::InvalidState;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.sinkPerPeer = perPeer;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::setSourceStreams(uint8_t perPeer) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setSourceStreams() before audio.begin()");
    return BTStatus::InvalidState;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.sourcePerPeer = perPeer;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::setBroadcastId(uint32_t broadcastId) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setBroadcastId() before audio.begin()");
    return BTStatus::InvalidState;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.broadcastId = broadcastId & 0xFFFFFF;
  return BTStatus::OK;
}

uint32_t BLEAudioCapInitiator::getBroadcastId() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->broadcastId;
}

BTStatus BLEAudioCapInitiator::setBroadcastCode(const String &code) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setBroadcastCode() before audio.begin()");
    return BTStatus::InvalidState;
  }
  BLELockGuard lock(impl.mtx.get());
  if (code.length() > BLE_AUDIO_BCODE_SIZE) {
    log_w("CapInitiator: Broadcast Code truncated to %u characters", BLE_AUDIO_BCODE_SIZE);
  }
  impl.code = code;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::setBroadcastName(const String &name) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.audio && impl.audio->started.load()) {
    log_e("CapInitiator: setBroadcastName() before audio.begin()");
    return BTStatus::InvalidState;
  }
  BLELockGuard lock(impl.mtx.get());
  impl.name = name;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::discover(uint16_t connHandle) {
  if (!_impl || !_impl->audio->started) {
    log_e("CapInitiator: discover() before audio.begin()");
    return BTStatus::InvalidState;
  }
  return _impl->discover(connHandle);
}

/** Stream handles are reused while the request count is unchanged, so attached callbacks survive a restart. */
BTStatus BLEAudioCapInitiator::startUnicast() {
  if (!_impl || !_impl->audio->started.load()) {
    return BTStatus::InvalidState;
  }
  std::vector<ble_audio_uc_stream_req_t> reqs;
  {
    BLELockGuard lock(_impl->mtx.get());
    if (_impl->ucRunning || _impl->ucStarting || _impl->handedOver || _impl->hoBusy) {
      return BTStatus::InvalidState;
    }
    reqs = _impl->requests(false);
    if (reqs.empty()) {
      log_e("CapInitiator: startUnicast() with no discovered acceptor endpoint");
      return BTStatus::InvalidState;
    }
    auto &streams = _impl->ucStreams;
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
    _impl->ucStarting = true;
  }
  int err = bleAudioCapUnicastStart(reqs.data(), (uint8_t)reqs.size());
  {
    BLELockGuard lock(_impl->mtx.get());
    if (err != 0) {
      _impl->ucStarting = false;
    } else if (_impl->ucStarting) {
      // The engine may report UC_STOPPED synchronously on a failed request.
      _impl->ucStarting = false;
      _impl->ucRunning = true;
    }
  }
  if (err != 0) {
    log_e("CapInitiator: unicast start failed (%u streams, err=%d)", (unsigned)reqs.size(), err);
    return bleAudioStatus(err);
  }
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::updateUnicast(BLEAudioContext context) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->ucRunning || _impl->hoBusy) {
      return BTStatus::InvalidState;
    }
    _impl->context = context;
  }
  return bleAudioStatus(bleAudioCapUnicastUpdate(static_cast<uint16_t>(context)));
}

BTStatus BLEAudioCapInitiator::stopUnicast() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    // After a handover the CIS is gone but the CAP unicast group still owns
    // the streams; it must be releasable just like a running session.
    if ((!_impl->ucRunning && !_impl->handedOver) || _impl->hoBusy) {
      return BTStatus::InvalidState;
    }
  }
  int err = bleAudioCapUnicastStop();
  if (err != 0) {
    log_e("CapInitiator: unicast stop failed (err=%d)", err);
  }
  return bleAudioStatus(err);
}

bool BLEAudioCapInitiator::isUnicastStreaming() const {
  if (!_impl) {
    return false;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->ucStreaming;
}

/** Order: source (BASE) first, then advertising (needs the BASE), then the BIG (needs the train). */
BTStatus BLEAudioCapInitiator::startBroadcast(uint8_t channels, uint8_t advInstance) {
  if (!_impl || !_impl->audio->started.load()) {
    log_e("CapInitiator: startBroadcast() before audio.begin()");
    return BTStatus::InvalidState;
  }
  Impl &m = *_impl;
  if (channels == 0) {
    channels = 1;
  }
  std::vector<ble_audio_slot_t *> slots;
  std::vector<uint32_t> locations;
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  uint16_t ctx;
  String cfgCode;
  {
    BLELockGuard lock(m.mtx.get());
    if (m.advInstance >= 0 || m.hoBusy || m.bcStarting) {
      log_w("CapInitiator: a broadcast is already running");
      return BTStatus::InvalidState;
    }
    while (m.bcStreams.size() > channels) {
      m.bcStreams.pop_back();
    }
    while (m.bcStreams.size() < channels) {
      BLEAudioStream s = BLEAudioStreamAccess::create(BLE_AUDIO_STREAM_BROADCAST_SOURCE);
      if (!s) {
        return BTStatus::NoMemory;
      }
      m.bcStreams.push_back(s);
    }
    slots.reserve(channels);
    locations.reserve(channels);
    for (uint8_t i = 0; i < channels; i++) {
      slots.push_back(BLEAudioStreamAccess::slot(m.bcStreams[i]));
      locations.push_back(1u << i);
    }
    m.codecQos(true, c, q);
    ctx = static_cast<uint16_t>(m.context);
    cfgCode = m.code;
    m.bcStarting = true;
  }
  uint8_t bcode[BLE_AUDIO_BCODE_SIZE];
  int err =
    bleAudioCapBroadcastCreate(slots.data(), channels > 1 ? locations.data() : nullptr, channels, &c, &q, ctx, bleAudioPackBroadcastCode(cfgCode, bcode));
  if (err != 0) {
    log_e("CapInitiator: broadcast create failed (%u BIS, err=%d)", channels, err);
    BLELockGuard lock(m.mtx.get());
    m.bcStarting = false;
    return bleAudioStatus(err);
  }
  BTStatus st = m.advertise(advInstance, true);
  if (!st) {
    bleAudioCapBroadcastDelete();
    BLELockGuard lock(m.mtx.get());
    m.bcStarting = false;
    return st;
  }
  const ble_audio_cap_adv_t adv = m.advInfo(advInstance);
  err = bleAudioCapBroadcastStart(&adv);
  if (err != 0) {
    log_e("CapInitiator: BIG creation failed on instance %u (err=%d)", advInstance, err);
    m.broadcastEnded();
    return bleAudioStatus(err);
  }
  BLELockGuard lock(m.mtx.get());
  m.plainBroadcast = true;
  return BTStatus::OK;
}

/** Terminate the BIG, wait for CAP_BC_STOPPED, then drop the source.
 *
 * source_delete is refused while STREAMING (LEA_LIB BapBsrcDeleteInvState[4])
 * and a delete racing an in-flight terminate also fails HCI Terminate BIG
 * (0x0C Command Disallowed). Wait for STOPPED so delete is legal, matching
 * BroadcastSource::stop(). Flags are cleared only when the engine really let
 * the source go; a still-live source returns Fail with the flags retained so
 * a retry (or role end()) can recover instead of wedging the next start.
 */
BTStatus BLEAudioCapInitiator::stopBroadcast() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  if (BLETask::inStackCallbackTask()) {
    log_e("CapInitiator: stopBroadcast() cannot run from a stack callback; defer it to loop()");
    return BTStatus::Busy;
  }
  bool starting;
  BLEAudioStopBudget budget;
  budget.start();
  {
    BLELockGuard lock(_impl->mtx.get());
    if (_impl->advInstance < 0 || _impl->hoBusy) {
      return BTStatus::InvalidState;
    }
    starting = _impl->bcStarting;
  }
  if (starting) {
    _impl->stopSync.take();
    {
      BLELockGuard lock(_impl->mtx.get());
      if (!_impl->bcStarting) {
        _impl->stopSync.give(BTStatus::OK);
      }
    }
    if (_impl->stopSync.wait(budget.remainingMs()) != BTStatus::OK) {
      return budget.stall("CapInitiator", "start-resolve");
    }
  }
  int err = bleAudioCapBroadcastStop();
  if (err != 0 && err != ESP_ERR_INVALID_STATE && err != -EALREADY) {
    log_w("CapInitiator: BIG terminate failed (err=%d)", err);
  } else if (!budget.expired()) {
    (void)_impl->waitStopped(budget.remainingMs());
  }
  // CAP_BC_STOPPED already ran broadcastEnded() on the happy path. A leftover
  // source after a timeout or a stop that never reported STOPPED is dropped
  // here so the next startBroadcast() can recreate it — but only when the
  // engine really let it go (delete is refused while the BIG streams; clearing
  // the flags anyway wedges the next start behind a live source).
  if (bleAudioCapBroadcastExists()) {
    bleAudioCapBroadcastDelete();
  }
  bool leftover;
  {
    BLELockGuard lock(_impl->mtx.get());
    leftover = _impl->bcStarting || _impl->bcStreaming || _impl->advInstance >= 0;
  }
  if (leftover && !bleAudioCapBroadcastExists()) {
    _impl->broadcastEnded();
    return BTStatus::OK;
  }
  if (bleAudioCapBroadcastExists()) {
    log_e("CapInitiator: broadcast source still live; retry stopBroadcast() or end() the role");
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

bool BLEAudioCapInitiator::isBroadcasting() const {
  if (!_impl) {
    return false;
  }
  BLELockGuard lock(_impl->mtx.get());
  return _impl->bcStreaming;
}

#if BLE_AUDIO_CAP_HANDOVER_SUPPORTED
/** The extended advertising runs without periodic data until the engine created the source (HO_CREATED). */
BTStatus BLEAudioCapInitiator::handoverToBroadcast(uint8_t advInstance) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  Impl &m = *_impl;
  {
    BLELockGuard lock(m.mtx.get());
    if (!m.ucStreaming || m.advInstance >= 0 || m.hoBusy) {
      return BTStatus::InvalidState;
    }
    m.hoBusy = true;
  }
  BTStatus st = m.advertise(advInstance, false);
  if (!st) {
    BLELockGuard lock(m.mtx.get());
    m.hoBusy = false;
    return st;
  }
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  m.codecQos(true, c, q);
  const ble_audio_cap_adv_t adv = m.advInfo(advInstance);
  uint8_t bcode[BLE_AUDIO_BCODE_SIZE];
  int err = bleAudioCapHandoverToBroadcast(&adv, &q, bleAudioPackBroadcastCode(m.code, bcode));
  if (err != 0) {
    log_e("CapInitiator: handover to broadcast failed to start (err=%d)", err);
    m.stopAdvertising();
    BLELockGuard lock(m.mtx.get());
    m.hoBusy = false;
    return bleAudioStatus(err);
  }
  return BTStatus::OK;
}

/** The requests name the same sink streams, in the order startUnicast() created them. */
BTStatus BLEAudioCapInitiator::handoverToUnicast() {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  std::vector<ble_audio_uc_stream_req_t> reqs;
  size_t n = 0;
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->handedOver || _impl->hoBusy) {
      return BTStatus::InvalidState;
    }
    reqs = _impl->requests(true);
    for (const auto &s : _impl->ucStreams) {
      if (s.direction() == BLEAudioStream::Direction::Tx && n < reqs.size()) {
        reqs[n++].slot = BLEAudioStreamAccess::slot(s);
      }
    }
    if (reqs.empty() || n != reqs.size()) {
      log_e("CapInitiator: the acceptors changed since the handover to broadcast");
      return BTStatus::InvalidState;
    }
    _impl->hoBusy = true;
  }
  int err = bleAudioCapHandoverToUnicast(reqs.data(), (uint8_t)reqs.size());
  if (err != 0) {
    log_e("CapInitiator: handover to unicast failed to start (err=%d)", err);
    BLELockGuard lock(_impl->mtx.get());
    _impl->hoBusy = false;
    return bleAudioStatus(err);
  }
  return BTStatus::OK;
}
#endif /* BLE_AUDIO_CAP_HANDOVER_SUPPORTED */

size_t BLEAudioCapInitiator::streamCount() const {
  if (!_impl) {
    return 0;
  }
  return _impl->streams().size();
}

BLEAudioStream BLEAudioCapInitiator::stream(size_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  // Snapshot once: two stream()/size() calls would each copy the vector under
  // the lock separately, so a concurrent start could invalidate the reference
  // between them.
  std::vector<BLEAudioStream> snapshot = _impl->streams();
  return index < snapshot.size() ? snapshot[index] : BLEAudioStream();
}

BLEAudioStream BLEAudioCapInitiator::stream(BLEAudioStream::Direction dir, uint8_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  std::vector<BLEAudioStream> snapshot = _impl->streams();
  for (const auto &s : snapshot) {
    if (s.direction() == dir && index-- == 0) {
      return s;
    }
  }
  return BLEAudioStream();
}

void BLEAudioCapInitiator::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = std::move(cb);
}

void BLEAudioCapInitiator::onUnicastStarted(StatusCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.ucStartedCb = std::move(cb);
}

void BLEAudioCapInitiator::onUnicastUpdated(StatusCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.ucUpdatedCb = std::move(cb);
}

void BLEAudioCapInitiator::onUnicastStopped(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.ucStoppedCb = std::move(cb);
}

void BLEAudioCapInitiator::onBroadcastStarted(Callback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.bcStartedCb = std::move(cb);
}

void BLEAudioCapInitiator::onBroadcastStopped(StoppedCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.bcStoppedCb = std::move(cb);
}

void BLEAudioCapInitiator::onHandover(HandoverCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.handoverCb = std::move(cb);
}

void BLEAudioCapInitiator::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = nullptr;
  impl.ucStartedCb = nullptr;
  impl.ucUpdatedCb = nullptr;
  impl.ucStoppedCb = nullptr;
  impl.bcStartedCb = nullptr;
  impl.bcStoppedCb = nullptr;
  impl.handoverCb = nullptr;
}

// --------------------------------------------------------------------------
// Commander
// --------------------------------------------------------------------------

struct BLEAudioCapCommander::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLESharedMutex mtx;             ///< Protects this role's callbacks and shared state.
  std::vector<uint16_t> pending;  ///< discover() called before the link's GATT discovery finished.
  DiscoveredCallback discoveredCb;
  ResultCallback resultCb;
  ReceiveStateCallback recvStateCb;

  /** Start the commander discovery (CAS, then BASS) of @p conn. */
  BTStatus start(uint16_t conn) {
    int err = bleAudioCapCommanderDiscover(conn);
    if (err != 0) {
      log_e("CapCommander: discovery of conn %u failed to start (err=%d)", conn, err);
    }
    return bleAudioStatus(err);
  }

  /**
   * Engine event: link readiness releases pending discoveries (a disconnect
   * drops them); commander events go to the application callbacks.
   */
  void handle(const ble_audio_evt_t &e) {
    switch (e.type) {
      case BLE_AUDIO_EVT_GATT_DISCOVERED:
      case BLE_AUDIO_EVT_ACL_DISCONNECTED:
      {
        bool doStart = false;
        {
          BLELockGuard lock(mtx.get());
          for (auto it = pending.begin(); it != pending.end(); ++it) {
            if (*it == e.conn_handle) {
              pending.erase(it);
              doStart = (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED);
              break;
            }
          }
        }
        if (doStart) {
          BTStatus st = e.err ? bleAudioStatus(e.err) : start(e.conn_handle);
          if (!st) {
            if (auto cb = bleAudioCopyCallback(*this, &Impl::discoveredCb)) {
              cb(st, e.conn_handle, false, false);
            }
          }
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_CMD_DISCOVERED:
      {
        const auto *d = static_cast<const ble_audio_cap_cmd_discovered_t *>(e.data);
        if (auto cb = bleAudioCopyCallback(*this, &Impl::discoveredCb)) {
          cb(bleAudioStatus(e.err), e.conn_handle, d && d->coordinated, d && d->bass);
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_CMD_DONE:
        if (e.data) {
          if (auto cb = bleAudioCopyCallback(*this, &Impl::resultCb)) {
            cb(static_cast<Operation>(static_cast<const ble_audio_cap_cmd_done_t *>(e.data)->op), bleAudioStatus(e.err));
          }
        }
        break;
      case BLE_AUDIO_EVT_CAP_CMD_RECV_STATE:
        if (e.data) {
          const auto *r = static_cast<const ble_audio_cap_recv_state_t *>(e.data);
          const ReceiveState s = {e.conn_handle, r->src_id, r->broadcast_id, r->pa_state, r->big_enc, r->bis_sync};
          if (auto cb = bleAudioCopyCallback(*this, &Impl::recvStateCb)) {
            cb(s);
          }
        }
        break;
      default: break;
    }
  }
};

BLEAudioCapCommander::operator bool() const {
  return _impl != nullptr;
}

BTStatus BLEAudioCapCommander::discover(uint16_t connHandle) {
  if (!_impl || !_impl->audio->started.load() || connHandle == BLE_AUDIO_CONN_NONE) {
    log_e("CapCommander: discover() before audio.begin()");
    return BTStatus::InvalidState;
  }
  {
    BLELockGuard lock(_impl->mtx.get());
    if (!_impl->audio->isGattReady(connHandle)) {
      for (uint16_t c : _impl->pending) {
        if (c == connHandle) {
          return BTStatus::OK;
        }
      }
      _impl->pending.push_back(connHandle);
      return BTStatus::OK;
    }
  }
  return _impl->start(connHandle);
}

BTStatus BLEAudioCapCommander::setCoordinated(bool enable) {
  return _impl ? bleAudioStatus(bleAudioCapCommanderSetCoordinated(enable)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCapCommander::setVolume(uint8_t volume) {
  return _impl ? bleAudioStatus(bleAudioCapCommanderVolume(volume)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCapCommander::setMute(bool mute) {
  return _impl ? bleAudioStatus(bleAudioCapCommanderVolumeMute(mute)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCapCommander::setVolumeOffset(int16_t offset) {
  return _impl ? bleAudioStatus(bleAudioCapCommanderVolumeOffset(offset)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCapCommander::setMicMute(bool mute) {
  return _impl ? bleAudioStatus(bleAudioCapCommanderMicMute(mute)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCapCommander::setMicGain(int8_t gain) {
  return _impl ? bleAudioStatus(bleAudioCapCommanderMicGain(gain)) : BTStatus::InvalidState;
}

BTStatus BLEAudioCapCommander::startBroadcastReception(const BTAddress &source, uint8_t sid, uint32_t broadcastId, uint32_t bisSync, uint16_t paInterval) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  ble_audio_cap_bcast_src_t s = {};
  s.addr_type = bleAudioAddrType(source.type());
  memcpy(s.addr, source.data(), sizeof(s.addr));
  s.sid = sid;
  s.pa_interval = paInterval;
  s.broadcast_id = broadcastId & 0xFFFFFF;
  s.bis_sync = bisSync ? bisSync : 0xFFFFFFFF;
  return bleAudioStatus(bleAudioCapCommanderReceptionStart(&s));
}

BTStatus BLEAudioCapCommander::stopBroadcastReception() {
  return _impl ? bleAudioStatus(bleAudioCapCommanderReceptionStop()) : BTStatus::InvalidState;
}

BTStatus BLEAudioCapCommander::distributeBroadcastCode(const String &code) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  if (!code.length()) {
    log_e("CapCommander: distributeBroadcastCode() needs a non-empty code");
    return BTStatus::InvalidParam;
  }
  uint8_t bcode[BLE_AUDIO_BCODE_SIZE];
  bleAudioPackBroadcastCode(code, bcode);
  return bleAudioStatus(bleAudioCapCommanderDistributeCode(bcode));
}

void BLEAudioCapCommander::onDiscovered(DiscoveredCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = std::move(cb);
}

void BLEAudioCapCommander::onResult(ResultCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.resultCb = std::move(cb);
}

void BLEAudioCapCommander::onReceiveState(ReceiveStateCallback cb) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.recvStateCb = std::move(cb);
}

void BLEAudioCapCommander::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx.get());
  impl.discoveredCb = nullptr;
  impl.resultCb = nullptr;
  impl.recvStateCb = nullptr;
}

// ==========================================================================
// Factories (enabled roles)
// ==========================================================================

namespace {

BTStatus applyCapInitiator() {
  int err = bleAudioCapInitiatorInit();
#if BLE_AUDIO_CAP_HANDOVER_SUPPORTED
  if (err == 0) {
    err = bleAudioCapHandoverInit();
  }
#endif
  return bleAudioStatus(err);
}

}  // namespace

#if BLE_AUDIO_CAP_INITIATOR_SUPPORTED
/** One Impl serves both the CAP_INITIATOR and CAP_HANDOVER groups; the handover route skips core events. */
BLEAudioCapInitiator BLEAudio::createCapInitiator() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioCapInitiator();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("CapInitiator: create before audio.begin() commits");
    return BLEAudioCapInitiator();
  }
  auto i = std::make_shared<BLEAudioCapInitiator::Impl>();
  i->audio = _impl;
  _impl->roles.push_back(i);
  std::weak_ptr<BLEAudioCapInitiator::Impl> weak = i;
  _impl->setHandler(BLE_AUDIO_GRP_CAP_INITIATOR, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
#if BLE_AUDIO_CAP_HANDOVER_SUPPORTED
  _impl->setHandler(BLE_AUDIO_GRP_CAP_HANDOVER, [weak](const ble_audio_evt_t &e) {
    auto s = weak.lock();
    if (s && BLE_AUDIO_EVT_GROUP(e.type) == BLE_AUDIO_GRP_CAP_HANDOVER) {
      s->handle(e);
    }
  });
#endif
  _impl->roleApplies.push_back(applyCapInitiator);
  _impl->roleStops.push_back([weak]() {
    if (auto s = weak.lock()) {
      (void)BLEAudioCapInitiator(s).stopBroadcast();
      s->stopAdvertising();
    }
  });
  return BLEAudioCapInitiator(i);
}
#endif

#if BLE_AUDIO_CAP_COMMANDER_SUPPORTED
BLEAudioCapCommander BLEAudio::createCapCommander() {
  if (ensureActive() != BTStatus::OK) {
    return BLEAudioCapCommander();  // begin() already logged the cause
  }
  if (_impl->started) {
    log_e("CapCommander: create before audio.begin() commits");
    return BLEAudioCapCommander();
  }
  auto c = std::make_shared<BLEAudioCapCommander::Impl>();
  c->audio = _impl;
  _impl->roles.push_back(c);
  std::weak_ptr<BLEAudioCapCommander::Impl> weak = c;
  _impl->setHandler(BLE_AUDIO_GRP_CAP_COMMANDER, [weak](const ble_audio_evt_t &e) {
    if (auto s = weak.lock()) {
      s->handle(e);
    }
  });
  _impl->roleApplies.push_back([]() -> BTStatus {
    return bleAudioStatus(bleAudioCapCommanderInit());
  });
  return BLEAudioCapCommander(c);
}
#endif

// ==========================================================================
// Feature-disabled & capability stubs
// ==========================================================================

#if !BLE_AUDIO_CAP_HANDOVER_SUPPORTED
BTStatus BLEAudioCapInitiator::handoverToBroadcast(uint8_t) {
  return BTStatus::NotSupported;
}

BTStatus BLEAudioCapInitiator::handoverToUnicast() {
  return BTStatus::NotSupported;
}
#endif

#if !BLE_AUDIO_CAP_INITIATOR_SUPPORTED
BLEAudioCapInitiator BLEAudio::createCapInitiator() {
  log_e("CapInitiator: not enabled in this build (CONFIG_BT_CAP_INITIATOR)");
  return BLEAudioCapInitiator();
}
#endif

#if !BLE_AUDIO_CAP_COMMANDER_SUPPORTED
BLEAudioCapCommander BLEAudio::createCapCommander() {
  log_e("CapCommander: not enabled in this build (CONFIG_BT_CAP_COMMANDER)");
  return BLEAudioCapCommander();
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
