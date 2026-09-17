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

// ESP32-C5 publishes one sdkconfig (Wi-Fi Matter). Tools → Matter Network → Thread
// links libespressif__esp_matter.thread.a and sets ARDUINO_MATTER_NETWORK_THREAD.
// Remap CHIP Kconfig the sketch and Matter.cpp read. Include after sdkconfig.h
// and before CHIP / esp_matter headers. Do not #undef these from a sketch.

#if defined(ARDUINO_MATTER_NETWORK_THREAD)
#undef CONFIG_ENABLE_MATTER_OVER_THREAD
#define CONFIG_ENABLE_MATTER_OVER_THREAD 1
#undef CONFIG_ENABLE_WIFI_STATION
#undef CONFIG_ENABLE_WIFI_AP
#endif
