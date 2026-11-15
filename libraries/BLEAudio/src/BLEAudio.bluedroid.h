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
 * @file BLEAudio.bluedroid.h
 * @brief Bluedroid backend definition of `BLEAudio::Impl`.
 *
 * The engine's Bluedroid adapter registers its own BTA GATTS/GATTC apps; only
 * SMP completion (`ESP_GAP_BLE_AUTH_CMPL_EVT`) and PAST diagnostics arrive on
 * the application GAP callback and are routed through `BLEClass::Impl::audioGapHook`.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID && BLE_AUDIO_SUPPORTED

#include "BLEAudioImpl.h"

struct BLEAudio::Impl : BLEAudioImplCommon {
  static void backendOnControllerCreated(BLEClass::Impl &bleImpl);
  BTStatus backendConnect(const BTAddress &address);
  void backendPreEndTeardown();
  void backendOnAclDisconnected(uint16_t) {}
  void backendResetLinks() {}
};

#endif /* BLE_BLUEDROID && BLE_AUDIO_SUPPORTED */
