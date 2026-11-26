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

#include <sdkconfig.h>
#include "soc/soc_caps.h"
#include <MatterButton.h>

// Sketch-side helpers. They print to Serial and may reboot.
// Not members of ArduinoMatter. Pulled in by Matter.h.

#if !CONFIG_ENABLE_CHIPOBLE && (SOC_WIFI_SUPPORTED || CONFIG_ESP_HOSTED_ENABLED)
/**
 * @brief Connect Wi-Fi station with IPv6 enabled (builds without CHIPoBLE).
 *
 * Present only when @c CONFIG_ENABLE_CHIPOBLE is off (Arduino IDE ESP32 / ESP32-S2).
 * Enables station IPv6 before @c WiFi.begin() (Arduino does not do that by default),
 * then waits for IPv4. CHIPoBLE / Thread builds omit this so MatterHelpers.cpp does
 * not pull in WiFi.h. On-network sketches with CHIPoBLE compiled in include WiFi.h
 * and call @c WiFi.begin() themselves. Not present on ESP32-H2.
 *
 * Call from @c setup() before @c Matter.begin(). Prints to Serial.
 *
 * @param ssid Wi-Fi SSID.
 * @param password Wi-Fi password.
 */
void matterConnectWiFi(const char *ssid, const char *password);
#endif

/**
 * @brief Set example VendorName "Espressif" and ProductName "<SoC> <endpointName>".
 *
 * For example @c matterSetExampleIdentity("Color Light") yields product
 * "ESP32-C6 Color Light". ProductName is capped at 32 characters.
 * Call from @c setup() before @c Matter.begin().
 *
 * @param endpointName Endpoint / product suffix (e.g. "Color Light").
 * @return true if identity was stored, false otherwise.
 */
bool matterSetExampleIdentity(const char *endpointName);

/**
 * @brief Wait in @c setup() after @c Matter.begin() until commissioned (and prefer CASE).
 *
 * If the stack never started, prints that @c begin() failed and halts (does not
 * return to @c loop()). Otherwise prints pairing codes if there is no fabric;
 * one-line status every 10 s and once more when CASE is up. Waits up to
 * @a timeoutMs for CASE. Reboots if still uncommissioned. If commissioned but
 * CASE never arrives, continues.
 *
 * Prints to Serial and may reboot. Do not wait for commissioning in @c loop().
 *
 * @param timeoutMs Maximum wait in milliseconds. Default 5 minutes.
 *                  0 waits forever (unlike @c Matter.waitForNetwork(0), which is a single check).
 */
void matterWaitUntilReady(uint32_t timeoutMs = 5 * 60 * 1000);
/**
 * @brief Reboot from @c loop() if the hub removed the fabric.
 *
 * No-op if the stack never started. @c Matter.decommission() already factory-resets.
 * Prints to Serial and may reboot.
 */
void matterRestartIfNoFabric();

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
