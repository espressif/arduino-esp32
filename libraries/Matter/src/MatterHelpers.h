// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <MatterButton.h>

// Sketch-side helpers. They print to Serial and may reboot.
// Not members of ArduinoMatter. Pulled in by Matter.h.

// setup() after Matter.begin(): pairing codes if no fabric; status every 10 s
// and once more when CASE is up; wait up to timeoutMs (0 = forever). Reboots
// if still no fabric. If commissioned but CASE never arrives, continues.
void matterWaitUntilReady(uint32_t timeoutMs = 5 * 60 * 1000);
// loop(): hub removed the fabric. Button Matter.decommission() already resets.
void matterRestartIfNoFabric();

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
