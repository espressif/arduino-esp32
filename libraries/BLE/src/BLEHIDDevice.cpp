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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLE.h"

#include <cstring>

static const BLEUUID kDevInfoSvcUUID(static_cast<uint16_t>(0x180A));
static const BLEUUID kHIDSvcUUID(static_cast<uint16_t>(0x1812));
static const BLEUUID kBatterySvcUUID(static_cast<uint16_t>(0x180F));

static const BLEUUID kMfgNameUUID(static_cast<uint16_t>(0x2A29));
static const BLEUUID kPnpIdUUID(static_cast<uint16_t>(0x2A50));
static const BLEUUID kHIDInfoUUID(static_cast<uint16_t>(0x2A4A));
static const BLEUUID kReportMapUUID(static_cast<uint16_t>(0x2A4B));
static const BLEUUID kHIDControlUUID(static_cast<uint16_t>(0x2A4C));
static const BLEUUID kProtocolModeUUID(static_cast<uint16_t>(0x2A4E));
static const BLEUUID kReportUUID(static_cast<uint16_t>(0x2A4D));
static const BLEUUID kBatteryLevelUUID(static_cast<uint16_t>(0x2A19));
static const BLEUUID kBootInputUUID(static_cast<uint16_t>(0x2A22));
static const BLEUUID kBootOutputUUID(static_cast<uint16_t>(0x2A32));

static const BLEUUID kReportRefDescUUID(static_cast<uint16_t>(0x2908));
static const BLEUUID kExtReportRefDescUUID(static_cast<uint16_t>(0x2907));

/**
 * @brief Construct a HID device and create all required GATT services and characteristics.
 * @param server The BLE server to host the HID, Device Information, and Battery services on.
 * @note Creates Device Information (0x180A), HID (0x1812), and Battery (0x180F) services.
 *       The HID Service includes the Battery Service via an Include Declaration
 *       (HIDS 1.0 §3) and an External Report Reference descriptor on the
 *       Report Map pointing to the Battery Level UUID (HIDS 1.0 §3.6).
 *       All characteristics default to open (unencrypted) permissions; for HoGP
 *       compliance, configure BLEServer-level security or re-create with
 *       encrypted permissions. Protocol Mode is initialized to Report Protocol
 *       Mode (1). The HID Service UUID is automatically added to the
 *       advertising payload.
 */
BLEHIDDevice::BLEHIDDevice(BLEServer server) : _server(server) {
  _devInfoSvc = _server.createService(kDevInfoSvcUUID);
  _batterySvc = _server.createService(kBatterySvcUUID);
  _hidSvc = _server.createService(kHIDSvcUUID, 40);

  // HIDS 1.0 §3: Battery Service shall be an Included Service of the HID Service.
  _hidSvc.addIncludedService(_batterySvc);

  _mfgChar = _devInfoSvc.createCharacteristic(kMfgNameUUID, BLEProperty::Read, BLEPermissions::OpenRead);
  _pnpChar = _devInfoSvc.createCharacteristic(kPnpIdUUID, BLEProperty::Read, BLEPermissions::OpenRead);

  _hidInfoChar = _hidSvc.createCharacteristic(kHIDInfoUUID, BLEProperty::Read, BLEPermissions::OpenRead);
  _reportMapChar = _hidSvc.createCharacteristic(kReportMapUUID, BLEProperty::Read, BLEPermissions::OpenRead);

  // HIDS 1.0 §3.6: External Report Reference descriptor on Report Map
  // referencing the Battery Level characteristic UUID.
  BLEDescriptor extRef = _reportMapChar.createDescriptor(kExtReportRefDescUUID, BLEPermission::Read);
  uint8_t battUuid16[2] = {0x19, 0x2A};  // 0x2A19 little-endian
  extRef.setValue(battUuid16, 2);

  _hidControlChar = _hidSvc.createCharacteristic(kHIDControlUUID, BLEProperty::WriteNR, BLEPermissions::OpenWrite);
  _protocolModeChar = _hidSvc.createCharacteristic(kProtocolModeUUID, BLEProperty::Read | BLEProperty::WriteNR, BLEPermissions::OpenReadWrite);

  uint8_t protocolMode = 1;  // Report Protocol Mode
  _protocolModeChar.setValue(&protocolMode, 1);

  _batteryLevelChar = _batterySvc.createCharacteristic(kBatteryLevelUUID, BLEProperty::Read | BLEProperty::Notify, BLEPermissions::OpenRead);

  // HoGP §4.3.1 / HIDS 1.0 §3: The HID Service UUID (0x1812) MUST appear in
  // the advertising data so that HID hosts can discover the device by service
  // class.  We add it automatically here so callers do not have to.
  // BLEClass::end() resets the advertising state before teardown, so after a
  // begin() + BLEHIDDevice() the only UUID in the advertising payload is this
  // one — no manual re-add or adv.reset() is required by the caller.
  BLEAdvertising adv = BLE.getAdvertising();
  adv.addServiceUUID(kHIDSvcUUID);
}

/**
 * @brief Set the HID Report Map (report descriptor) characteristic value.
 * @param map Pointer to the HID report descriptor byte array.
 * @param size Length of the report descriptor in bytes.
 */
void BLEHIDDevice::reportMap(const uint8_t *map, uint16_t size) {
  _reportMapChar.setValue(map, size);
}

/**
 * @brief Expose the Device Information service (UUID 0x180A) created in the constructor.
 * @return Handle to that service.
 */
BLEService BLEHIDDevice::deviceInfoService() {
  return _devInfoSvc;
}

/**
 * @brief Expose the HID service (UUID 0x1812) created in the constructor.
 * @return Handle to that service.
 */
BLEService BLEHIDDevice::hidService() {
  return _hidSvc;
}

/**
 * @brief Expose the Battery service (UUID 0x180F) created in the constructor.
 * @return Handle to that service.
 */
BLEService BLEHIDDevice::batteryService() {
  return _batterySvc;
}

/**
 * @brief Expose the Manufacturer Name String characteristic.
 * @return Handle to the characteristic; use the String overload of
 *         @c manufacturer to set the value.
 */
BLECharacteristic BLEHIDDevice::manufacturer() {
  return _mfgChar;
}

/**
 * @brief Set the Manufacturer Name String characteristic value.
 * @param name Manufacturer name to write into the Device Information Service.
 */
void BLEHIDDevice::manufacturer(const String &name) {
  _mfgChar.setValue(name);
}

/**
 * @brief Set the PnP ID characteristic (7 bytes: sig + vendorId + productId + version).
 * @param sig Vendor ID source (0x01 = Bluetooth SIG, 0x02 = USB IF).
 * @param vendorId Vendor ID (stored little-endian).
 * @param productId Product ID (stored little-endian).
 * @param version Product version in BCD (stored little-endian).
 */
void BLEHIDDevice::pnp(uint8_t sig, uint16_t vendorId, uint16_t productId, uint16_t version) {
  uint8_t pnpData[7];
  pnpData[0] = sig;
  pnpData[1] = vendorId & 0xFF;
  pnpData[2] = (vendorId >> 8) & 0xFF;
  pnpData[3] = productId & 0xFF;
  pnpData[4] = (productId >> 8) & 0xFF;
  pnpData[5] = version & 0xFF;
  pnpData[6] = (version >> 8) & 0xFF;
  _pnpChar.setValue(pnpData, sizeof(pnpData));
}

/**
 * @brief Set the HID Information characteristic (4 bytes: bcdHID + country + flags).
 * @param country HID country code (0x00 = not localized).
 * @param flags HID information flags (bit 0: RemoteWake, bit 1: NormallyConnectable).
 * @note bcdHID is hardcoded to 0x0111 (HID version 1.11).
 */
void BLEHIDDevice::hidInfo(uint8_t country, uint8_t flags) {
  uint8_t info[4];
  info[0] = 0x11;  // bcdHID low byte (1.11)
  info[1] = 0x01;  // bcdHID high byte
  info[2] = country;
  info[3] = flags;
  _hidInfoChar.setValue(info, sizeof(info));
}

/**
 * @brief Update the battery level and send a notification to subscribed clients.
 * @param level Battery percentage (0-100).
 * @note Delivery requires an enabled Client Characteristic Configuration
 *       (notification) on the battery level attribute; that step is a normal
 *       GATT client operation and may occur before the link is fully encrypted
 *       depending on your permission model.
 */
void BLEHIDDevice::setBatteryLevel(uint8_t level) {
  _batteryLevelChar.setValue(&level, 1);
  _batteryLevelChar.notify();
}

/**
 * @brief Expose the HID Control Point characteristic (0x2A4C).
 * @return Handle to the characteristic.
 */
BLECharacteristic BLEHIDDevice::hidControl() {
  return _hidControlChar;
}

/**
 * @brief Expose the Protocol Mode characteristic (0x2A4E).
 * @return Handle to the characteristic.
 */
BLECharacteristic BLEHIDDevice::protocolMode() {
  return _protocolModeChar;
}

// Derive the minimal open permission set that covers the requested
// properties. Keeps HID report characteristics backwards-compatible with the
// pre-refactor auto-fill behavior.
/**
 * @brief Derive the minimal open permission set from requested properties.
 * @param props The BLE property flags for the characteristic.
 * @return The corresponding open (unencrypted) BLEPermission bitmask.
 */
static BLEPermission openPermsFor(BLEProperty props) {
  BLEPermission p = BLEPermission::None;
  if (props & BLEProperty::Read) {
    p = p | BLEPermission::Read;
  }
  if ((props & BLEProperty::Write) || (props & BLEProperty::WriteNR)) {
    p = p | BLEPermission::Write;
  }
  return p;
}

/**
 * @brief Create a HID Report characteristic with a Report Reference descriptor.
 * @param svc The HID service to create the characteristic under.
 * @param props BLE property flags (Read, Write, Notify, etc.).
 * @param reportId HID Report ID written into the Report Reference descriptor.
 * @param reportType HID Report Type (Input=1, Output=2, Feature=3).
 * @return Handle to the newly created characteristic.
 * @note HID-over-GATT hosts read the Report Reference and enable notifications
 *       during enumeration; this helper uses open (unencrypted) permissions
 *       for those attributes so enumeration works before the connection is
 *       encrypted, matching typical HOGP expectations.
 */
static BLECharacteristic createReportChar(BLEService &svc, BLEProperty props, uint8_t reportId, uint8_t reportType) {
  BLECharacteristic chr = svc.createCharacteristic(kReportUUID, props, openPermsFor(props));
  BLEDescriptor refDesc = chr.createDescriptor(kReportRefDescUUID, BLEPermission::Read);
  uint8_t refValue[2] = {reportId, reportType};
  refDesc.setValue(refValue, 2);
  return chr;
}

/**
 * @brief Create an Input Report characteristic (Read + Notify) with the given report ID.
 * @param reportId HID Report ID for the Report Reference descriptor.
 * @return Handle to the new characteristic.
 */
BLECharacteristic BLEHIDDevice::inputReport(uint8_t reportId) {
  return createReportChar(_hidSvc, BLEProperty::Read | BLEProperty::Notify, reportId, HID_REPORT_TYPE_INPUT);
}

/**
 * @brief Create an Output Report characteristic (Read + Write + WriteNR) with the given report ID.
 * @param reportId HID Report ID for the Report Reference descriptor.
 * @return Handle to the new characteristic.
 */
BLECharacteristic BLEHIDDevice::outputReport(uint8_t reportId) {
  return createReportChar(_hidSvc, BLEProperty::Read | BLEProperty::Write | BLEProperty::WriteNR, reportId, HID_REPORT_TYPE_OUTPUT);
}

/**
 * @brief Create a Feature Report characteristic (Read + Write) with the given report ID.
 * @param reportId HID Report ID for the Report Reference descriptor.
 * @return Handle to the new characteristic.
 */
BLECharacteristic BLEHIDDevice::featureReport(uint8_t reportId) {
  return createReportChar(_hidSvc, BLEProperty::Read | BLEProperty::Write, reportId, HID_REPORT_TYPE_FEATURE);
}

/**
 * @brief Get or create the Boot Keyboard Input Report characteristic (Read + Notify).
 * @return Handle to the Boot Keyboard Input Report characteristic.
 * @note HIDS 1.0 §3.4 requires Read and Notify properties. Created lazily
 *       on first call; subsequent calls return the cached handle.
 */
BLECharacteristic BLEHIDDevice::bootInput() {
  if (!_bootInputChar) {
    _bootInputChar = _hidSvc.createCharacteristic(kBootInputUUID, BLEProperty::Read | BLEProperty::Notify, BLEPermissions::OpenRead);
  }
  return _bootInputChar;
}

/**
 * @brief Get or create the Boot Keyboard Output Report characteristic (Read + Write + WriteNR).
 * @return Handle to the Boot Keyboard Output Report characteristic.
 * @note Created lazily on first call; subsequent calls return the cached handle.
 */
BLECharacteristic BLEHIDDevice::bootOutput() {
  if (!_bootOutputChar) {
    _bootOutputChar = _hidSvc.createCharacteristic(kBootOutputUUID, BLEProperty::Read | BLEProperty::Write | BLEProperty::WriteNR, BLEPermissions::OpenReadWrite);
  }
  return _bootOutputChar;
}

#endif /* BLE_ENABLED */
