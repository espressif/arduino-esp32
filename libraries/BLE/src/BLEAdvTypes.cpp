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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEAdvTypes.h"

// Contract documented on the declaration in BLEAdvTypes.h. Allowed ranges
// (BT Core Spec v5.x, Vol 6, Part B, §4.5.1, Table 4.45): interval 6–3200
// (1.25 ms units, min ≤ max), latency 0–499, timeout 10–3200 (10 ms units).
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
