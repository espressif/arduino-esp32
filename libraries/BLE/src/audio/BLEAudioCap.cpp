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
#include <vector>
#include "audio/BLEAudioCap.h"
#include "audio/BLEAudioImpl.h"
#include "audio/BLEAudioStreamInternal.h"
#include "audio/BLEAudioEngineCap.h"
#include "BLE.h"
#include "advertising/BLEAdvertising.h"
#include "advertising/BLEAdvertisementData.h"
#include "esp_random.h"
#include "esp32-hal-log.h"

#if BLE_NIMBLE
#include <host/ble_hs.h>
#elif BLE_BLUEDROID
#include "esp_gap_ble_api.h"
#endif

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

constexpr uint16_t kBroadcastAudioAnnouncementUuid = 0x1852;
constexpr uint8_t kAdTypeServiceData16 = 0x16;
constexpr uint16_t kAppearanceBroadcastingDevice = 0x0885;
constexpr uint16_t kPaInterval = 0x40;  ///< 80 ms; also sent to acceptors, so min = max.

/** An acceptor added with addAcceptor(). */
struct Peer {
  BLEAudioCapPeerInfo info;  ///< What discovery found (endpoints, locations, contexts).
  bool discovering = false;  ///< CAP discovery in flight.
  bool discovered = false;   ///< Discovery succeeded; the peer takes part in startUnicast().
};

/** @return The @p n-th set bit of @p mask, or 0 (Mono) when there are fewer bits. */
uint32_t nthLocation(uint32_t mask, uint8_t n) {
  for (uint32_t bit = 1; bit; bit <<= 1) {
    if ((mask & bit) && n-- == 0) {
      return bit;
    }
  }
  return 0;
}

/** @brief Zero-padded 16-octet Broadcast Code, or nullptr when @p code is empty. */
const uint8_t *broadcastCode(const String &code, uint8_t out[BLE_AUDIO_BCODE_SIZE]) {
  memset(out, 0, BLE_AUDIO_BCODE_SIZE);
  const size_t n = code.length() < BLE_AUDIO_BCODE_SIZE ? code.length() : BLE_AUDIO_BCODE_SIZE;
  memcpy(out, code.c_str(), n);
  return n ? out : nullptr;
}

/** @brief Send the SyncInfo of advertising set @p adv to the acceptor on @p conn (service data = source ID). */
void sendPast(uint8_t adv, uint16_t conn, const ble_audio_cap_past_req_t &r) {
  const uint16_t serviceData = (uint16_t)r.src_id << 8;
  int err = -1;
#if BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_PERIODIC_ADV_SYNC_TRANSFER)
  (void)r;
  err = ble_gap_periodic_adv_sync_set_info(adv, conn, serviceData);
#elif BLE_BLUEDROID && defined(CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER)
  (void)conn;
  uint8_t bda[6];
  BTAddress(r.addr, static_cast<BTAddress::Type>(r.addr_type)).toEspBdAddr(bda);
  err = esp_ble_gap_periodic_adv_set_info_trans(bda, serviceData, adv);
#else
  (void)adv;
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

BLEAudioCapAcceptor &BLEAudioCapAcceptor::setSirk(const uint8_t sirk[BLE_AUDIO_SIRK_SIZE]) {
  coordinatedSet().setSirk(sirk);
  return *this;
}

BLEAudioCapAcceptor &BLEAudioCapAcceptor::setSetSize(uint8_t size) {
  coordinatedSet().setSetSize(size);
  return *this;
}

BLEAudioCapAcceptor &BLEAudioCapAcceptor::setRank(uint8_t rank) {
  coordinatedSet().setRank(rank);
  return *this;
}

BLEAudioCapAcceptor &BLEAudioCapAcceptor::setLockable(bool lockable) {
  coordinatedSet().setLockable(lockable);
  return *this;
}

// --------------------------------------------------------------------------
// Initiator
// --------------------------------------------------------------------------

struct BLEAudioCapInitiator::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
  BLEAudioCodecPreset preset = BLEAudioCodecPreset::LC3_16_2_1;
  BLEAudioQos qos;                                 ///< Used instead of the preset QoS when customQos.
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
  bool ucRunning = false;                 ///< Unicast start accepted, not stopped yet.
  bool ucStreaming = false;
  bool bcStreaming = false;
  bool plainBroadcast = false;            ///< startBroadcast() session (bcStreams).
  bool handedOver = false;                ///< The unicast sink streams run as a broadcast.
  bool hoBusy = false;                    ///< Handover procedure in flight.
  int16_t advInstance = -1;               ///< Advertising instance of the broadcast, -1 when none.

  DiscoveredCallback discoveredCb;
  StatusCallback ucStartedCb;
  StatusCallback ucUpdatedCb;
  Callback ucStoppedCb;
  Callback bcStartedCb;
  StoppedCallback bcStoppedCb;
  HandoverCallback handoverCb;

  /** @return the acceptor on @p conn, or nullptr. */
  Peer *peer(uint16_t conn) {
    for (auto &p : peers) {
      if (p.info.connHandle == conn) {
        return &p;
      }
    }
    return nullptr;
  }

  /** @brief Start discovery of @p p; deferred to GATT_DISCOVERED while the link is not ready. */
  BTStatus discover(Peer &p) {
    if (p.discovering || p.discovered || !audio->isGattReady(p.info.connHandle)) {
      return BTStatus::OK;
    }
    p.discovering = true;
    int err = bleAudioCapDiscover(p.info.connHandle);
    if (err != 0) {
      p.discovering = false;
      log_e("CapInitiator: discovery of conn %u failed to start (err=%d)", p.info.connHandle, err);
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
  std::vector<ble_audio_uc_stream_req_t> requests(bool sinkOnly) const {
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
        r.codec.chan_alloc = nthLocation(static_cast<uint32_t>(tx ? p.info.sinkLocation : p.info.sourceLocation), idx);
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
    a.addr_type = own.type() & 1;
    memcpy(a.addr, own.data(), sizeof(a.addr));
    a.pa_interval = kPaInterval;
    a.broadcast_id = broadcastId;
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
    const String advName = name.length() ? name : String(BLE.getDeviceName().c_str());
    BLEAdvertisementData ext(BLEAdvertisementData::EXTENDED_MAX_PAYLOAD);
    ext.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
    ext.setAppearance(kAppearanceBroadcastingDevice);
    const uint8_t id[3] = {(uint8_t)broadcastId, (uint8_t)(broadcastId >> 8), (uint8_t)(broadcastId >> 16)};
    ext.setServiceData(BLEUUID(kBroadcastAudioAnnouncementUuid), id, sizeof(id));
    ext.setBroadcastName(advName);
    ext.setName(advName);
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
    advInstance = inst;
    return BTStatus::OK;
  }

  /** Stop the periodic and extended advertising started by advertise(), if any. */
  void stopAdvertising() {
    if (advInstance < 0) {
      return;
    }
    BLEAdvertising adv = BLE.getAdvertising();
    (void)adv.stopPeriodicAdv((uint8_t)advInstance);
    (void)adv.stopExtended((uint8_t)advInstance);
    advInstance = -1;
  }

  /** @brief Engine source gone: release the advertising and reset the broadcast state. */
  void broadcastEnded() {
    bleAudioCapBroadcastDelete();
    stopAdvertising();
    plainBroadcast = false;
    handedOver = false;
    bcStreaming = false;
  }

  /**
   * Engine event (core and initiator/handover groups): runs deferred
   * discoveries, tracks acceptors, and maps procedure completions to the
   * application callbacks.
   */
  void handle(const ble_audio_evt_t &e) {
    switch (e.type) {
      case BLE_AUDIO_EVT_GATT_DISCOVERED:
        if (Peer *p = peer(e.conn_handle)) {
          (void)discover(*p);
        }
        break;
      case BLE_AUDIO_EVT_ACL_DISCONNECTED:
        for (auto it = peers.begin(); it != peers.end(); ++it) {
          if (it->info.connHandle == e.conn_handle) {
            peers.erase(it);
            break;
          }
        }
        break;
      case BLE_AUDIO_EVT_CAP_DISCOVERED: {
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
          log_i("CapInitiator: conn %u has %u sink / %u source ASEs%s", e.conn_handle, d->eps.sink_eps, d->eps.source_eps,
                d->coordinated ? " (set member)" : "");
        } else {
          log_w("CapInitiator: discovery of conn %u failed (err=%d)", e.conn_handle, e.err);
        }
        const BLEAudioCapPeerInfo info = p->info;
        if (discoveredCb) {
          discoveredCb(bleAudioStatus(e.err), info);
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_UC_STARTED:
        ucStreaming = (e.err == 0);
        if (e.err != 0) {
          log_w("CapInitiator: unicast start failed (conn %u, err=%d)", e.conn_handle, e.err);
        }
        if (ucStartedCb) {
          ucStartedCb(bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_CAP_UC_UPDATED:
        if (ucUpdatedCb) {
          ucUpdatedCb(bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_CAP_UC_STOPPED:
        ucRunning = false;
        ucStreaming = false;
        if (ucStoppedCb) {
          ucStoppedCb();
        }
        break;
      case BLE_AUDIO_EVT_CAP_BC_STARTED:
        bcStreaming = true;
        log_i("CapInitiator: broadcast 0x%06lX streaming", (unsigned long)broadcastId);
        if (bcStartedCb) {
          bcStartedCb();
        }
        break;
      case BLE_AUDIO_EVT_CAP_BC_STOPPED:
        if (hoBusy) {
          bcStreaming = false;
          break;
        }
        broadcastEnded();
        if (bcStoppedCb) {
          bcStoppedCb((uint8_t)e.err);
        }
        break;
      case BLE_AUDIO_EVT_CAP_PAST_REQ:
        if (advInstance >= 0 && e.data) {
          sendPast((uint8_t)advInstance, e.conn_handle, *static_cast<const ble_audio_cap_past_req_t *>(e.data));
        }
        break;
      case BLE_AUDIO_EVT_CAP_HO_CREATED: {
        BTStatus st = advInstance >= 0 ? publishBase((uint8_t)advInstance) : BTStatus::InvalidState;
        if (st) {
          st = BLE.getAdvertising().startPeriodicAdv((uint8_t)advInstance);
        }
        if (!st) {
          log_e("CapInitiator: periodic advertising for the handover failed (%s)", st.toString());
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_HO_TO_BROADCAST:
        hoBusy = false;
        if (e.err == 0) {
          ucRunning = false;
          ucStreaming = false;
          handedOver = true;
          log_i("CapInitiator: unicast handed over to broadcast 0x%06lX", (unsigned long)broadcastId);
        } else {
          log_w("CapInitiator: handover to broadcast failed (err=%d)", e.err);
          if (bleAudioCapBroadcastStop() != 0) {
            broadcastEnded();
          }
          stopAdvertising();
        }
        if (handoverCb) {
          handoverCb(bleAudioStatus(e.err), true);
        }
        break;
      case BLE_AUDIO_EVT_CAP_HO_TO_UNICAST:
        hoBusy = false;
        if (e.err == 0) {
          stopAdvertising();
          handedOver = false;
          bcStreaming = false;
          ucRunning = true;
          ucStreaming = true;
          log_i("CapInitiator: broadcast handed back to unicast");
        } else {
          log_w("CapInitiator: handover to unicast failed (err=%d)", e.err);
        }
        if (handoverCb) {
          handoverCb(bleAudioStatus(e.err), false);
        }
        break;
      default: break;
    }
  }

  const std::vector<BLEAudioStream> &streams() const {
    return plainBroadcast ? bcStreams : ucStreams;
  }
};

BLEAudioCapInitiator::operator bool() const {
  return _impl != nullptr;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setPreset(BLEAudioCodecPreset preset) {
  if (_impl) {
    _impl->preset = preset;
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setQos(const BLEAudioQos &qos) {
  if (_impl) {
    _impl->qos = qos;
    _impl->customQos = true;
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setContext(BLEAudioContext context) {
  if (_impl) {
    _impl->context = context;
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setSinkStreams(uint8_t perPeer) {
  if (_impl) {
    _impl->sinkPerPeer = perPeer;
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setSourceStreams(uint8_t perPeer) {
  if (_impl) {
    _impl->sourcePerPeer = perPeer;
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setBroadcastId(uint32_t broadcastId) {
  if (_impl) {
    _impl->broadcastId = broadcastId & 0xFFFFFF;
  }
  return *this;
}

uint32_t BLEAudioCapInitiator::getBroadcastId() const {
  return _impl ? _impl->broadcastId : 0;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setBroadcastCode(const String &code) {
  if (_impl) {
    if (code.length() > BLE_AUDIO_BCODE_SIZE) {
      log_w("CapInitiator: Broadcast Code truncated to %u characters", BLE_AUDIO_BCODE_SIZE);
    }
    _impl->code = code;
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::setBroadcastName(const String &name) {
  if (_impl) {
    _impl->name = name;
  }
  return *this;
}

BTStatus BLEAudioCapInitiator::discover(uint16_t connHandle) {
  if (!_impl || !_impl->audio->started) {
    log_e("CapInitiator: discover() before audio.start()");
    return BTStatus::InvalidState;
  }
  Peer *p = _impl->peer(connHandle);
  if (!p) {
    Peer np;
    np.info.connHandle = connHandle;
    _impl->peers.push_back(np);
    p = &_impl->peers.back();
  }
  return _impl->discover(*p);
}

/** Stream handles are reused while the request count is unchanged, so attached callbacks survive a restart. */
BTStatus BLEAudioCapInitiator::startUnicast() {
  if (!_impl || !_impl->audio->started || _impl->ucRunning || _impl->handedOver || _impl->hoBusy) {
    return BTStatus::InvalidState;
  }
  std::vector<ble_audio_uc_stream_req_t> reqs = _impl->requests(false);
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
  int err = bleAudioCapUnicastStart(reqs.data(), (uint8_t)reqs.size());
  if (err != 0) {
    log_e("CapInitiator: unicast start failed (%u streams, err=%d)", (unsigned)reqs.size(), err);
    return bleAudioStatus(err);
  }
  _impl->ucRunning = true;
  return BTStatus::OK;
}

BTStatus BLEAudioCapInitiator::updateUnicast(BLEAudioContext context) {
  if (!_impl || !_impl->ucRunning) {
    return BTStatus::InvalidState;
  }
  _impl->context = context;
  return bleAudioStatus(bleAudioCapUnicastUpdate(static_cast<uint16_t>(context)));
}

BTStatus BLEAudioCapInitiator::stopUnicast() {
  if (!_impl || !_impl->ucRunning || _impl->hoBusy) {
    return BTStatus::InvalidState;
  }
  int err = bleAudioCapUnicastStop();
  if (err != 0) {
    log_e("CapInitiator: unicast stop failed (err=%d)", err);
  }
  return bleAudioStatus(err);
}

bool BLEAudioCapInitiator::isUnicastStreaming() const {
  return _impl && _impl->ucStreaming;
}

/** Order: source (BASE) first, then advertising (needs the BASE), then the BIG (needs the train). */
BTStatus BLEAudioCapInitiator::startBroadcast(uint8_t channels, uint8_t advInstance) {
  if (!_impl || !_impl->audio->started) {
    log_e("CapInitiator: startBroadcast() before audio.start()");
    return BTStatus::InvalidState;
  }
  Impl &m = *_impl;
  if (m.advInstance >= 0 || m.hoBusy) {
    log_w("CapInitiator: a broadcast is already running");
    return BTStatus::InvalidState;
  }
  if (channels == 0) {
    channels = 1;
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
  std::vector<ble_audio_slot_t *> slots;
  std::vector<uint32_t> locations;
  for (uint8_t i = 0; i < channels; i++) {
    slots.push_back(BLEAudioStreamAccess::slot(m.bcStreams[i]));
    locations.push_back(1u << i);
  }
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  m.codecQos(true, c, q);
  uint8_t bcode[BLE_AUDIO_BCODE_SIZE];
  int err = bleAudioCapBroadcastCreate(slots.data(), channels > 1 ? locations.data() : nullptr, channels, &c, &q,
                                       static_cast<uint16_t>(m.context), broadcastCode(m.code, bcode));
  if (err != 0) {
    log_e("CapInitiator: broadcast create failed (%u BIS, err=%d)", channels, err);
    return bleAudioStatus(err);
  }
  BTStatus st = m.advertise(advInstance, true);
  if (!st) {
    bleAudioCapBroadcastDelete();
    return st;
  }
  const ble_audio_cap_adv_t adv = m.advInfo(advInstance);
  err = bleAudioCapBroadcastStart(&adv);
  if (err != 0) {
    log_e("CapInitiator: BIG creation failed on instance %u (err=%d)", advInstance, err);
    m.broadcastEnded();
    return bleAudioStatus(err);
  }
  m.plainBroadcast = true;
  return BTStatus::OK;
}

/** Without a stop the engine never reports BC_STOPPED, so a failed stop releases everything here. */
BTStatus BLEAudioCapInitiator::stopBroadcast() {
  if (!_impl || _impl->advInstance < 0 || _impl->hoBusy) {
    return BTStatus::InvalidState;
  }
  int err = bleAudioCapBroadcastStop();
  _impl->stopAdvertising();
  if (err != 0) {
    log_w("CapInitiator: BIG terminate failed (err=%d)", err);
    _impl->broadcastEnded();
  }
  return bleAudioStatus(err);
}

bool BLEAudioCapInitiator::isBroadcasting() const {
  return _impl && _impl->bcStreaming;
}

/** The extended advertising runs without periodic data until the engine created the source (HO_CREATED). */
BTStatus BLEAudioCapInitiator::handoverToBroadcast(uint8_t advInstance) {
#if BLE_AUDIO_CAP_HANDOVER_SUPPORTED
  if (!_impl || !_impl->ucStreaming || _impl->advInstance >= 0 || _impl->hoBusy) {
    return BTStatus::InvalidState;
  }
  Impl &m = *_impl;
  BTStatus st = m.advertise(advInstance, false);
  if (!st) {
    return st;
  }
  ble_audio_codec_t c;
  ble_audio_qos_t q;
  bleAudioPresetGet(static_cast<uint8_t>(m.preset), true, &c, &q);
  const ble_audio_cap_adv_t adv = m.advInfo(advInstance);
  uint8_t bcode[BLE_AUDIO_BCODE_SIZE];
  int err = bleAudioCapHandoverToBroadcast(&adv, &q, broadcastCode(m.code, bcode));
  if (err != 0) {
    log_e("CapInitiator: handover to broadcast failed to start (err=%d)", err);
    m.stopAdvertising();
    return bleAudioStatus(err);
  }
  m.hoBusy = true;
  return BTStatus::OK;
#else
  (void)advInstance;
  return BTStatus::NotSupported;
#endif
}

/** The requests name the same sink streams, in the order startUnicast() created them. */
BTStatus BLEAudioCapInitiator::handoverToUnicast() {
#if BLE_AUDIO_CAP_HANDOVER_SUPPORTED
  if (!_impl || !_impl->handedOver || _impl->hoBusy) {
    return BTStatus::InvalidState;
  }
  std::vector<ble_audio_uc_stream_req_t> reqs = _impl->requests(true);
  size_t n = 0;
  for (const auto &s : _impl->ucStreams) {
    if (s.direction() == BLEAudioStream::Direction::Tx && n < reqs.size()) {
      reqs[n++].slot = BLEAudioStreamAccess::slot(s);
    }
  }
  if (reqs.empty() || n != reqs.size()) {
    log_e("CapInitiator: the acceptors changed since the handover to broadcast");
    return BTStatus::InvalidState;
  }
  int err = bleAudioCapHandoverToUnicast(reqs.data(), (uint8_t)reqs.size());
  if (err != 0) {
    log_e("CapInitiator: handover to unicast failed to start (err=%d)", err);
    return bleAudioStatus(err);
  }
  _impl->hoBusy = true;
  return BTStatus::OK;
#else
  return BTStatus::NotSupported;
#endif
}

size_t BLEAudioCapInitiator::streamCount() const {
  return _impl ? _impl->streams().size() : 0;
}

BLEAudioStream BLEAudioCapInitiator::stream(size_t index) const {
  return (_impl && index < _impl->streams().size()) ? _impl->streams()[index] : BLEAudioStream();
}

BLEAudioStream BLEAudioCapInitiator::stream(BLEAudioStream::Direction dir, uint8_t index) const {
  if (!_impl) {
    return BLEAudioStream();
  }
  for (const auto &s : _impl->streams()) {
    if (s.direction() == dir && index-- == 0) {
      return s;
    }
  }
  return BLEAudioStream();
}

BLEAudioCapInitiator &BLEAudioCapInitiator::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::onUnicastStarted(StatusCallback cb) {
  if (_impl) {
    _impl->ucStartedCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::onUnicastUpdated(StatusCallback cb) {
  if (_impl) {
    _impl->ucUpdatedCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::onUnicastStopped(Callback cb) {
  if (_impl) {
    _impl->ucStoppedCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::onBroadcastStarted(Callback cb) {
  if (_impl) {
    _impl->bcStartedCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::onBroadcastStopped(StoppedCallback cb) {
  if (_impl) {
    _impl->bcStoppedCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapInitiator &BLEAudioCapInitiator::onHandover(HandoverCallback cb) {
  if (_impl) {
    _impl->handoverCb = std::move(cb);
  }
  return *this;
}

void BLEAudioCapInitiator::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->ucStartedCb = nullptr;
    _impl->ucUpdatedCb = nullptr;
    _impl->ucStoppedCb = nullptr;
    _impl->bcStartedCb = nullptr;
    _impl->bcStoppedCb = nullptr;
    _impl->handoverCb = nullptr;
  }
}

// --------------------------------------------------------------------------
// Commander
// --------------------------------------------------------------------------

struct BLEAudioCapCommander::Impl {
  std::shared_ptr<BLEAudio::Impl> audio;
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
        for (auto it = pending.begin(); it != pending.end(); ++it) {
          if (*it == e.conn_handle) {
            pending.erase(it);
            if (e.type == BLE_AUDIO_EVT_GATT_DISCOVERED) {
              (void)start(e.conn_handle);
            }
            break;
          }
        }
        break;
      case BLE_AUDIO_EVT_CAP_CMD_DISCOVERED: {
        const auto *d = static_cast<const ble_audio_cap_cmd_discovered_t *>(e.data);
        if (discoveredCb) {
          discoveredCb(bleAudioStatus(e.err), e.conn_handle, d && d->coordinated, d && d->bass);
        }
        break;
      }
      case BLE_AUDIO_EVT_CAP_CMD_DONE:
        if (resultCb && e.data) {
          resultCb(static_cast<Operation>(static_cast<const ble_audio_cap_cmd_done_t *>(e.data)->op), bleAudioStatus(e.err));
        }
        break;
      case BLE_AUDIO_EVT_CAP_CMD_RECV_STATE:
        if (recvStateCb && e.data) {
          const auto *r = static_cast<const ble_audio_cap_recv_state_t *>(e.data);
          const ReceiveState s = {e.conn_handle, r->src_id, r->broadcast_id, r->pa_state, r->big_enc, r->bis_sync};
          recvStateCb(s);
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
  if (!_impl || !_impl->audio->started) {
    log_e("CapCommander: discover() before audio.start()");
    return BTStatus::InvalidState;
  }
  if (_impl->audio->isGattReady(connHandle)) {
    return _impl->start(connHandle);
  }
  for (uint16_t c : _impl->pending) {
    if (c == connHandle) {
      return BTStatus::OK;
    }
  }
  _impl->pending.push_back(connHandle);
  return BTStatus::OK;
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

BTStatus BLEAudioCapCommander::startBroadcastReception(const BTAddress &source, uint8_t sid, uint32_t broadcastId, uint32_t bisSync,
                                                       uint16_t paInterval) {
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  ble_audio_cap_bcast_src_t s = {};
  s.addr_type = source.type() & 1;
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
  uint8_t bcode[BLE_AUDIO_BCODE_SIZE];
  broadcastCode(code, bcode);
  return bleAudioStatus(bleAudioCapCommanderDistributeCode(bcode));
}

BLEAudioCapCommander &BLEAudioCapCommander::onDiscovered(DiscoveredCallback cb) {
  if (_impl) {
    _impl->discoveredCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapCommander &BLEAudioCapCommander::onResult(ResultCallback cb) {
  if (_impl) {
    _impl->resultCb = std::move(cb);
  }
  return *this;
}

BLEAudioCapCommander &BLEAudioCapCommander::onReceiveState(ReceiveStateCallback cb) {
  if (_impl) {
    _impl->recvStateCb = std::move(cb);
  }
  return *this;
}

void BLEAudioCapCommander::resetCallbacks() {
  if (_impl) {
    _impl->discoveredCb = nullptr;
    _impl->resultCb = nullptr;
    _impl->recvStateCb = nullptr;
  }
}

// --------------------------------------------------------------------------
// Factories
// --------------------------------------------------------------------------

/** One Impl serves both the CAP_INITIATOR and CAP_HANDOVER groups; the handover route skips core events. */
BLEAudioCapInitiator BLEAudio::createCapInitiator() {
#if BLE_AUDIO_CAP_INITIATOR_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("CapInitiator: create between audio.begin() and audio.start()");
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
  _impl->roleApplies.push_back([]() -> BTStatus {
    int err = bleAudioCapInitiatorInit();
#if BLE_AUDIO_CAP_HANDOVER_SUPPORTED
    if (err == 0) {
      err = bleAudioCapHandoverInit();
    }
#endif
    return bleAudioStatus(err);
  });
  return BLEAudioCapInitiator(i);
#else
  log_e("CapInitiator: not enabled in this build (CONFIG_BT_CAP_INITIATOR)");
  return BLEAudioCapInitiator();
#endif
}

BLEAudioCapCommander BLEAudio::createCapCommander() {
#if BLE_AUDIO_CAP_COMMANDER_SUPPORTED
  if (!_impl || !_impl->active || _impl->started) {
    log_e("CapCommander: create between audio.begin() and audio.start()");
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
#else
  log_e("CapCommander: not enabled in this build (CONFIG_BT_CAP_COMMANDER)");
  return BLEAudioCapCommander();
#endif
}

#endif /* BLE_AUDIO_SUPPORTED */
