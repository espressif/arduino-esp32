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

#pragma once

/**
 * @file
 * @brief Stack-agnostic advertising/scan helpers shared by both backends: the
 *        0.625 ms controller-time conversion and the @ref BLEAdvType to neutral
 *        advertising-PDU property mapping. Must stay free of any NimBLE/Bluedroid
 *        or native BT header.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEAdvTypes.h"
#include <stdint.h>

/**
 * @brief Convert milliseconds to 0.625 ms controller time units.
 *
 * Advertising and scan interval/window parameters are expressed in 0.625 ms
 * units by the controller (BT Core Spec v5.x, Vol 6, Part B, §4.4.2.2 for
 * advertising, §4.4.3 for scanning; advertising valid range 0x0020 = 20 ms to
 * 0x4000 = 10.24 s). Result is @c floor(ms * 1.6); the @c uint32_t widening
 * avoids overflow for the full @c uint16_t input range.
 *
 * @param ms Duration in milliseconds.
 * @return   Duration in 0.625 ms units.
 */
inline uint16_t bleMsToUnits0625(uint16_t ms) {
  return static_cast<uint16_t>((static_cast<uint32_t>(ms) * 1000) / 625);
}

/**
 * @brief Neutral advertising-PDU property flags derived from @ref BLEAdvType.
 *
 * Mirrors the four independent bits every controller API exposes for an
 * advertising set (BT Core Spec v5.x, Vol 6, Part B, §4.4.2.4). @c highDutyDirected
 * is only meaningful when @c directed is set.
 */
struct BLEExtAdvProps {
  bool connectable;
  bool scannable;
  bool directed;
  bool highDutyDirected;
};

/**
 * @brief Map an advertising type onto its neutral PDU property flags.
 *
 * Owns the single decision of which of {connectable, scannable, directed,
 * high-duty-directed} a given advertising type implies. Each backend translates
 * these neutral flags into its native params/mask, so the type semantics live in
 * exactly one place.
 *
 * @param type      High-level advertising type.
 * @param legacyPdu When true, apply legacy-PDU semantics (a connectable
 *        undirected PDU, ADV_IND, is always scannable); when false, extended-PDU
 *        semantics apply (a connectable extended PDU cannot also be scannable,
 *        Vol 6, Part B, §4.4.2.4).
 * @return The property flags implied by @p type.
 */
inline BLEExtAdvProps advTypeToExtAdvProps(BLEAdvType type, bool legacyPdu) {
  BLEExtAdvProps p{false, false, false, false};
  switch (type) {
    case BLEAdvType::ConnectableScannable:
      p.connectable = true;
      p.scannable = legacyPdu;  // legacy connectable PDUs are scannable; extended ones are not
      break;
    case BLEAdvType::ConnectableDirected:
    case BLEAdvType::DirectedLowDuty:
      p.connectable = true;
      p.directed = true;
      break;
    case BLEAdvType::DirectedHighDuty:
      p.connectable = true;
      p.directed = true;
      p.highDutyDirected = true;
      break;
    case BLEAdvType::ScannableUndirected: p.scannable = true; break;
    case BLEAdvType::NonConnectable:      break;
  }
  return p;
}

#endif /* BLE_ENABLED */
