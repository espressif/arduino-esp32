// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <app/server/Server.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace esp_matter::identification;
using namespace chip::app::Clusters;

constexpr auto k_timeout_seconds = 300;

static bool _matter_has_started = false;
static node::config_t node_config;
static node_t *deviceNode = NULL;

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(
  attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data
) {
  log_d("Attribute update callback: type: %u, endpoint: %u, cluster: %u, attribute: %u, val: %u", type, endpoint_id, cluster_id, attribute_id, val->val.u32);
  esp_err_t err = ESP_OK;
  MatterEndPoint *ep = (MatterEndPoint *)priv_data;  // endpoint pointer to base class
  switch (type) {
    case PRE_UPDATE:  // Callback before updating the value in the database
      log_v("Attribute update callback: PRE_UPDATE");
      if (ep != NULL) {
        err = ep->attributeChangeCB(endpoint_id, cluster_id, attribute_id, val) ? ESP_OK : ESP_FAIL;
      }
      break;
    case POST_UPDATE:  // Callback after updating the value in the database
      log_v("Attribute update callback: POST_UPDATE");
      break;
    case READ:  // Callback for reading the attribute value. This is used when the `ATTRIBUTE_FLAG_OVERRIDE` is set.
      log_v("Attribute update callback: READ");
      break;
    case WRITE:  // Callback for writing the attribute value. This is used when the `ATTRIBUTE_FLAG_OVERRIDE` is set.
      log_v("Attribute update callback: WRITE");
      break;
    default: log_v("Attribute update callback: Unknown type %d", type);
  }
  return err;
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
  log_d("Identification callback to endpoint %d: type: %u, effect: %u, variant: %u", endpoint_id, type, effect_id, effect_variant);
  esp_err_t err = ESP_OK;
  MatterEndPoint *ep = (MatterEndPoint *)priv_data;  // endpoint pointer to base class
  // Identify the endpoint sending a counter to the application
  bool identifyIsActive = false;

  if (type == identification::callback_type_t::START) {
    log_v("Identification callback: START");
    identifyIsActive = true;
  } else if (type == identification::callback_type_t::EFFECT) {
    log_v("Identification callback: EFFECT");
  } else if (type == identification::callback_type_t::STOP) {
    identifyIsActive = false;
    log_v("Identification callback: STOP");
  }
  if (ep != NULL) {
    err = ep->endpointIdentifyCB(endpoint_id, identifyIsActive) ? ESP_OK : ESP_FAIL;
  }

  return err;
}

// This callback is invoked for all Matter events. The application can handle the events as required.
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
  switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
      log_i(
        "Interface %s Address changed", event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Assigned ? "IPv4" : "IPV6"
      );
      break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:       log_i("Commissioning complete"); break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:        log_i("Commissioning failed, fail safe timer expired"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted: log_i("Commissioning session started"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped: log_i("Commissioning session stopped"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:   log_i("Commissioning window opened"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:   log_i("Commissioning window closed"); break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
    {
      log_i("Fabric removed successfully");
      if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
        log_i("No fabric left, opening commissioning window");
        chip::CommissioningWindowManager &commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
        constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
        if (!commissionMgr.IsCommissioningWindowOpen()) {
          // After removing last fabric, it does not remove the Wi-Fi credentials and still has IP connectivity so, only advertising on DNS-SD.
          CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds, chip::CommissioningWindowAdvertisement::kDnssdOnly);
          if (err != CHIP_NO_ERROR) {
            log_e("Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
          }
        }
      }
      break;
    }
    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved: log_i("Fabric will be removed"); break;
    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:       log_i("Fabric is updated"); break;
    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:     log_i("Fabric is committed"); break;
    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:    log_i("BLE deinitialized and memory reclaimed"); break;
    default:                                                       break;
  }
}

void ArduinoMatter::_init() {
  if (_matter_has_started) {
    return;
  }

  // Create a Matter node and add the mandatory Root Node device type on endpoint 0
  // node handle can be used to add/modify other endpoints.
  deviceNode = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
  if (deviceNode == nullptr) {
    log_e("Failed to create Matter node");
    return;
  }

  _matter_has_started = true;
}

void ArduinoMatter::begin() {
  if (!_matter_has_started) {
    log_e("No Matter endpoint has been created. Please create an endpoint first.");
    return;
  }

  /* Matter start */
  esp_err_t err = esp_matter::start(app_event_cb);
  if (err != ESP_OK) {
    log_e("Failed to start Matter, err:%d", err);
    _matter_has_started = false;
  }
}

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
bool ArduinoMatter::isThreadConnected() {
  return false;  // Thread Network TBD
}
#endif

bool ArduinoMatter::isDeviceCommissioned() {
  return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
bool ArduinoMatter::isWiFiConnected() {
  return chip::DeviceLayer::ConnectivityMgr().IsWiFiStationConnected();
}
#endif

bool ArduinoMatter::isDeviceConnected() {
  bool retCode = false;
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  retCode |= ArduinoMatter::isThreadConnected();
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  retCode |= ArduinoMatter::isWiFiConnected();
#endif
  return retCode;
}

void ArduinoMatter::decommission() {
  esp_matter::factory_reset();
}

// Global Matter Object
ArduinoMatter Matter;

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
