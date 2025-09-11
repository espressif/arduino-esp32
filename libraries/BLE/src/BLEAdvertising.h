/*
 * BLEAdvertising.h
 *
 *  Created on: Jun 21, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEADVERTISING_H_
#define COMPONENTS_CPP_UTILS_BLEADVERTISING_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include "BLEUUID.h"
#include <vector>
#include "RTOS.h"
#include "BLEUtils.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#endif

/***************************************************************************
 *                      NimBLE includes and definitions                    *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gap.h>
#include <services/gap/ble_svc_gap.h>
#include <host/ble_hs_adv.h>

#define ESP_BLE_ADV_DATA_LEN_MAX            BLE_HS_ADV_MAX_SZ
#define ESP_BLE_ADV_FLAG_LIMIT_DISC         (0x01 << 0)
#define ESP_BLE_ADV_FLAG_GEN_DISC           (0x01 << 1)
#define ESP_BLE_ADV_FLAG_BREDR_NOT_SPT      (0x01 << 2)
#define ESP_BLE_ADV_FLAG_DMT_CONTROLLER_SPT (0x01 << 3)
#define ESP_BLE_ADV_FLAG_DMT_HOST_SPT       (0x01 << 4)
#define ESP_BLE_ADV_FLAG_NON_LIMIT_DISC     (0x00)
#endif /* CONFIG_NIMBLE_ENABLED */

/***************************************************************************
 *                             NimBLE types                                *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
typedef enum {
  ESP_BLE_AD_TYPE_FLAG = 0x01,
  ESP_BLE_AD_TYPE_16SRV_PART = 0x02,
  ESP_BLE_AD_TYPE_16SRV_CMPL = 0x03,
  ESP_BLE_AD_TYPE_32SRV_PART = 0x04,
  ESP_BLE_AD_TYPE_32SRV_CMPL = 0x05,
  ESP_BLE_AD_TYPE_128SRV_PART = 0x06,
  ESP_BLE_AD_TYPE_128SRV_CMPL = 0x07,
  ESP_BLE_AD_TYPE_NAME_SHORT = 0x08,
  ESP_BLE_AD_TYPE_NAME_CMPL = 0x09,
  ESP_BLE_AD_TYPE_TX_PWR = 0x0A,
  ESP_BLE_AD_TYPE_DEV_CLASS = 0x0D,
  ESP_BLE_AD_TYPE_SM_TK = 0x10,
  ESP_BLE_AD_TYPE_SM_OOB_FLAG = 0x11,
  ESP_BLE_AD_TYPE_INT_RANGE = 0x12,
  ESP_BLE_AD_TYPE_SOL_SRV_UUID = 0x14,
  ESP_BLE_AD_TYPE_128SOL_SRV_UUID = 0x15,
  ESP_BLE_AD_TYPE_SERVICE_DATA = 0x16,
  ESP_BLE_AD_TYPE_PUBLIC_TARGET = 0x17,
  ESP_BLE_AD_TYPE_RANDOM_TARGET = 0x18,
  ESP_BLE_AD_TYPE_APPEARANCE = 0x19,
  ESP_BLE_AD_TYPE_ADV_INT = 0x1A,
  ESP_BLE_AD_TYPE_LE_DEV_ADDR = 0x1b,
  ESP_BLE_AD_TYPE_LE_ROLE = 0x1c,
  ESP_BLE_AD_TYPE_SPAIR_C256 = 0x1d,
  ESP_BLE_AD_TYPE_SPAIR_R256 = 0x1e,
  ESP_BLE_AD_TYPE_32SOL_SRV_UUID = 0x1f,
  ESP_BLE_AD_TYPE_32SERVICE_DATA = 0x20,
  ESP_BLE_AD_TYPE_128SERVICE_DATA = 0x21,
  ESP_BLE_AD_TYPE_LE_SECURE_CONFIRM = 0x22,
  ESP_BLE_AD_TYPE_LE_SECURE_RANDOM = 0x23,
  ESP_BLE_AD_TYPE_URI = 0x24,
  ESP_BLE_AD_TYPE_INDOOR_POSITION = 0x25,
  ESP_BLE_AD_TYPE_TRANS_DISC_DATA = 0x26,
  ESP_BLE_AD_TYPE_LE_SUPPORT_FEATURE = 0x27,
  ESP_BLE_AD_TYPE_CHAN_MAP_UPDATE = 0x28,
  ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE = 0xFF,
} esp_ble_adv_data_type;
#endif

/**
 * @brief Advertisement data set by the programmer to be published by the %BLE server.
 */
class BLEAdvertisementData {
public:
  /***************************************************************************
   *                           Common public declarations                    *
   ***************************************************************************/

  void setAppearance(uint16_t appearance);
  void setCompleteServices(BLEUUID uuid);
  void setFlags(uint8_t);
  void setManufacturerData(String data);
  void setName(String name);
  void setPartialServices(BLEUUID uuid);
  void setServiceData(BLEUUID uuid, String data);
  void setShortName(String name);
  void setPreferredParams(uint16_t min, uint16_t max);
  void addTxPower();
  void addData(String data);
  void addData(char *data, size_t length);
  String getPayload();

private:
  friend class BLEAdvertising;

  /***************************************************************************
   *                           Common private declarations                   *
   ***************************************************************************/

  String m_payload;
};

/**
 * @brief Perform and manage %BLE advertising.
 */
class BLEAdvertising {
public:
  /***************************************************************************
   *                           Common public declarations                    *
   ***************************************************************************/

  BLEAdvertising();
  void addServiceUUID(BLEUUID serviceUUID);
  void addServiceUUID(const char *serviceUUID);
  bool removeServiceUUID(int index);
  bool removeServiceUUID(BLEUUID serviceUUID);
  bool removeServiceUUID(const char *serviceUUID);
  bool stop();
  void reset();
  void setAppearance(uint16_t appearance);
  void setAdvertisementType(uint8_t adv_type);
  void setMaxInterval(uint16_t maxinterval);
  void setMinInterval(uint16_t mininterval);
  bool setAdvertisementData(BLEAdvertisementData &advertisementData);
  void setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly);
  bool setScanResponseData(BLEAdvertisementData &advertisementData);
  void setMinPreferred(uint16_t);
  void setMaxPreferred(uint16_t);
  void setScanResponse(bool);

  /***************************************************************************
   *                           Bluedroid public declarations                 *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void setPrivateAddress(esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM);
  bool setDeviceAddress(esp_bd_addr_t addr, esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM);
  void setAdvertisementChannelMap(esp_ble_adv_channel_t channel_map);
  void handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  bool start();
#endif

  /***************************************************************************
   *                           NimBLE public declarations                    *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  void setName(String name);
  void addTxPower();
  void advCompleteCB();
  bool isAdvertising();
  void onHostSync();
  bool start(uint32_t duration = 0, void (*advCompleteCB)(BLEAdvertising *pAdv) = nullptr);
  static int handleGAPEvent(ble_gap_event *event, void *arg);
#endif

private:
  /***************************************************************************
   *                          Common private properties                      *
   ***************************************************************************/

  std::vector<BLEUUID> m_serviceUUIDs;
  bool m_customAdvData = false;
  bool m_customScanResponseData = false;
  FreeRTOS::Semaphore m_semaphoreSetAdv = FreeRTOS::Semaphore("startAdvert");
  bool m_scanResp = true;

  /***************************************************************************
   *                          Bluedroid private properties                   *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_adv_data_t m_advData;
  esp_ble_adv_data_t m_scanRespData;
  esp_ble_adv_params_t m_advParams;
#endif

  /***************************************************************************
   *                          NimBLE private properties                      *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_adv_fields m_advData;
  ble_hs_adv_fields m_scanData;
  ble_gap_adv_params m_advParams;
  bool m_advDataSet;
  void (*m_advCompCB)(BLEAdvertising *pAdv);
  uint8_t m_slaveItvl[4];
  uint32_t m_duration;
  String m_name;
#endif
};

/***************************************************************************
 *                          Bluedroid 5.0 specific classes                 *
 ***************************************************************************/

#if defined(SOC_BLE_50_SUPPORTED) && defined(CONFIG_BLUEDROID_ENABLED)
class BLEMultiAdvertising {
public:
  BLEMultiAdvertising(uint8_t num = 1);
  ~BLEMultiAdvertising() {}

  bool setAdvertisingData(uint8_t instance, uint16_t length, const uint8_t *data);
  bool setScanRspData(uint8_t instance, uint16_t length, const uint8_t *data);
  bool start();
  bool start(uint8_t num, uint8_t from);
  void setDuration(uint8_t instance, int duration = 0, int max_events = 0);
  bool setInstanceAddress(uint8_t instance, uint8_t *rand_addr);
  bool stop(uint8_t num_adv, const uint8_t *ext_adv_inst);
  bool remove(uint8_t instance);
  bool clear();
  bool setPeriodicAdvertisingData(uint8_t instance, uint16_t length, const uint8_t *data);
  bool startPeriodicAdvertising(uint8_t instance);
  bool setAdvertisingParams(uint8_t instance, const esp_ble_gap_ext_adv_params_t *params);
  bool setPeriodicAdvertisingParams(uint8_t instance, const esp_ble_gap_periodic_adv_params_t *params);

private:
  esp_ble_gap_ext_adv_params_t *params_arrays;
  esp_ble_gap_ext_adv_t *ext_adv;
  uint8_t count;
};
#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEADVERTISING_H_ */
