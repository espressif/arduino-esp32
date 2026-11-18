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
 * @file BLEAudio.nimble.h
 * @brief NimBLE backend definition of `BLEAudio::Impl`.
 *
 * NimBLE GATT event forwarding (`bleAudioForwardHostGattEvent`) and GATT
 * coordinator queries (`bleAudioGattActive`, `bleAudioGattFrozen`) are wired
 * through weak `extern "C"` hooks in `core/BLEAudioHooks.h` and defined in
 * `BLEAudio.nimble.cpp`.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE && BLE_AUDIO_SUPPORTED

#include "BLEAudioImpl.h"
#include "client/BLEClient.h"
#include <memory>
#include <vector>

struct BLEAudio::Impl : BLEAudioImplCommon {
  /// NimBLE central `connect()` fallback clients; pruned on ACL disconnect and end().
  std::vector<std::shared_ptr<BLEClient>> clients;

  static void backendOnControllerCreated(BLEClass::Impl &bleImpl);
  BTStatus backendConnect(const BTAddress &address);
  void backendPreEndTeardown();
  void backendOnAclDisconnected(uint16_t connHandle);
  void backendResetLinks();
};

#endif /* BLE_NIMBLE && BLE_AUDIO_SUPPORTED */
