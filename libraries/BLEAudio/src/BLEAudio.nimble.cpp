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

/**
 * @file BLEAudio.nimble.cpp
 * @brief NimBLE backend methods of `BLEAudio::Impl` and core GATT/GATT-event weak hooks.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE && BLE_AUDIO_SUPPORTED

#include "BLE.h"
#include "BLE.nimble.h"
#include "BLEAudio.nimble.h"
#include "engine/BLEAudioEngine.h"
#include "engine/BLEAudioEngine.nimble.h"
#include "core/BLEAudioHooks.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"
#include <algorithm>

extern "C" void bleAudioForwardHostGattEvent(struct ble_gap_event *event) {
  BLEAudioEngine::forwardHostGattEvent(event);
}

extern "C" bool bleAudioGattActive() {
  return bleAudioEngineIsInitialized();
}

extern "C" bool bleAudioGattFrozen() {
  return bleAudioEngineIsStarted();
}

void BLEAudio::Impl::backendOnControllerCreated(BLEClass::Impl &) {}

BTStatus BLEAudio::Impl::backendConnect(const BTAddress &address) {
  auto client = std::make_shared<BLEClient>(BLE.createClient());
  BTStatus st = client->connectAsync(address);
  if (st) {
    BLELockGuard lock(mtx.get());
    clients.erase(
      std::remove_if(clients.begin(), clients.end(), [](const std::shared_ptr<BLEClient> &c) {
        return !c || !c->isConnected();
      }),
      clients.end()
    );
    clients.push_back(client);
  }
  return st;
}

void BLEAudio::Impl::backendPreEndTeardown() {
  std::vector<std::shared_ptr<BLEClient>> localClients;
  {
    BLELockGuard lock(mtx.get());
    localClients.swap(clients);
  }
  for (auto &client : localClients) {
    if (client) {
      (void)client->cancelConnect();
      if (client->isConnected()) {
        (void)client->disconnect();
      }
    }
  }
}

void BLEAudio::Impl::backendOnAclDisconnected(uint16_t connHandle) {
  clients.erase(
    std::remove_if(clients.begin(), clients.end(), [&](const std::shared_ptr<BLEClient> &client) {
      return !client || !client->isConnected() || client->getHandle() == connHandle;
    }),
    clients.end()
  );
}

void BLEAudio::Impl::backendResetLinks() {
  clients.clear();
}

#endif /* BLE_NIMBLE && BLE_AUDIO_SUPPORTED */
