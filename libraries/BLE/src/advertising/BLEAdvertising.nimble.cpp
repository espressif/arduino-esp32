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
 * @file BLEAdvertising.nimble.cpp
 * @brief NimBLE implementation of BLE GAP advertising and advertising completion callbacks.
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part C (GAP) — advertising and discovery procedures.
 *  - BT Core Spec v5.x, Vol 6, Part B, §2.3.1 — LE advertising PDU types
 *    (ADV_IND, ADV_DIRECT_IND, ADV_NONCONN_IND, ADV_SCAN_IND, SCAN_RSP).
 *  - BT Core Spec v5.x, Vol 6, Part B, §4.4.2 — advertising interval timing:
 *    unit = 0.625 ms; minimum = 0x0020 (20 ms); maximum = 0x4000 (10.24 s).
 *  - CSS (Supplement to Core Spec), Part A — AD type codes and structures.
 *    §1.3: Flags (0x01); §1.2: Local Name (0x08/0x09).
 *  - BT Core Spec v5.x, Vol 3, Part C, §11 — AD structure format and 31-byte limit
 *    for legacy advertising PDUs.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"
#include "BLE.nimble.h"
#include "advertising/BLEAdvertising.nimble.h"

#include "core/BLEImplHelpers.h"
#include "advertising/BLEAdvScanHelpers.h"
#include "advertising/BLEAdvertisingData.h"
#include "esp32-hal-log.h"

#if BLE_ADVERTISING_SUPPORTED

#include <host/ble_hs.h>
#include <host/ble_gap.h>
#if BLE_GATT_SERVER_SUPPORTED
void nimbleCancelPendingReAdvertise();
void nimbleInvalidatePendingReAdvertise();
SemaphoreHandle_t nimbleReAdvertiseMutex();

// Taken before nativeAdmission so stop() can wait out an in-flight
// advertiseOnDisconnect start() without inverting the admission gate.
// Public start()/stop() cancel a queued restart; preemption resume only
// takes the mutex so stop() can wait it out without dropping that restart.
struct ReAdvGuard {
  BLELockGuard lock;
  explicit ReAdvGuard(bool invalidatePending = true) : lock(nimbleReAdvertiseMutex()) {
    if (invalidatePending) {
      nimbleInvalidatePendingReAdvertise();
    }
  }
};
#endif
#include <esp_timer.h>
#include <algorithm>
#include <cstring>

#include "core/BLEValidationHooks.h"
#if BLE_VALIDATION_HOOKS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace {
uint32_t remainingMsFromDeadline(int64_t deadlineUs, bool &expired) {
  if (deadlineUs == 0) {
    expired = false;
    return 0;
  }
  int64_t remainingUs = deadlineUs - esp_timer_get_time();
  expired = remainingUs <= 0;
  if (expired) {
    return 0;
  }
  return static_cast<uint32_t>((remainingUs + 999) / 1000);
}

ble_addr_t toNimblePeerAddr(const BTAddress &addr) {
  ble_addr_t peer{};
  peer.type = static_cast<uint8_t>(addr.type());
  memcpy(peer.val, addr.data(), 6);
  return peer;
}
}  // namespace

/**
 * @brief NimBLE GAP callback for advertising: completion, connect, and server forwarding.
 * @param event GAP event from NimBLE.
 * @param arg User pointer to BLEAdvertising::Impl.
 * @return 0 for handled; may forward to the server for connect events.
 */
int BLEAdvertising::Impl::gapEventCallback(struct ble_gap_event *event, void *arg) {
  auto *impl = static_cast<BLEAdvertising::Impl *>(arg);
  if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) {
#if BLE5_SUPPORTED
    uint8_t inst = event->adv_complete.instance;
    const bool isLegacy = (inst == kLegacyInstance);
    // A late complete from a previous stop must not clear a session that has
    // already been restarted (ble_gap_ext_adv_active is the native truth).
    if (isLegacy ? ble_gap_ext_adv_active(kLegacyInstance) : (inst < kMaxExtInstances && ble_gap_ext_adv_active(inst))) {
      return 0;
    }
    bool wasAdvertising = false;
    if (isLegacy) {
      wasAdvertising = impl->isAdvertising.load();
      impl->isAdvertising.store(false);
    } else if (inst < kMaxExtInstances) {
      wasAdvertising = impl->extInstances[inst].isAdvertising.load();
      impl->extInstances[inst].isAdvertising.store(false);
    }
#else
    const bool isLegacy = true;
    if (ble_gap_adv_active()) {
      return 0;
    }
    bool wasAdvertising = impl->isAdvertising.load();
    impl->isAdvertising.store(false);
#endif
    // NimBLE preempts (stops) advertising internally for some operations --
    // e.g. ble_gap_unpair() removes the peer IRK from the controller
    // resolving list, which cannot run while advertising is active
    // (ble_gap_preempt/ble_gap_preempt_done in ble_gap.c). It then reports
    // ADV_COMPLETE with reason BLE_HS_EPREEMPTED and expects the application
    // to resume advertising. This library owns the advertising state, so it
    // resumes a set that was active when preempted; otherwise a bond
    // operation would silently end advertising for good.
#if BLE_VALIDATION_HOOKS
    // Only consume the hook on a complete that could actually be a preemption
    // of a live set. A leftover complete from an earlier stop() has
    // wasAdvertising == false and must not steal the next test's arming.
    const bool forcePreempted = wasAdvertising && BLEHooks.adv.forcePreemptedComplete.exchange(false, std::memory_order_acq_rel);
#else
    const bool forcePreempted = false;
#endif
    const bool preempted = (forcePreempted || event->adv_complete.reason == BLE_HS_EPREEMPTED) && wasAdvertising && BLE.isInitialized();
    BLEAdvertising::CompleteHandler cb;
    {
      BLELockGuard lock(impl->mtx);
      cb = impl->onCompleteCb;
    }
    // A preempted completion is internal to the library (the session is
    // resumed below with its original limits), not the end of the user's
    // session: do not fire onComplete for it, or handlers that treat
    // completion as terminal would race the resume.
    if (preempted) {
      cb = nullptr;
    }
    if (cb) {
      // Legacy advertising reports instance 0 (even when routed over the
      // reserved kLegacyInstance on BLE5 builds); extended advertising reports
      // the set that completed (BT Core Spec v5.x, Vol 4, Part E, §7.7.65.18).
#if BLE5_SUPPORTED
      cb(isLegacy ? 0 : inst);
#else
      cb(0);
#endif
    }
    if (preempted) {
      // Resume the preempted set with the time left on the original deadline,
      // matching BLEScan. Take the restart mutex before the session recheck so a
      // concurrent public stop() either bumps advSession first (resume aborts)
      // or waits until this resume finishes and then stops the radio. Do not
      // invalidate a queued advertiseOnDisconnect restart: if this session's
      // deadline has expired, that restart is what brings advertising back.
      // Do not call public start()/startExtended(): those fetch_add a new
      // session and would undo stop().
      const uint32_t session = impl->advSession.load(std::memory_order_relaxed);
#if BLE_VALIDATION_HOOKS
      BLEHooks.adv.preemptResumeEntered.store(true, std::memory_order_release);
      BLEValidationHooks::holdWhile(BLEHooks.adv.holdPreemptResume);
#endif
#if BLE_GATT_SERVER_SUPPORTED
      ReAdvGuard reAdv(false);
#endif
      if (impl->advSession.load(std::memory_order_relaxed) != session) {
        return 0;
      }
      bool expired = false;
      uint32_t remainingMs = 0;
#if BLE5_SUPPORTED
      remainingMs =
        remainingMsFromDeadline(isLegacy ? impl->sessionDeadlineUs : (inst < kMaxExtInstances ? impl->extInstances[inst].sessionDeadlineUs : 0), expired);
#else
      remainingMs = remainingMsFromDeadline(impl->sessionDeadlineUs, expired);
#endif
      if (expired) {
        return 0;
      }
#if BLE_VALIDATION_HOOKS
      BLEHooks.adv.resumeDurationMs.store(remainingMs, std::memory_order_relaxed);
#endif
      BTStatus resumeSt = BTStatus::Fail;
#if BLE5_SUPPORTED
      if (isLegacy) {
        impl->legacyDurationMs = remainingMs;
        impl->sessionDeadlineUs = remainingMs == 0 ? 0 : esp_timer_get_time() + static_cast<int64_t>(remainingMs) * 1000;
        impl->isAdvertising = true;
        int rc = impl->startLegacyViaExtAdv(remainingMs);
        if (rc != 0) {
          impl->isAdvertising = false;
          resumeSt = BTStatus::Fail;
        } else {
          resumeSt = BTStatus::OK;
        }
      } else if (inst < kMaxExtInstances) {
        auto *extInst = &impl->extInstances[inst];
        extInst->durationMs = remainingMs;
        extInst->sessionDeadlineUs = remainingMs == 0 ? 0 : esp_timer_get_time() + static_cast<int64_t>(remainingMs) * 1000;
        extInst->isAdvertising = true;
        int duration = static_cast<int>(bleGapDurationMsTo10ms(remainingMs));
        int rc = ble_gap_ext_adv_start(inst, duration, extInst->maxEvents);
        if (rc != 0) {
          extInst->isAdvertising = false;
          resumeSt = BTStatus::Fail;
        } else {
          resumeSt = BTStatus::OK;
        }
      }
#else
      impl->legacyDurationMs = remainingMs;
      impl->sessionDeadlineUs = remainingMs == 0 ? 0 : esp_timer_get_time() + static_cast<int64_t>(remainingMs) * 1000;
      impl->isAdvertising = true;
      struct ble_gap_adv_params advParams = {};
      advParams.itvl_min = impl->minInterval;
      advParams.itvl_max = impl->maxInterval;
      advParams.filter_policy = impl->filterPolicy;
      switch (impl->advType) {
        case BLEAdvType::ConnectableScannable:
          advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
          advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
          break;
        case BLEAdvType::ConnectableDirected:
        case BLEAdvType::DirectedLowDuty:
          advParams.conn_mode = BLE_GAP_CONN_MODE_DIR;
          advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
          break;
        case BLEAdvType::DirectedHighDuty:
          advParams.conn_mode = BLE_GAP_CONN_MODE_DIR;
          advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
          advParams.high_duty_cycle = 1;
          break;
        case BLEAdvType::ScannableUndirected:
          advParams.conn_mode = BLE_GAP_CONN_MODE_NON;
          advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
          break;
        case BLEAdvType::NonConnectable:
          advParams.conn_mode = BLE_GAP_CONN_MODE_NON;
          advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
          break;
      }
      int32_t duration = (remainingMs == 0) ? BLE_HS_FOREVER : static_cast<int32_t>(remainingMs);
      ble_addr_t peerAddr = toNimblePeerAddr(impl->directedPeer);
      const ble_addr_t *directAddr = bleAdvTypeIsDirected(impl->advType) ? &peerAddr : nullptr;
      int rc = ble_gap_adv_start(static_cast<uint8_t>(BLE.getOwnAddressType()), directAddr, duration, &advParams, BLEAdvertising::Impl::gapEventCallback, impl);
      if (rc != 0) {
        impl->isAdvertising = false;
        resumeSt = BTStatus::Fail;
      } else {
        resumeSt = BTStatus::OK;
      }
#endif
      if (!resumeSt) {
        log_e("Advertising: preempt resume failed (%s)", resumeSt.toString());
      }
    }
    return 0;
  }

  if (event->type == BLE_GAP_EVENT_CONNECT) {
#if BLE5_SUPPORTED
    impl->isAdvertising = ble_gap_ext_adv_active(kLegacyInstance) != 0;
    for (uint8_t i = 0; i < kLegacyInstance; i++) {
      impl->extInstances[i].isAdvertising = ble_gap_ext_adv_active(i) != 0;
    }
#else
    impl->isAdvertising = false;
#endif
  }

  return nimbleForwardServerGapEvent(event);
}

// --------------------------------------------------------------------------
// BLEAdvertising public API (stack-specific)
// --------------------------------------------------------------------------

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

void BLEAdvertising::setName(const String &name) {
  BLE_CHECK_IMPL();
  impl.deviceName = name;
}

void BLEAdvertising::setScanResponse(bool enable) {
  BLE_CHECK_IMPL();
  impl.scanResponseEnabled = enable;
}

void BLEAdvertising::setType(BLEAdvType type) {
  BLE_CHECK_IMPL();
  impl.advType = type;
}

void BLEAdvertising::setInterval(uint16_t minMs, uint16_t maxMs) {
  BLE_CHECK_IMPL();
  impl.minInterval = bleMsToUnits0625(minMs);
  impl.maxInterval = bleMsToUnits0625(maxMs);
}

void BLEAdvertising::setMinPreferred(uint16_t v) {
  BLE_CHECK_IMPL();
  impl.minPreferred = v;
}

void BLEAdvertising::setMaxPreferred(uint16_t v) {
  BLE_CHECK_IMPL();
  impl.maxPreferred = v;
}

void BLEAdvertising::setTxPower(bool include) {
  BLE_CHECK_IMPL();
  impl.includeTxPower = include;
}

void BLEAdvertising::setAppearance(uint16_t appearance) {
  BLE_CHECK_IMPL();
  impl.appearance = appearance;
}

void BLEAdvertising::setScanFilter(bool scanWl, bool connWl) {
  BLE_CHECK_IMPL();
  impl.filterPolicy = 0;
  if (scanWl) {
    impl.filterPolicy |= 0x01;
  }
  if (connWl) {
    impl.filterPolicy |= 0x02;
  }
}

void BLEAdvertising::reset() {
  BLE_CHECK_IMPL();
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (admission) {
    (void)stopBackend();
  } else {
    impl.advSession.fetch_add(1, std::memory_order_relaxed);
  }
  impl.serviceUUIDs.clear();
  impl.deviceName = "";
  impl.useCustomAdvData = false;
  impl.useCustomScanRsp = false;
  impl.customAdvData.clear();
  impl.customScanRspData.clear();
  impl.appearance = 0;
  impl.includeTxPower = false;
  impl.minInterval = 0x20;
  impl.maxInterval = 0x40;
  impl.minPreferred = 0;
  impl.maxPreferred = 0;
  impl.advType = BLEAdvType::ConnectableScannable;
  impl.directedPeer = BTAddress();
  impl.scanResponseEnabled = true;
  impl.filterPolicy = 0;
#if BLE5_SUPPORTED
  if (admission) {
    // Stop every instance first: ble_gap_ext_adv_clear() returns BLE_HS_EBUSY
    // if any instance still has an active advertising op.
    for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
#if BLE_PERIODIC_ADV_SUPPORTED
      (void)ble_gap_periodic_adv_stop(i);
#endif
      (void)ble_gap_ext_adv_stop(i);
    }
    (void)ble_gap_ext_adv_clear();
  }
  for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
    impl.extInstances[i].reset();
  }
#endif
  // Closed-gate reset() must still clear software flags (DESIGN.md lifecycle),
  // including BLE4.2 where there is no extended-instance block.
  impl.isAdvertising = false;
}

#if BLE_VALIDATION_HOOKS
static void bleAdvTestDeliverPreemptedComplete(void *arg);
static void nimbleInjectPreemptedCompleteIfArmed(BLEAdvertising::Impl *impl);
#endif

void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.customAdvData = data;
  impl.useCustomAdvData = true;
}

void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.customScanRspData = data;
  impl.useCustomScanRsp = true;
}

BTStatus BLEAdvertising::start(uint32_t durationMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  if (!bleGapDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if (impl.advType == BLEAdvType::DirectedHighDuty && !bleHighDutyDirectedDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if (bleAdvTypeIsDirected(impl.advType) && impl.directedPeer.isZero()) {
    return BTStatus::InvalidParam;
  }
  if (impl.isAdvertising.load()) {
    (void)stopBackend();
  }
  impl.legacyDurationMs = durationMs;
  impl.sessionDeadlineUs = durationMs == 0 ? 0 : esp_timer_get_time() + static_cast<int64_t>(durationMs) * 1000;
  impl.advSession.fetch_add(1, std::memory_order_relaxed);

#if BLE5_SUPPORTED
  // On ext-adv-enabled controllers the native-legacy ble_gap_adv_* path does not
  // radiate; run the public "legacy" advertising over the extended API using a
  // legacy PDU on the reserved instance. Set isAdvertising before the native
  // start so a synchronous ADV_COMPLETE cannot be overwritten by a post-return
  // assignment.
  impl.isAdvertising = true;
  int rc = impl.startLegacyViaExtAdv(durationMs);
#if BLE_VALIDATION_HOOKS
  BLEHooks.adv.lastNativeStartRc.store(rc, std::memory_order_relaxed);
#endif
  if (rc != 0) {
    impl.isAdvertising = false;
    log_e("startLegacyViaExtAdv: rc=%d", rc);
    return BTStatus::Fail;
  }
  log_i(
    "Advertising: started (legacy over ext instance %u, duration=%u ms, %u service UUID(s))", Impl::kLegacyInstance, durationMs,
    (unsigned)impl.serviceUUIDs.size()
  );
#if BLE_VALIDATION_HOOKS
  nimbleInjectPreemptedCompleteIfArmed(&impl);
#endif
  return BTStatus::OK;
#else
  struct ble_gap_adv_params advParams = {};
  switch (impl.advType) {
    case BLEAdvType::ConnectableScannable:
      advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
      advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
      break;
    case BLEAdvType::ConnectableDirected:
    case BLEAdvType::DirectedLowDuty:
      advParams.conn_mode = BLE_GAP_CONN_MODE_DIR;
      advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
      advParams.high_duty_cycle = 0;
      break;
    case BLEAdvType::DirectedHighDuty:
      advParams.conn_mode = BLE_GAP_CONN_MODE_DIR;
      advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
      advParams.high_duty_cycle = 1;
      break;
    case BLEAdvType::ScannableUndirected:
      advParams.conn_mode = BLE_GAP_CONN_MODE_NON;
      advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
      break;
    case BLEAdvType::NonConnectable:
      advParams.conn_mode = BLE_GAP_CONN_MODE_NON;
      advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
      break;
  }
  advParams.itvl_min = impl.minInterval;
  advParams.itvl_max = impl.maxInterval;
  advParams.filter_policy = impl.filterPolicy;

  // Resolve device name once; placed in scan response per BLE spec
  // (CSS Part A, §1.2) to keep the primary AD free for flags and UUIDs.
  // The scan response is only sent in reply to an active SCAN_REQ from a
  // scanning device (BT Core Spec v5.x, Vol 6, Part B, §4.4.3.2).
  String name = impl.deviceName.length() > 0 ? impl.deviceName : BLE.getDeviceName();

  // Build primary advertisement data: flags, UUIDs, appearance.
  // Set both LE General Discoverable Mode (bit 1) and BR/EDR Not Supported
  // (bit 2) in the Flags AD type (0x01) as required for LE-only devices.
  // CSS Part A, §1.3; BT Core Spec v5.x, Vol 3, Part C, §9.2.4 (discoverable mode).
  // The name is omitted here when scan response is enabled (spec recommends
  // Local Name in the scan response to leave room for service UUIDs in the
  // primary AD). When scan response is disabled, the name goes into the
  // primary AD instead.
  struct ble_hs_adv_fields fields = {};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  if (!impl.scanResponseEnabled && name.length() > 0) {
    fields.name = reinterpret_cast<const uint8_t *>(name.c_str());
    fields.name_len = name.length();
    fields.name_is_complete = 1;
  }

  if (impl.includeTxPower) {
    fields.tx_pwr_lvl = BLE.getPower();
    fields.tx_pwr_lvl_is_present = 1;
  }

  if (impl.appearance > 0) {
    fields.appearance = impl.appearance;
    fields.appearance_is_present = 1;
  }

  // Slave Connection Interval Range (0x12) — same semantics as BLEAdvDataBuilder.
  uint8_t slaveItvl[4];
  if (impl.minPreferred != 0 || impl.maxPreferred != 0) {
    const uint16_t minInt = impl.minPreferred != 0 ? impl.minPreferred : impl.maxPreferred;
    const uint16_t maxInt = impl.maxPreferred != 0 ? impl.maxPreferred : impl.minPreferred;
    slaveItvl[0] = static_cast<uint8_t>(minInt & 0xFF);
    slaveItvl[1] = static_cast<uint8_t>(minInt >> 8);
    slaveItvl[2] = static_cast<uint8_t>(maxInt & 0xFF);
    slaveItvl[3] = static_cast<uint8_t>(maxInt >> 8);
    fields.slave_itvl_range = slaveItvl;
  }

  if (impl.useCustomAdvData) {
    int rc = ble_gap_adv_set_data(impl.customAdvData.data(), impl.customAdvData.length());
    if (rc != 0) {
      log_e("ble_gap_adv_set_data: rc=%d", rc);
      return BTStatus::Fail;
    }
  } else {
    // Build service UUIDs into fields (16, 32, and 128-bit)
    ble_uuid16_t uuid16s[10];
    ble_uuid32_t uuid32s[10];
    ble_uuid128_t uuid128s[4];
    int n16 = 0, n32 = 0, n128 = 0;
    for (auto &uuid : impl.serviceUUIDs) {
      switch (uuid.bitSize()) {
        case 16:
          if (n16 < 10) {
            uuid16s[n16].u.type = BLE_UUID_TYPE_16;
            uuid16s[n16].value = uuid.toUint16();
            n16++;
          }
          break;
        case 32:
          if (n32 < 10) {
            uuid32s[n32].u.type = BLE_UUID_TYPE_32;
            uuid32s[n32].value = uuid.toUint32();
            n32++;
          }
          break;
        default:
        {
          if (n128 < 4) {
            BLEUUID u128 = uuid.to128();
            uuid128s[n128].u.type = BLE_UUID_TYPE_128;
            const uint8_t *be = u128.data();
            for (int i = 0; i < 16; i++) {
              uuid128s[n128].value[15 - i] = be[i];
            }
            n128++;
          }
          break;
        }
      }
    }
    if (n16 > 0) {
      fields.uuids16 = uuid16s;
      fields.num_uuids16 = n16;
      fields.uuids16_is_complete = 1;
    }
    if (n32 > 0) {
      fields.uuids32 = uuid32s;
      fields.num_uuids32 = n32;
      fields.uuids32_is_complete = 1;
    }
    if (n128 > 0) {
      fields.uuids128 = uuid128s;
      fields.num_uuids128 = n128;
      fields.uuids128_is_complete = 1;
    }

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
      log_e("ble_gap_adv_set_fields: rc=%d", rc);
      return BTStatus::Fail;
    }
  }

  if (impl.scanResponseEnabled) {
    if (impl.useCustomScanRsp) {
      int rc = ble_gap_adv_rsp_set_data(impl.customScanRspData.data(), impl.customScanRspData.length());
      if (rc != 0) {
        log_e("ble_gap_adv_rsp_set_data: rc=%d", rc);
        return BTStatus::Fail;
      }
    } else {
      // Auto-build a scan response with the device name, matching
      // Bluedroid's default behavior when scanResp is enabled.
      struct ble_hs_adv_fields rspFields = {};
      if (name.length() > 0) {
        rspFields.name = reinterpret_cast<const uint8_t *>(name.c_str());
        rspFields.name_len = name.length();
        rspFields.name_is_complete = 1;
      }
      int rc = ble_gap_adv_rsp_set_fields(&rspFields);
      if (rc != 0) {
        log_e("ble_gap_adv_rsp_set_fields: rc=%d", rc);
        return BTStatus::Fail;
      }
    }
  } else {
    (void)ble_gap_adv_rsp_set_data(nullptr, 0);
  }

  int32_t duration = (durationMs == 0) ? BLE_HS_FOREVER : static_cast<int32_t>(durationMs);
  impl.isAdvertising = true;
  ble_addr_t peerAddr = toNimblePeerAddr(impl.directedPeer);
  const ble_addr_t *directAddr = bleAdvTypeIsDirected(impl.advType) ? &peerAddr : nullptr;
  int rc = ble_gap_adv_start(static_cast<uint8_t>(BLE.getOwnAddressType()), directAddr, duration, &advParams, BLEAdvertising::Impl::gapEventCallback, &impl);
  if (rc != 0) {
    impl.isAdvertising = false;
    log_e("ble_gap_adv_start: rc=%d", rc);
    return BTStatus::Fail;
  }

  log_i("Advertising: started (duration=%u ms, %u service UUID(s))", durationMs, (unsigned)impl.serviceUUIDs.size());
#if BLE_VALIDATION_HOOKS
  nimbleInjectPreemptedCompleteIfArmed(&impl);
#endif
  return BTStatus::OK;
#endif /* BLE5_SUPPORTED */
}

#if BLE_VALIDATION_HOOKS
// HCI_LE_Set_Extended_Advertising_Enable(disable) does not generate
// HCI_LE_Advertising_Set_Terminated (Vol 4, Part E, §7.7.65.18). NimBLE
// preemption therefore synthesizes ADV_COMPLETE / BLE_HS_EPREEMPTED from
// ble_gap_preempt_done() after the native stop. Deliver that event on a
// helper task so a public stop() can race the resume window.
static void bleAdvTestDeliverPreemptedComplete(void *arg) {
  auto *advImpl = static_cast<BLEAdvertising::Impl *>(arg);
  struct ble_gap_event ev = {};
  ev.type = BLE_GAP_EVENT_ADV_COMPLETE;
  ev.adv_complete.reason = BLE_HS_EPREEMPTED;
#if BLE5_SUPPORTED
  ev.adv_complete.instance = BLEAdvertising::Impl::kLegacyInstance;
#endif
  (void)BLEAdvertising::Impl::gapEventCallback(&ev, advImpl);
  vTaskDelete(nullptr);
}

static void nimbleInjectPreemptedCompleteIfArmed(BLEAdvertising::Impl *impl) {
  if (!BLEHooks.adv.injectPreemptedComplete.exchange(false, std::memory_order_acq_rel)) {
    return;
  }
  // Native-stop without public stop() so a queued advertiseOnDisconnect restart
  // is not canceled before the synthesized preemption resume runs.
#if BLE5_SUPPORTED
  (void)ble_gap_ext_adv_stop(BLEAdvertising::Impl::kLegacyInstance);
#else
  (void)ble_gap_adv_stop();
#endif
  (void)xTaskCreate(bleAdvTestDeliverPreemptedComplete, "advPreempt", 8192, impl, tskIDLE_PRIORITY + 1, nullptr);
}
#endif

// The advertising flag is always cleared on return, even when ble_gap_adv_stop()
// reports BLE_HS_EALREADY (advertising was not running, e.g. after a BLE.end() /
// BLE.begin() stack restart). Leaving the flag set would make a subsequent start()
// return immediately as a no-op. Matches BLEScan::stop() and Bluedroid's
// BLEAdvertising::stop().
BTStatus BLEAdvertising::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
#if BLE_VALIDATION_HOOKS
  if (BLEHooks.adv.nativeStopKeepingFlag.exchange(false, std::memory_order_acq_rel)) {
    // Leave isAdvertising set so the synthesized complete observes
    // wasAdvertising and takes the preemption-resume path.
#if BLE5_SUPPORTED
    (void)ble_gap_ext_adv_stop(Impl::kLegacyInstance);
#else
    (void)ble_gap_adv_stop();
#endif
    (void)xTaskCreate(bleAdvTestDeliverPreemptedComplete, "advPreempt", 8192, &impl, tskIDLE_PRIORITY + 1, nullptr);
    return BTStatus::OK;
  }
#endif
  return stopBackend();
}

BTStatus BLEAdvertising::stopBackend() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.advSession.fetch_add(1, std::memory_order_relaxed);
#if BLE_GATT_SERVER_SUPPORTED
  // Invalidate even when advertising is already down: connectable advertising
  // stops on connect, but advertiseOnDisconnect may still have a host-queue
  // restart pending. BLEStream::begin() then rebuilds GATT (ble_gatts_reset
  // is EBUSY while GAP is active). Callers that must wait out an in-flight
  // restart take ReAdvGuard before admission; this path must not take that
  // mutex while already holding the admission gate.
  nimbleInvalidatePendingReAdvertise();
#endif
  if (!impl.isAdvertising.load()) {
    return BTStatus::OK;
  }
  log_d("Advertising: stop");
#if BLE5_SUPPORTED
  int rc = ble_gap_ext_adv_stop(Impl::kLegacyInstance);
#else
  int rc = ble_gap_adv_stop();
#endif
#if BLE_VALIDATION_HOOKS
  if (BLEHooks.adv.injectNativeStopFail.exchange(false, std::memory_order_acq_rel)) {
    rc = BLE_HS_EUNKNOWN;
  }
#endif
  // BLE_HS_EALREADY means "not currently advertising" which is our target
  // state; treat it as success and clear the flag. A real native failure
  // leaves the flag set so a later stop() retries instead of no-op'ing.
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    log_e("Advertising: ble_gap_adv_stop rc=%d", rc);
    return BTStatus::Fail;
  }
  impl.isAdvertising = false;
  log_i("Advertising: stopped");
  return BTStatus::OK;
}

void BLEAdvertising::onComplete(CompleteHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = handler;
}

void BLEAdvertising::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = nullptr;
}

// --------------------------------------------------------------------------
// Extended advertising (BLE5 TX)
//
// Spec: BT Core Spec v5.x, Vol 6, Part B, §4.4.2 (extended advertising events).
// NimBLE requires ble_gap_ext_adv_configure() to run before ext_adv_set_data()
// and ext_adv_start(); the per-setter public API therefore accumulates the
// params in Impl::extInstances[] and (re)configures the controller lazily the
// first time data/start is issued after a parameter change.
// --------------------------------------------------------------------------

#if BLE5_SUPPORTED

BLEAdvertising::Impl::ExtInstance *BLEAdvertising::Impl::extInstanceAt(uint8_t instance) {
  // The top instance (kLegacyInstance) is reserved for the public legacy
  // start()/stop() path, so user-addressable extended sets stop below it.
  if (instance >= kLegacyInstance) {
    return nullptr;
  }
  ExtInstance *extInst = &extInstances[instance];
  if (!extInst->slotInitialized) {
    extInst->slotInitialized = true;
    extInst->params.itvl_min = 0x30;  // 30 ms in 0.625 ms units
    extInst->params.itvl_max = 0x60;  // 60 ms
    extInst->params.primary_phy = BLE_HCI_LE_PHY_1M;
    extInst->params.secondary_phy = BLE_HCI_LE_PHY_1M;
    extInst->params.tx_power = 127;   // 127 = no preference
    extInst->params.connectable = 1;  // default: connectable extended (non-scannable)
  }
  return extInst;
}

int BLEAdvertising::Impl::extEnsureConfigured(uint8_t instance) {
  ExtInstance *extInst = extInstanceAt(instance);
  if (!extInst) {
    return BLE_HS_EINVAL;
  }
  if (extInst->extAdvRegistered) {
    return 0;
  }
  extInst->params.own_addr_type = static_cast<uint8_t>(BLE.getOwnAddressType());
  int rc = ble_gap_ext_adv_configure(instance, &extInst->params, nullptr, BLEAdvertising::Impl::gapEventCallback, this);
  if (rc == 0) {
    extInst->extAdvRegistered = true;
  }
  return rc;
}

// Fills a legacy-PDU extended params block from the shared advertising-type
// mapping (BT Core Spec v5.x, Vol 6, Part B, §4.4.2.4 legacy PDU combinations).
static void fillLegacyPduExtAdvParams(BLEAdvType type, struct ble_gap_ext_adv_params &params) {
  const BLEExtAdvProps props = advTypeToExtAdvProps(type, /*legacyPdu=*/true);
  params.legacy_pdu = 1;
  params.connectable = props.connectable;
  params.scannable = props.scannable;
  params.directed = props.directed;
  params.high_duty_directed = props.highDutyDirected;
}

int BLEAdvertising::Impl::startLegacyViaExtAdv(uint32_t durationMs) {
  // Build the legacy-PDU params fresh on every start(): the controller instance
  // is torn down on BLE.end() (and reset()), so we always reconfigure.
  if (ble_gap_ext_adv_active(kLegacyInstance)) {
    (void)ble_gap_ext_adv_stop(kLegacyInstance);
  }
  struct ble_gap_ext_adv_params params = {};
  fillLegacyPduExtAdvParams(advType, params);
  params.own_addr_type = static_cast<uint8_t>(BLE.getOwnAddressType());
  if (params.directed) {
    params.peer = toNimblePeerAddr(directedPeer);
  }
  params.primary_phy = BLE_HCI_LE_PHY_1M;  // legacy PDUs are 1M only
  params.secondary_phy = BLE_HCI_LE_PHY_1M;
  // High-duty directed uses a fixed interval; the HCI interval fields are ignored.
  if (!params.high_duty_directed) {
    params.itvl_min = minInterval;
    params.itvl_max = maxInterval;
  }
  params.tx_power = 127;  // no preference
  params.sid = 0;

  // ADV_DIRECT_IND carries no AD or SCAN_RSP (Vol 6, Part B, §2.3.1.2). The
  // controller rejects HCI_LE_Set_Extended_Advertising_Parameters for high-duty
  // directed while leftover data remains (Vol 4, Part E, §7.8.53 → 0x12).
  if (params.directed) {
    int rrc = ble_gap_ext_adv_remove(kLegacyInstance);
    if (rrc != 0 && rrc != BLE_HS_EALREADY) {
      return rrc;
    }
  }

  int rc = ble_gap_ext_adv_configure(kLegacyInstance, &params, nullptr, BLEAdvertising::Impl::gapEventCallback, this);
  if (rc != 0) {
    return rc;
  }

  const bool scannable = params.scannable != 0;
  String name = deviceName.length() > 0 ? deviceName : BLE.getDeviceName();

  // ADV_DIRECT_IND carries only AdvA and InitA (Vol 6, Part B, §2.3.1.2).
  // NimBLE rejects ble_gap_ext_adv_set_data on a legacy directed set.
  if (!params.directed) {
    // Primary advertisement data. The name is carried in the scan response when
    // scan response is enabled and the PDU is scannable (CSS Part A §1.2).
    const bool nameInScanRsp = scanResponseEnabled && scannable;
    if (useCustomAdvData) {
      struct os_mbuf *om = ble_hs_mbuf_from_flat(customAdvData.data(), customAdvData.length());
      if (!om) {
        return BLE_HS_ENOMEM;
      }
      rc = ble_gap_ext_adv_set_data(kLegacyInstance, om);
    } else {
      BLEAdvertisementData adv =
        BLEAdvDataBuilder::buildLegacyAdvData(name, serviceUUIDs, appearance, includeTxPower, BLE.getPower(), nameInScanRsp, minPreferred, maxPreferred);
      struct os_mbuf *om = ble_hs_mbuf_from_flat(adv.data(), adv.length());
      if (!om) {
        return BLE_HS_ENOMEM;
      }
      rc = ble_gap_ext_adv_set_data(kLegacyInstance, om);
    }
    if (rc != 0) {
      return rc;
    }

    // Scan response (only valid for scannable PDUs).
    if (scannable && scanResponseEnabled) {
      struct os_mbuf *om;
      if (useCustomScanRsp) {
        om = ble_hs_mbuf_from_flat(customScanRspData.data(), customScanRspData.length());
      } else {
        BLEAdvertisementData rsp = BLEAdvDataBuilder::buildLegacyScanRespData(name);
        om = ble_hs_mbuf_from_flat(rsp.data(), rsp.length());
      }
      if (!om) {
        return BLE_HS_ENOMEM;
      }
      rc = ble_gap_ext_adv_rsp_set_data(kLegacyInstance, om);
      if (rc != 0) {
        return rc;
      }
    } else if (scannable) {
      // NimBLE rejects a null mbuf (ble_gap_ext_adv_set). An empty buffer is the
      // HCI-legal way to drop a prior SCAN_RSP (Vol 4, Part E, §7.8.55).
      struct os_mbuf *om = ble_hs_mbuf_from_flat("", 0);
      if (!om) {
        return BLE_HS_ENOMEM;
      }
      rc = ble_gap_ext_adv_rsp_set_data(kLegacyInstance, om);
      if (rc != 0) {
        return rc;
      }
    }
  }

  // Extended advertising duration is in 10 ms units; 0 == no expiration.
  int duration = static_cast<int>(bleGapDurationMsTo10ms(durationMs));
  return ble_gap_ext_adv_start(kLegacyInstance, duration, 0);
}

void BLEAdvertising::setExtType(uint8_t instance, BLEAdvType type) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtType: instance %u out of range", instance);
    return;
  }
  // Extended PDU: a connectable set cannot also be scannable (Vol 6, Part B, §4.4.2.4).
  const BLEExtAdvProps props = advTypeToExtAdvProps(type, /*legacyPdu=*/false);
  extInst->params.legacy_pdu = 0;
  extInst->params.connectable = props.connectable;
  extInst->params.scannable = props.scannable;
  extInst->params.directed = props.directed;
  extInst->params.high_duty_directed = props.highDutyDirected;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtPhy(uint8_t instance, BLEPhy primaryPhy, BLEPhy secondaryPhy) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtPhy: instance %u out of range", instance);
    return;
  }
  // BLEPhy enum values match BLE_HCI_LE_PHY_* codes (1M=1, 2M=2, Coded=3).
  extInst->params.primary_phy = static_cast<uint8_t>(primaryPhy);
  extInst->params.secondary_phy = static_cast<uint8_t>(secondaryPhy);
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtTxPower(uint8_t instance, int8_t txPower) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtTxPower: instance %u out of range", instance);
    return;
  }
  extInst->params.tx_power = txPower;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtInterval(uint8_t instance, uint16_t minInterval, uint16_t maxInterval) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtInterval: instance %u out of range", instance);
    return;
  }
  // Extended advertising intervals are in 0.625 ms controller units.
  extInst->params.itvl_min = minInterval;
  extInst->params.itvl_max = maxInterval;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtChannelMap(uint8_t instance, uint8_t channelMap) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtChannelMap: instance %u out of range", instance);
    return;
  }
  extInst->params.channel_map = channelMap;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtSID(uint8_t instance, uint8_t sid) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtSID: instance %u out of range", instance);
    return;
  }
  extInst->params.sid = sid;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtAnonymous(uint8_t instance, bool anonymous) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtAnonymous: instance %u out of range", instance);
    return;
  }
  extInst->params.anonymous = anonymous ? 1 : 0;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtIncludeTxPower(uint8_t instance, bool include) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtIncludeTxPower: instance %u out of range", instance);
    return;
  }
  extInst->params.include_tx_power = include ? 1 : 0;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtScanReqNotify(uint8_t instance, bool enable) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtScanReqNotify: instance %u out of range", instance);
    return;
  }
  extInst->params.scan_req_notif = enable ? 1 : 0;
  extInst->extAdvRegistered = false;
}

BTStatus BLEAdvertising::setExtAdvertisementData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.extInstanceAt(instance)) {
    return BTStatus::InvalidParam;
  }
  int rc = impl.extEnsureConfigured(instance);
  if (rc != 0) {
    log_e("ext_adv_configure inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  struct os_mbuf *om = ble_hs_mbuf_from_flat(data.data(), data.length());
  if (!om) {
    return BTStatus::NoMemory;
  }
  // ble_gap_ext_adv_set_data takes ownership of the mbuf (frees on success/error).
  rc = ble_gap_ext_adv_set_data(instance, om);
  if (rc != 0) {
    log_e("ext_adv_set_data inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::setExtScanResponseData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.extInstanceAt(instance)) {
    return BTStatus::InvalidParam;
  }
  int rc = impl.extEnsureConfigured(instance);
  if (rc != 0) {
    log_e("ext_adv_configure inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  struct os_mbuf *om = ble_hs_mbuf_from_flat(data.data(), data.length());
  if (!om) {
    return BTStatus::NoMemory;
  }
  rc = ble_gap_ext_adv_rsp_set_data(instance, om);
  if (rc != 0) {
    log_e("ext_adv_rsp_set_data inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::setExtInstanceAddress(uint8_t instance, const BTAddress &addr) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.extInstanceAt(instance)) {
    return BTStatus::InvalidParam;
  }
  int rc = impl.extEnsureConfigured(instance);
  if (rc != 0) {
    log_e("ext_adv_configure inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  ble_addr_t a;
  a.type = static_cast<uint8_t>(addr.type());
  memcpy(a.val, addr.data(), 6);
  rc = ble_gap_ext_adv_set_addr(instance, &a);
  if (rc != 0) {
    log_e("ext_adv_set_addr inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::startExtended(uint8_t instance, uint32_t durationMs, uint8_t maxEvents) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  if (!bleGapDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if (extInst->params.high_duty_directed && !bleHighDutyDirectedDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  int rc = impl.extEnsureConfigured(instance);
  if (rc != 0) {
    log_e("ext_adv_configure inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  // Extended advertising duration is in 10 ms units; 0 == no expiration.
  int duration = static_cast<int>(bleGapDurationMsTo10ms(durationMs));
  extInst->isAdvertising = true;
  rc = ble_gap_ext_adv_start(instance, duration, maxEvents);
  if (rc != 0) {
    extInst->isAdvertising = false;
    log_e("ext_adv_start inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  extInst->durationMs = durationMs;
  extInst->sessionDeadlineUs = durationMs == 0 ? 0 : esp_timer_get_time() + static_cast<int64_t>(durationMs) * 1000;
  extInst->maxEvents = maxEvents;
  impl.advSession.fetch_add(1, std::memory_order_relaxed);
  log_i("Advertising: extended started (instance=%u)", instance);
  return BTStatus::OK;
}

BTStatus BLEAdvertising::stopExtended(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  impl.advSession.fetch_add(1, std::memory_order_relaxed);
  int rc = ble_gap_ext_adv_stop(instance);
  extInst->isAdvertising = false;
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    log_e("ext_adv_stop inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::removeExtended(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  int rc = ble_gap_ext_adv_remove(instance);
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    log_e("ext_adv_remove inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  if (instance < Impl::kMaxExtInstances) {
    impl.extInstances[instance].reset();
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::clearExtended() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  int rc = ble_gap_ext_adv_clear();
  if (rc != 0) {
    log_e("ext_adv_clear: rc=%d", rc);
    return BTStatus::Fail;
  }
  impl.isAdvertising = false;
  for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
    impl.extInstances[i].reset();
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Periodic advertising (BLE5 TX)
//
// Periodic advertising runs on top of an extended, non-connectable,
// non-scannable, non-anonymous instance (Vol 6, Part B, §4.4.2.9). Requires
// CONFIG_BT_NIMBLE_ENABLE_PERIODIC_ADV in addition to CONFIG_BT_NIMBLE_EXT_ADV;
// otherwise these entry points report NotSupported.
// --------------------------------------------------------------------------

#if BLE_PERIODIC_ADV_SUPPORTED

void BLEAdvertising::setPeriodicAdvInterval(uint8_t instance, uint16_t minInterval, uint16_t maxInterval) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setPeriodicAdvInterval: instance %u out of range", instance);
    return;
  }
  extInst->periodicParams.itvl_min = minInterval;
  extInst->periodicParams.itvl_max = maxInterval;
  extInst->periodicAdvRegistered = false;
}

void BLEAdvertising::setPeriodicAdvTxPower(uint8_t instance, bool include) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setPeriodicAdvTxPower: instance %u out of range", instance);
    return;
  }
  extInst->periodicParams.include_tx_power = include ? 1 : 0;
  extInst->periodicAdvRegistered = false;
}

BTStatus BLEAdvertising::setPeriodicAdvData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  int rc = impl.extEnsureConfigured(instance);
  if (rc != 0) {
    log_e("ext_adv_configure inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  if (!extInst->periodicAdvRegistered) {
    rc = ble_gap_periodic_adv_configure(instance, &extInst->periodicParams);
    if (rc != 0) {
      log_e("periodic_adv_configure inst %u: rc=%d", instance, rc);
      return BTStatus::Fail;
    }
    extInst->periodicAdvRegistered = true;
  }
  struct os_mbuf *om = ble_hs_mbuf_from_flat(data.data(), data.length());
  if (!om) {
    return BTStatus::NoMemory;
  }
#if defined(CONFIG_BT_NIMBLE_PERIODIC_ADV_ENH)
  struct ble_gap_periodic_adv_set_data_params params = {};
  rc = ble_gap_periodic_adv_set_data(instance, om, &params);
#else
  rc = ble_gap_periodic_adv_set_data(instance, om);
#endif
  if (rc != 0) {
    log_e("periodic_adv_set_data inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::startPeriodicAdv(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::InvalidState;
  }
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  int rc = impl.extEnsureConfigured(instance);
  if (rc != 0) {
    log_e("ext_adv_configure inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  if (!extInst->periodicAdvRegistered) {
    rc = ble_gap_periodic_adv_configure(instance, &extInst->periodicParams);
    if (rc != 0) {
      log_e("periodic_adv_configure inst %u: rc=%d", instance, rc);
      return BTStatus::Fail;
    }
    extInst->periodicAdvRegistered = true;
  }
#if defined(CONFIG_BT_NIMBLE_PERIODIC_ADV_ENH)
  struct ble_gap_periodic_adv_start_params params = {};
  rc = ble_gap_periodic_adv_start(instance, &params);
#else
  rc = ble_gap_periodic_adv_start(instance);
#endif
  if (rc != 0) {
    log_e("periodic_adv_start inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::stopPeriodicAdv(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE_GATT_SERVER_SUPPORTED
  ReAdvGuard reAdv;
#endif
  BLEAdmissionGuard admission(impl.nativeAdmission);
  if (!admission) {
    return BTStatus::OK;
  }
  int rc = ble_gap_periodic_adv_stop(instance);
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    log_e("periodic_adv_stop inst %u: rc=%d", instance, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

#endif /* BLE_PERIODIC_ADV_SUPPORTED */

#endif /* BLE5_SUPPORTED */

void BLEAdvertising::prepareBackendStart() {
  if (_impl) {
    _impl->nativeAdmission.reopen();
  }
}

void BLEAdvertising::beginBackendShutdown() {
  if (!_impl) {
    return;
  }
  _impl->nativeAdmission.closeAndWait();
  (void)stopBackend();
#if BLE5_SUPPORTED
  for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
#if BLE_PERIODIC_ADV_SUPPORTED
    (void)ble_gap_periodic_adv_stop(i);
#endif
    (void)ble_gap_ext_adv_stop(i);
  }
  (void)ble_gap_ext_adv_clear();
#endif
  reset();
}

#else /* !BLE_ADVERTISING_SUPPORTED -- stubs */

// Stubs for BLE_ADVERTISING_SUPPORTED == 0: log; start/stop return NotSupported; reset* mostly log.

void BLEAdvertising::setName(const String &) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanResponse(bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setType(BLEAdvType) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setInterval(uint16_t, uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setMinPreferred(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setMaxPreferred(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setTxPower(bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setAppearance(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanFilter(bool, bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::reset() {
  log_w("Advertising not supported");
}

void BLEAdvertising::prepareBackendStart() {}

void BLEAdvertising::beginBackendShutdown() {}

BTStatus BLEAdvertising::stopBackend() {
  return BTStatus::NotSupported;
}

void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &) {
  log_w("Advertising not supported");
}

BTStatus BLEAdvertising::start(uint32_t) {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::stop() {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

void BLEAdvertising::onComplete(CompleteHandler) {
  log_w("Advertising not supported");
}

void BLEAdvertising::resetCallbacks() {
  log_w("Advertising not supported");
}

// BLE5 extended/periodic advertising need no stubs here: the advertising role is
// unavailable, so BLE5_ADV_TX_SUPPORTED / BLE_PERIODIC_ADV_TX_SUPPORTED are 0 and
// the shared NotSupported fallback in BLEAdvertising.cpp defines those methods.

#endif /* BLE_ADVERTISING_SUPPORTED */

#endif /* BLE_NIMBLE */
