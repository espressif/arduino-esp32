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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEAdvTypes.h"

/**
 * @brief Validate connection parameters against Bluetooth Core Spec Vol 6, Part B, §4.5.1.
 * @return True if all parameters are within spec-mandated ranges and the supervision
 *         timeout satisfies: Supervision_Timeout > (1 + connSlaveLatency) × connInterval × 2
 *
 * Allowed ranges (BT Core Spec v5.x, Vol 6, Part B, §4.5.1, Table 4.45):
 *  - Connection Interval (minInterval / maxInterval): 6–3200 in 1.25 ms units (7.5 ms – 4 s).
 *    minInterval must be ≤ maxInterval.
 *  - Connection Latency: 0–499 (number of connection events the peripheral may skip).
 *  - Supervision Timeout: 10–3200 in 10 ms units (100 ms – 32 s).
 *
 * Supervision timeout constraint (derived from Vol 6, Part B, §4.5.1):
 *   Supervision_Timeout (ms) > (1 + latency) × maxInterval (ms) × 2
 *   = timeout × 10 ms > (1 + latency) × maxInterval × 1.25 ms × 2
 * Scaled to integer arithmetic (× 10 / 1.25 = × 8):
 *   timeout × 100 > (1 + latency) × maxInterval × 25
 */
bool BLEConnParams::isValid() const {
  if (minInterval < 6 || maxInterval > 3200 || minInterval > maxInterval) {
    return false;
  }

  if (latency > 499) {
    return false;
  }

  if (timeout < 10 || timeout > 3200) {
    return false;
  }

  // Supervision_Timeout (10 ms units) > (1 + latency) × maxInterval (1.25 ms units) × 2
  // Equivalent integer check avoiding floating-point:
  //   timeout × 10ms > (1 + latency) × maxInterval × 1.25ms × 2
  //   ⇒ timeout × 100 > (1 + latency) × maxInterval × 25
  if ((uint32_t)timeout * 100 <= (uint32_t)(1 + latency) * maxInterval * 25) {
    return false;
  }

  return true;
}

#endif /* BLE_ENABLED */
