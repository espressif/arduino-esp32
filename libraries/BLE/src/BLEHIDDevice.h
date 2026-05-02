/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2018 chegewara
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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEServer.h"
#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "HIDTypes.h"

/**
 * @brief HID-over-GATT (HOGP) device helper.
 *
 * Creates and manages the standard GATT services for a BLE HID device:
 *   - Device Information Service (0x180A)
 *   - HID Service (0x1812)
 *   - Battery Service (0x180F)
 *
 * Usage:
 * @code
 * BLEServer server = BLE.createServer();
 * BLEHIDDevice hid(server);
 * hid.manufacturer("MyCompany");
 * hid.pnp(0x02, 0x1234, 0x5678, 0x0100);
 * hid.hidInfo(0x00, 0x01);
 * hid.reportMap(myDescriptor, sizeof(myDescriptor));
 * server.start();
 * @endcode
 */
class BLEHIDDevice {
public:
  /**
   * @brief Construct a HID device and create all required GATT services.
   * @param server The BLE server to host the HID, Device Information, and Battery services on.
   */
  explicit BLEHIDDevice(BLEServer server);
  ~BLEHIDDevice() = default;

  /**
   * @brief Check whether this HID device was constructed with a valid server.
   * @return true if the underlying server handle is valid.
   */
  explicit operator bool() const {
    return static_cast<bool>(_server);
  }

  /**
   * @brief Set the HID Report Map (report descriptor).
   * @param map Pointer to the HID report descriptor byte array.
   * @param size Length of the report descriptor in bytes.
   */
  void reportMap(const uint8_t *map, uint16_t size);

  /**
   * @brief Get the Device Information Service (UUID 0x180A).
   * @return Handle to the Device Information Service.
   */
  BLEService deviceInfoService();

  /**
   * @brief Get the HID Service (UUID 0x1812).
   * @return Handle to the HID Service.
   */
  BLEService hidService();

  /**
   * @brief Get the Battery Service (UUID 0x180F).
   * @return Handle to the Battery Service.
   */
  BLEService batteryService();

  /**
   * @brief Get the Manufacturer Name characteristic.
   * @return Handle to the Manufacturer Name String characteristic.
   */
  BLECharacteristic manufacturer();

  /**
   * @brief Set the Manufacturer Name String value.
   * @param name Manufacturer name to write into the Device Information Service.
   */
  void manufacturer(const String &name);

  /**
   * @brief Set the PnP ID characteristic of the Device Information Service.
   * @param sig Vendor ID source (0x01 = Bluetooth SIG, 0x02 = USB Implementers Forum).
   * @param vendorId Vendor ID assigned by the source indicated by @p sig.
   * @param productId Product ID assigned by the vendor.
   * @param version Product version (BCD, e.g. 0x0100 = 1.0.0).
   */
  void pnp(uint8_t sig, uint16_t vendorId, uint16_t productId, uint16_t version);

  /**
   * @brief Set the HID Information characteristic.
   * @param country HID country code (0x00 = not localized).
   * @param flags HID information flags (bit 0: RemoteWake, bit 1: NormallyConnectable).
   */
  void hidInfo(uint8_t country, uint8_t flags);

  /**
   * @brief Update the battery level characteristic and notify subscribed clients.
   * @param level Battery percentage (0–100).
   */
  void setBatteryLevel(uint8_t level);

  /**
   * @brief Get the HID Control Point characteristic.
   * @return Handle to the HID Control Point characteristic (Suspend / Exit Suspend).
   */
  BLECharacteristic hidControl();

  /**
   * @brief Get the Protocol Mode characteristic.
   * @return Handle to the Protocol Mode characteristic (Boot / Report protocol).
   */
  BLECharacteristic protocolMode();

  /**
   * @brief Create and return an Input Report characteristic.
   * @param reportId HID Report ID to assign via the Report Reference descriptor.
   * @return Handle to the new Input Report characteristic.
   */
  BLECharacteristic inputReport(uint8_t reportId);

  /**
   * @brief Create and return an Output Report characteristic.
   * @param reportId HID Report ID to assign via the Report Reference descriptor.
   * @return Handle to the new Output Report characteristic.
   */
  BLECharacteristic outputReport(uint8_t reportId);

  /**
   * @brief Create and return a Feature Report characteristic.
   * @param reportId HID Report ID to assign via the Report Reference descriptor.
   * @return Handle to the new Feature Report characteristic.
   */
  BLECharacteristic featureReport(uint8_t reportId);

  /**
   * @brief Get the Boot Keyboard Input Report characteristic.
   * @return Handle to the Boot Keyboard Input Report characteristic.
   */
  BLECharacteristic bootInput();

  /**
   * @brief Get the Boot Keyboard Output Report characteristic.
   * @return Handle to the Boot Keyboard Output Report characteristic.
   */
  BLECharacteristic bootOutput();

private:
  BLEServer _server;
  BLEService _devInfoSvc;
  BLEService _hidSvc;
  BLEService _batterySvc;

  BLECharacteristic _mfgChar;
  BLECharacteristic _pnpChar;
  BLECharacteristic _hidInfoChar;
  BLECharacteristic _reportMapChar;
  BLECharacteristic _hidControlChar;
  BLECharacteristic _protocolModeChar;
  BLECharacteristic _batteryLevelChar;
  BLECharacteristic _bootInputChar;
  BLECharacteristic _bootOutputChar;
};

#endif /* BLE_ENABLED */
