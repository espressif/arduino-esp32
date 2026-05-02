// Copyright 2018-2026 Espressif Systems (Shanghai) PTE LTD
// Copyright 2018 Evandro Luis Copercini
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

#ifndef _BLUETOOTH_SERIAL_H_
#define _BLUETOOTH_SERIAL_H_

#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if SOC_BT_SUPPORTED && defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include "Arduino.h"
#include "Stream.h"
#include "BTAddress.h"
#include "BTStatus.h"
#include <functional>
#include <memory>
#include <vector>

class [[deprecated("BluetoothSerial won't be supported in version 4.0.0 by default")]] BluetoothSerial : public Stream {
public:
  BluetoothSerial();
  ~BluetoothSerial();

  BluetoothSerial(const BluetoothSerial &) = delete;
  BluetoothSerial &operator=(const BluetoothSerial &) = delete;
  BluetoothSerial(BluetoothSerial &&) = default;
  BluetoothSerial &operator=(BluetoothSerial &&) = default;

  struct DiscoveryResult {
    BTAddress address;
    String name;
    uint32_t cod = 0;
    int8_t rssi = -128;
  };

  using DiscoveryHandler = std::function<void(const DiscoveryResult &)>;
  using ConfirmRequestHandler = std::function<bool(uint32_t)>;
  using AuthCompleteHandler = std::function<void(bool)>;
  using DataHandler = std::function<void(const uint8_t *, size_t)>;

  BTStatus begin(const String &localName = "ESP32", bool isInitiator = false);
  void end(bool releaseMemory = false);

  // Stream interface
  int available() override;
  int peek() override;
  int read() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t len) override;
  /**
   * @brief Waits until the TX queue drains. Does NOT wait for data to be
   *        acknowledged on the wire — bytes may still be in-flight when
   *        flush() returns.
   */
  void flush() override;

  // Connection
  BTStatus connect(const String &remoteName, uint32_t timeoutMs = 10000);
  BTStatus connect(const BTAddress &address, uint8_t channel = 0);
  BTStatus disconnect();
  bool connected() const;
  bool hasClient() const;

  // Discovery
  std::vector<DiscoveryResult> discover(uint32_t timeoutMs = 10000);
  BTStatus discoverAsync(DiscoveryHandler callback, uint32_t timeoutMs = 10000);
  BTStatus discoverStop();

  // Security
  void enableSSP(bool input = false, bool output = false);
  void disableSSP();
  void setPin(const char *pin);
  BTStatus onConfirmRequest(ConfirmRequestHandler callback);
  BTStatus onAuthComplete(AuthCompleteHandler callback);

  // Bond management
  std::vector<BTAddress> getBondedDevices();
  BTStatus deleteBond(const BTAddress &address);
  BTStatus deleteAllBonds();

  // Data callback
  BTStatus onData(DataHandler callback);

  // Info
  BTAddress getAddress() const;
  explicit operator bool() const;

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

#endif /* SOC_BT_SUPPORTED && CONFIG_BT_ENABLED && CONFIG_BLUEDROID_ENABLED */

#endif /* _BLUETOOTH_SERIAL_H_ */
