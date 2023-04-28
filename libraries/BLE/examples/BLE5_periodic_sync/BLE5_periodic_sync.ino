/*
   BLE5 extended scan example for esp32 C3 and S3
   with this code it is simple to scan legacy (BLE4) compatible advertising,
   and BLE5 extended advertising. New coded added in BLEScan is not changing old behavior,
   which can be used with old esp32, but is adding functionality to use on C3/S3.
   With this new API advertised device wont be stored in API, it is now user responsibility

   author: chegewara
*/
#ifndef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#warning "Not compatible hardware"
#else
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

BLEScan *pBLEScan;
static bool periodic_sync = false;

static esp_ble_gap_periodic_adv_sync_params_t periodic_adv_sync_params = {
    .filter_policy = 0,
    .sid = 0,
    .addr_type = BLE_ADDR_TYPE_RANDOM,
    .skip = 10,
    .sync_timeout = 1000, // timeout: 1000 * 10ms
};

class MyBLEExtAdvertisingCallbacks : public BLEExtAdvertisingCallbacks
{
  void onResult(esp_ble_gap_ext_adv_reprot_t params)
  {
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    adv_name = esp_ble_resolve_adv_data(params.adv_data, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
    if ((adv_name != NULL) && (memcmp(adv_name, "ESP_MULTI_ADV_2M", adv_name_len) == 0) && !periodic_sync)
    {
      periodic_sync = true;
      char adv_temp_name[60] = {'0'};
      memcpy(adv_temp_name, adv_name, adv_name_len);
      log_i("Start create sync with the peer device %s", adv_temp_name);
      periodic_adv_sync_params.sid = params.sid;
      //          periodic_adv_sync_params.addr_type = params.addr_type;
      memcpy(periodic_adv_sync_params.addr, params.addr, sizeof(esp_bd_addr_t));
      esp_ble_gap_periodic_adv_create_sync(&periodic_adv_sync_params);
    }
  }
};

class MyPeriodicScan : public BLEPeriodicScanCallbacks
{
  // void onCreateSync(esp_bt_status_t status){}
  // void onCancelSync(esp_bt_status_t status){}
  // void onTerminateSync(esp_bt_status_t status){}

  void onStop(esp_bt_status_t status)
  {
    log_i("ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT");
    periodic_sync = false;
    pBLEScan->startExtScan(0, 0); // scan duration in n * 10ms, period - repeat after n seconds (period >= duration)
  }

  void onLostSync(uint16_t sync_handle)
  {
    log_i("ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT");
    esp_ble_gap_stop_ext_scan();
  }

  void onSync(esp_ble_periodic_adv_sync_estab_param_t params)
  {
    log_i("ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT, status %d", params.status);
    // esp_log_buffer_hex("sync addr", param->periodic_adv_sync_estab.adv_addr, 6);
    log_i("sync handle %d sid %d perioic adv interval %d adv phy %d", params.sync_handle,
          params.sid,
          params.period_adv_interval,
          params.adv_phy);
  }

  void onReport(esp_ble_gap_periodic_adv_report_t params)
  {
    log_i("periodic adv report, sync handle %d data status %d data len %d rssi %d", params.sync_handle,
          params.data_status,
          params.data_length,
          params.rssi);
  }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Periodic scan...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setExtendedScanCallback(new MyBLEExtAdvertisingCallbacks());
  pBLEScan->setExtScanParams(); // use with pre-defined/default values, overloaded function allows to pass parameters
  pBLEScan->setPeriodicScanCallback(new MyPeriodicScan());
  delay(100);                         // it is just for simplicity this example, to let ble stack to set extended scan params
  pBLEScan->startExtScan(0, 0);

}

void loop()
{
  delay(2000);
}

#endif // CONFIG_BT_BLE_50_FEATURES_SUPPORTED
