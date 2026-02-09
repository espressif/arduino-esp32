/*
 * BLEHIDDevice.cpp
 *
 *  Created on: Jan 03, 2018
 *      Author: chegewara
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include "BLEHIDDevice.h"
#include "BLE2904.h"
#include "BLEDescriptor.h"

/***************************************************************************
 *                     NimBLE includes and definitions                     *
 ***************************************************************************/

#ifdef CONFIG_NIMBLE_ENABLED
#include <host/ble_att.h>
#endif

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

BLEHIDDevice::BLEHIDDevice(BLEServer *server) {
  m_server = server;
  /*
	 * Here we create mandatory services described in bluetooth specification
	 */
  m_deviceInfoService = server->createService(BLEUUID((uint16_t)0x180a));
  m_hidService = server->createService(BLEUUID((uint16_t)0x1812), 40);
  m_batteryService = server->createService(BLEUUID((uint16_t)0x180f));

  /*
	 * Mandatory characteristic for device info service
	 */
  m_pnpCharacteristic = m_deviceInfoService->createCharacteristic((uint16_t)0x2a50, BLECharacteristic::PROPERTY_READ);

  /*
	 * Mandatory characteristics for HID service
	 */
  m_hidInfoCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a4a, BLECharacteristic::PROPERTY_READ);
  m_reportMapCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a4b, BLECharacteristic::PROPERTY_READ);
  m_hidControlCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a4c, BLECharacteristic::PROPERTY_WRITE_NR);
  m_protocolModeCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a4e, BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_READ);

  /*
	 * Mandatory battery level characteristic with notification and presence descriptor
	 */
  BLE2904 *batteryLevelDescriptor = new BLE2904();
  batteryLevelDescriptor->setFormat(BLE2904::FORMAT_UINT8);
  batteryLevelDescriptor->setNamespace(1);
  batteryLevelDescriptor->setUnit(0x27ad);

  m_batteryLevelCharacteristic =
    m_batteryService->createCharacteristic((uint16_t)0x2a19, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  m_batteryLevelCharacteristic->addDescriptor(batteryLevelDescriptor);
#if CONFIG_BLUEDROID_ENABLED
  BLE2902 *batLevelIndicator = new BLE2902();
  // Battery Level Notification is ON by default, making it work always on BLE Pairing and Bonding
  batLevelIndicator->setNotifications(true);
  // IMPORTANT: CCCD must be accessible without encryption for HID enumeration
  batLevelIndicator->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  m_batteryLevelCharacteristic->addDescriptor(batLevelIndicator);
#endif

  /*
	 * This value is setup here because its default value in most usage cases, its very rare to use boot mode
	 * and we want to simplify library using as much as possible
	 */
  const uint8_t pMode[] = {0x01};
  protocolMode()->setValue((uint8_t *)pMode, 1);
}

BLEHIDDevice::~BLEHIDDevice() {}

/*
 * @brief
 */
void BLEHIDDevice::reportMap(uint8_t *map, uint16_t size) {
  m_reportMapCharacteristic->setValue(map, size);
}

/*
 * @brief This function suppose to be called at the end, when we have created all characteristics we need to build HID service
 */
void BLEHIDDevice::startServices() {
  m_deviceInfoService->start();
  m_hidService->start();
  m_batteryService->start();
  m_server->start();
}

/*
 * @brief Create manufacturer characteristic (this characteristic is optional)
 */
BLECharacteristic *BLEHIDDevice::manufacturer() {
  m_manufacturerCharacteristic = m_deviceInfoService->createCharacteristic((uint16_t)0x2a29, BLECharacteristic::PROPERTY_READ);
  return m_manufacturerCharacteristic;
}

/*
 * @brief Set manufacturer name
 * @param [in] name manufacturer name
 */
void BLEHIDDevice::manufacturer(String name) {
  m_manufacturerCharacteristic->setValue(name);
}

/*
 * @brief
 */
void BLEHIDDevice::pnp(uint8_t sig, uint16_t vid, uint16_t pid, uint16_t version) {
  uint8_t pnp[] = {sig, (uint8_t)(vid >> 8), (uint8_t)vid, (uint8_t)(pid >> 8), (uint8_t)pid, (uint8_t)(version >> 8), (uint8_t)version};
  m_pnpCharacteristic->setValue(pnp, sizeof(pnp));
}

/*
 * @brief
 */
void BLEHIDDevice::hidInfo(uint8_t country, uint8_t flags) {
  uint8_t info[] = {0x11, 0x1, country, flags};
  m_hidInfoCharacteristic->setValue(info, sizeof(info));
}

/*
 * @brief Create input report characteristic that need to be saved as new characteristic object so can be further used
 * @param [in] reportID input report ID, the same as in report map for input object related to created characteristic
 * @return pointer to new input report characteristic
 */
BLECharacteristic *BLEHIDDevice::inputReport(uint8_t reportID) {
  // Note: READ_ENC removed per HOGP specification - characteristics must be readable without encryption for enumeration
  // Actual report data is still encrypted via BLE connection encryption after pairing
  uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY;
  // For NimBLE: Characteristic encryption properties can be added if needed
  // For Bluedroid: Standard properties, permissions set separately below

  BLECharacteristic *inputReportCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a4d, properties);
  BLEDescriptor *inputReportDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2908));

  // For Bluedroid: Set access permissions (ignored by NimBLE, but doesn't hurt)
  inputReportCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // IMPORTANT: Report Reference Descriptor must be readable without encryption per HOGP specification
  // HID hosts must read Report ID and Report Type during enumeration (before encryption is established)
  // The descriptor only contains metadata; actual HID reports are encrypted via BLE connection
  inputReportDescriptor->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);

  uint8_t desc1_val[] = {reportID, 0x01};
  inputReportDescriptor->setValue((uint8_t *)desc1_val, 2);
  inputReportCharacteristic->addDescriptor(inputReportDescriptor);

#if CONFIG_BLUEDROID_ENABLED
  BLE2902 *p2902 = new BLE2902();
  // IMPORTANT: CCCD must be readable/writable without encryption for HID enumeration
  // Host needs to enable notifications before encryption is established
  p2902->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  inputReportCharacteristic->addDescriptor(p2902);
#endif

  return inputReportCharacteristic;
}

/*
 * @brief Create output report characteristic that need to be saved as new characteristic object so can be further used
 * @param [in] reportID Output report ID, the same as in report map for output object related to created characteristic
 * @return Pointer to new output report characteristic
 */
BLECharacteristic *BLEHIDDevice::outputReport(uint8_t reportID) {
  // Note: Encryption properties removed per HOGP specification - characteristics must be readable without encryption for enumeration
  // Actual report data is still encrypted via BLE connection encryption after pairing
  uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR;
  // For NimBLE: Characteristic encryption properties can be added if needed
  // For Bluedroid: Standard properties, permissions set separately below

  BLECharacteristic *outputReportCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a4d, properties);
  BLEDescriptor *outputReportDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2908));

  // For Bluedroid: Set access permissions (ignored by NimBLE, but doesn't hurt)
  outputReportCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // IMPORTANT: Report Reference Descriptor must be readable without encryption for HID enumeration
  outputReportDescriptor->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);

  uint8_t desc1_val[] = {reportID, 0x02};
  outputReportDescriptor->setValue((uint8_t *)desc1_val, 2);
  outputReportCharacteristic->addDescriptor(outputReportDescriptor);

  return outputReportCharacteristic;
}

/*
 * @brief Create feature report characteristic that need to be saved as new characteristic object so can be further used
 * @param [in] reportID Feature report ID, the same as in report map for feature object related to created characteristic
 * @return Pointer to new feature report characteristic
 */
BLECharacteristic *BLEHIDDevice::featureReport(uint8_t reportID) {
  // Note: Encryption properties removed per HOGP specification - characteristics must be readable without encryption for enumeration
  // Actual report data is still encrypted via BLE connection encryption after pairing
  uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE;
  // For NimBLE: Characteristic encryption properties can be added if needed
  // For Bluedroid: Standard properties, permissions set separately below

  BLECharacteristic *featureReportCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a4d, properties);
  BLEDescriptor *featureReportDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2908));

  // For Bluedroid: Set access permissions (ignored by NimBLE, but doesn't hurt)
  featureReportCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // IMPORTANT: Report Reference Descriptor must be readable without encryption for HID enumeration
  featureReportDescriptor->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);

  uint8_t desc1_val[] = {reportID, 0x03};
  featureReportDescriptor->setValue((uint8_t *)desc1_val, 2);
  featureReportCharacteristic->addDescriptor(featureReportDescriptor);

  return featureReportCharacteristic;
}

/*
 * @brief Create boot input characteristic
 */
BLECharacteristic *BLEHIDDevice::bootInput() {
  // Note: READ_ENC removed to match input report behavior
  // Boot mode characteristics follow same security model as report mode
  uint32_t properties = BLECharacteristic::PROPERTY_NOTIFY;

  BLECharacteristic *bootInputCharacteristic = m_hidService->createCharacteristic((uint16_t)0x2a22, properties);
#if CONFIG_BLUEDROID_ENABLED
  BLE2902 *bootInputCCCD = new BLE2902();
  // IMPORTANT: CCCD must be accessible without encryption for HID enumeration
  bootInputCCCD->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  bootInputCharacteristic->addDescriptor(bootInputCCCD);
#endif

  return bootInputCharacteristic;
}

/*
 * @brief
 */
BLECharacteristic *BLEHIDDevice::bootOutput() {
  return m_hidService->createCharacteristic(
    (uint16_t)0x2a32, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
}

/*
 * @brief
 */
BLECharacteristic *BLEHIDDevice::hidControl() {
  return m_hidControlCharacteristic;
}

/*
 * @brief
 */
BLECharacteristic *BLEHIDDevice::protocolMode() {
  return m_protocolModeCharacteristic;
}

void BLEHIDDevice::setBatteryLevel(uint8_t level) {
  m_batteryLevelCharacteristic->setValue(&level, 1);
  if (m_server->isStarted()) {
    m_batteryLevelCharacteristic->notify();
  }
}
/*
 * @brief Returns battery level characteristic
 * @ return battery level characteristic
 */
/*
BLECharacteristic* BLEHIDDevice::batteryLevel() {
	return m_batteryLevelCharacteristic;
}



BLECharacteristic*	 BLEHIDDevice::reportMap() {
	return m_reportMapCharacteristic;
}

BLECharacteristic*	 BLEHIDDevice::pnp() {
	return m_pnpCharacteristic;
}


BLECharacteristic*	BLEHIDDevice::hidInfo() {
	return m_hidInfoCharacteristic;
}
*/
/*
 * @brief
 */
BLEService *BLEHIDDevice::deviceInfo() {
  return m_deviceInfoService;
}

/*
 * @brief
 */
BLEService *BLEHIDDevice::hidService() {
  return m_hidService;
}

/*
 * @brief
 */
BLEService *BLEHIDDevice::batteryService() {
  return m_batteryService;
}

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
