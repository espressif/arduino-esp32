/*
 * BLEAdvertising.h
 *
 *  Created on: Jun 21, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEADVERTISING_H_
#define COMPONENTS_CPP_UTILS_BLEADVERTISING_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#include "BLEUUID.h"
#include <vector>
#include "RTOS.h"

/**
 * @brief Advertisement data set by the programmer to be published by the %BLE server.
 */
class BLEAdvertisementData {
  // Only a subset of the possible BLE architected advertisement fields are currently exposed.  Others will
  // be exposed on demand/request or as time permits.
  //
public:
  void setAppearance(uint16_t appearance);
  void setCompleteServices(BLEUUID uuid);
  void setFlags(uint8_t);
  void setManufacturerData(String data);
  void setName(String name);
  void setPartialServices(BLEUUID uuid);
  void setServiceData(BLEUUID uuid, String data);
  void setShortName(String name);
  void addData(String data);  // Add data to the payload.
  String getPayload();        // Retrieve the current advert payload.

private:
  friend class BLEAdvertising;
  String m_payload;  // The payload of the advertisement.
};  // BLEAdvertisementData

/**
 * @brief Perform and manage %BLE advertising.
 *
 * A %BLE server will want to perform advertising in order to make itself known to %BLE clients.
 */
class BLEAdvertising {
public:
  BLEAdvertising();
  void addServiceUUID(BLEUUID serviceUUID);
  void addServiceUUID(const char *serviceUUID);
  bool removeServiceUUID(int index);
  bool removeServiceUUID(BLEUUID serviceUUID);
  bool removeServiceUUID(const char *serviceUUID);
  void start();
  void stop();
  void setAppearance(uint16_t appearance);
  void setAdvertisementType(esp_ble_adv_type_t adv_type);
  void setAdvertisementChannelMap(esp_ble_adv_channel_t channel_map);
  void setMaxInterval(uint16_t maxinterval);
  void setMinInterval(uint16_t mininterval);
  void setAdvertisementData(BLEAdvertisementData &advertisementData);
  void setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly);
  void setScanResponseData(BLEAdvertisementData &advertisementData);
  void setPrivateAddress(esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM);
  void setDeviceAddress(esp_bd_addr_t addr, esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM);

  void handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void setMinPreferred(uint16_t);
  void setMaxPreferred(uint16_t);
  void setScanResponse(bool);

private:
  esp_ble_adv_data_t m_advData;
  esp_ble_adv_data_t m_scanRespData;  // Used for configuration of scan response data when m_scanResp is true
  esp_ble_adv_params_t m_advParams;
  std::vector<BLEUUID> m_serviceUUIDs;
  bool m_customAdvData = false;           // Are we using custom advertising data?
  bool m_customScanResponseData = false;  // Are we using custom scan response data?
  FreeRTOS::Semaphore m_semaphoreSetAdv = FreeRTOS::Semaphore("startAdvert");
  bool m_scanResp = true;
};

#ifdef SOC_BLE_50_SUPPORTED

class BLEMultiAdvertising {
private:
  esp_ble_gap_ext_adv_params_t *params_arrays;
  esp_ble_gap_ext_adv_t *ext_adv;
  uint8_t count;

public:
  BLEMultiAdvertising(uint8_t num = 1);
  ~BLEMultiAdvertising() {}

  bool setAdvertisingParams(uint8_t instance, const esp_ble_gap_ext_adv_params_t *params);
  bool setAdvertisingData(uint8_t instance, uint16_t length, const uint8_t *data);
  bool setScanRspData(uint8_t instance, uint16_t length, const uint8_t *data);
  bool start();
  bool start(uint8_t num, uint8_t from);
  void setDuration(uint8_t instance, int duration = 0, int max_events = 0);
  bool setInstanceAddress(uint8_t instance, esp_bd_addr_t rand_addr);
  bool stop(uint8_t num_adv, const uint8_t *ext_adv_inst);
  bool remove(uint8_t instance);
  bool clear();
  bool setPeriodicAdvertisingParams(uint8_t instance, const esp_ble_gap_periodic_adv_params_t *params);
  bool setPeriodicAdvertisingData(uint8_t instance, uint16_t length, const uint8_t *data);
  bool startPeriodicAdvertising(uint8_t instance);
};

#endif  // SOC_BLE_50_SUPPORTED

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLEADVERTISING_H_ */
