/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2026 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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
#if BLE_ENABLED && !BLE_L2CAP_SUPPORTED

#include "BLEL2CAP.h"
#include "BLE.h"
#include "esp32-hal-log.h"

/**
 * @file BLEL2CAP.cpp
 * @brief L2CAP CoC translation unit when @c BLE_L2CAP_SUPPORTED is false: inert types and
 *        factories; mutating or callback paths log and return @c NotSupported or 0/false.
 */

// Stubs for !BLE_L2CAP_SUPPORTED: null implementation pointer, log on API use, return NotSupported / 0 / false.

BLEL2CAPChannel::BLEL2CAPChannel() : _impl(nullptr) {}

BLEL2CAPChannel::operator bool() const {
  return false;
}

BTStatus BLEL2CAPChannel::write(const uint8_t *data, size_t len) {
  (void)data;
  (void)len;
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEL2CAPChannel::disconnect() {
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEL2CAPChannel::onData(DataHandler handler) {
  (void)handler;
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEL2CAPChannel::onDisconnect(DisconnectHandler handler) {
  (void)handler;
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEL2CAPChannel::resetCallbacks() {
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

bool BLEL2CAPChannel::isConnected() const {
  return false;
}

uint16_t BLEL2CAPChannel::getPSM() const {
  return 0;
}

uint16_t BLEL2CAPChannel::getMTU() const {
  return 0;
}

uint16_t BLEL2CAPChannel::getConnHandle() const {
  return 0;
}

BLEL2CAPServer::BLEL2CAPServer() : _impl(nullptr) {}

BLEL2CAPServer::operator bool() const {
  return false;
}

BTStatus BLEL2CAPServer::onAccept(AcceptHandler handler) {
  (void)handler;
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEL2CAPServer::onData(DataHandler handler) {
  (void)handler;
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEL2CAPServer::onDisconnect(DisconnectHandler handler) {
  (void)handler;
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEL2CAPServer::resetCallbacks() {
  log_w("%s not supported (L2CAP unavailable)", __func__);
  return BTStatus::NotSupported;
}

uint16_t BLEL2CAPServer::getPSM() const {
  return 0;
}

uint16_t BLEL2CAPServer::getMTU() const {
  return 0;
}

BLEL2CAPServer BLEClass::createL2CAPServer(uint16_t, uint16_t) {
  log_w("createL2CAPServer not supported (L2CAP unavailable)");
  return BLEL2CAPServer();
}

BLEL2CAPChannel BLEClass::connectL2CAP(uint16_t, uint16_t, uint16_t) {
  log_w("connectL2CAP not supported (L2CAP unavailable)");
  return BLEL2CAPChannel();
}

#endif
