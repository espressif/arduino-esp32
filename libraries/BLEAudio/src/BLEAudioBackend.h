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
 * @file BLEAudioBackend.h
 * @brief Single backend selector for the BLEAudio library (mirrors core/BLEBackend.h).
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#if BLE_NIMBLE
#include "BLEAudio.nimble.h"
#elif BLE_BLUEDROID
#include "BLEAudio.bluedroid.h"
#endif

#include <type_traits>
static_assert(std::is_base_of<BLEAudioImplCommon, BLEAudio::Impl>::value, "BLEAudio::Impl must inherit BLEAudioImplCommon");

#endif /* BLE_AUDIO_SUPPORTED */
