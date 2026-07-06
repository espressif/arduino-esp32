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

#include "BLEConnInfo.h"

struct BLEConnInfo::Data {
  uint16_t handle;
  uint16_t mtu;
  BTAddress address;
  BTAddress idAddress;
  uint16_t interval;
  uint16_t latency;
  uint16_t supervisionTimeout;
  uint8_t txPhy;
  uint8_t rxPhy;
  uint8_t keySize;
  bool encrypted;
  bool authenticated;
  bool bonded;
  bool central;
  int8_t rssi;
};
