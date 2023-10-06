/*
   Simple BLE5 multi advertising example on esp32 C3/S3
   only ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND is backward compatible
   and can be scanned with BLE4.2 devices

   author: chegewara
*/

#ifndef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
  #error "This SoC does not support BLE5. Try using ESP32-C3, or ESP32-S3"
#else

#include <BLEDevice.h>
#include <BLEAdvertising.h>

esp_ble_gap_ext_adv_params_t ext_adv_params_1M = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE,
    .interval_min = 0x30,
    .interval_max = 0x30,
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr = {0,0,0,0,0,0},
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
    .primary_phy = ESP_BLE_GAP_PHY_CODED,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_1M,
    .sid = 0,
    .scan_req_notif = false,
};

esp_ble_gap_ext_adv_params_t ext_adv_params_2M = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_SCANNABLE,
    .interval_min = 0x40,
    .interval_max = 0x40,
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr = {0,0,0,0,0,0},
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_2M,
    .sid = 1,
    .scan_req_notif = false,
};

esp_ble_gap_ext_adv_params_t legacy_adv_params = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
    .interval_min = 0x45,
    .interval_max = 0x45,
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr = {0,0,0,0,0,0},
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_1M,
    .sid = 2,
    .scan_req_notif = false,
};

esp_ble_gap_ext_adv_params_t ext_adv_params_coded = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_SCANNABLE,
    .interval_min = 0x50,
    .interval_max = 0x50,
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr_type = BLE_ADDR_TYPE_RANDOM,
    .peer_addr = {0,0,0,0,0,0},
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
    .primary_phy = ESP_BLE_GAP_PHY_1M,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_CODED,
    .sid = 3,
    .scan_req_notif = false,
};

static uint8_t raw_adv_data_1m[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb,
        0x12, 0x09, 'E', 'S', 'P', '_', 'M', 'U', 'L', 'T', 'I', '_', 'A',
        'D', 'V', '_', '1', 'M', 0X0
};

static uint8_t raw_scan_rsp_data_2m[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb,
        0x12, 0x09, 'E', 'S', 'P', '_', 'M', 'U', 'L', 'T', 'I', '_', 'A',
        'D', 'V', '_', '2', 'M', 0X0
};

static uint8_t legacy_adv_data[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb,
        0x15, 0x09, 'E', 'S', 'P', '_', 'M', 'U', 'L', 'T', 'I', '_', 'A',
        'D', 'V', '_', 'C', 'O', 'D', 'E', 'D', 0X0
};

static uint8_t legacy_scan_rsp_data[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb,
        0x16, 0x09, 'E', 'S', 'P', '_', 'M', 'U', 'L', 'T', 'I', '_', 'A',
        'D', 'V', '_', 'L', 'E', 'G', 'A', 'C', 'Y', 0X0
};

static uint8_t raw_scan_rsp_data_coded[] = {
        0x37, 0x09, 'V', 'E', 'R', 'Y', '_', 'L', 'O', 'N', 'G', '_', 'D', 'E', 'V', 'I', 'C', 'E', '_', 'N', 'A', 'M', 'E', '_',
        'S', 'E', 'N', 'T', '_', 'U', 'S', 'I', 'N', 'G', '_', 'E', 'X', 'T', 'E', 'N', 'D', 'E', 'D', '_', 'A', 'D', 'V', 'E', 'R', 'T', 'I', 'S', 'I', 'N', 'G', 0X0
};


uint8_t addr_1m[6] = {0xc0, 0xde, 0x52, 0x00, 0x00, 0x01};
uint8_t addr_2m[6] = {0xc0, 0xde, 0x52, 0x00, 0x00, 0x02};
uint8_t addr_legacy[6] = {0xc0, 0xde, 0x52, 0x00, 0x00, 0x03};
uint8_t addr_coded[6] = {0xc0, 0xde, 0x52, 0x00, 0x00, 0x04};

BLEMultiAdvertising advert(4); // max number of advertisement data 

void setup() {
  Serial.begin(115200);
  Serial.println("Multi-Advertising...");

  BLEDevice::init("");

  advert.setAdvertisingParams(0, &ext_adv_params_1M);
  advert.setAdvertisingData(0, sizeof(raw_adv_data_1m), &raw_adv_data_1m[0]);
  advert.setInstanceAddress(0, addr_1m);
  advert.setDuration(0);

  advert.setAdvertisingParams(1, &ext_adv_params_2M);
  advert.setScanRspData(1, sizeof(raw_scan_rsp_data_2m), &raw_scan_rsp_data_2m[0]);
  advert.setInstanceAddress(1, addr_2m);
  advert.setDuration(1);

  advert.setAdvertisingParams(2, &legacy_adv_params);
  advert.setAdvertisingData(2, sizeof(legacy_adv_data), &legacy_adv_data[0]);
  advert.setScanRspData(2, sizeof(legacy_scan_rsp_data), &legacy_scan_rsp_data[0]);
  advert.setInstanceAddress(2, addr_legacy);
  advert.setDuration(2);

  advert.setAdvertisingParams(3, &ext_adv_params_coded);
  advert.setDuration(3);
  advert.setScanRspData(3, sizeof(raw_scan_rsp_data_coded), &raw_scan_rsp_data_coded[0]);
  advert.setInstanceAddress(3, addr_coded);

  delay(1000);
  advert.start(4, 0);
}

void loop() {
  delay(2000);
}
#endif