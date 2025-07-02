/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <Arduino.h>
#include "matter_accessory_driver.h"

#include <esp_err.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#include "esp_openthread_types.h"

#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = RADIO_MODE_NATIVE, }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = HOST_CONNECTION_MODE_NONE, }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG() \
  { .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, }
#endif

// set your board button pin here
const uint8_t button_gpio = BUTTON_PIN;  // GPIO BOOT Button

uint16_t light_endpoint_id = 0;

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

constexpr auto k_timeout_seconds = 300;

#if CONFIG_ENABLE_ENCRYPTED_OTA
extern const char decryption_key_start[] asm("_binary_esp_image_encryption_key_pem_start");
extern const char decryption_key_end[] asm("_binary_esp_image_encryption_key_pem_end");

static const char *s_decryption_key = decryption_key_start;
static const uint16_t s_decryption_key_len = decryption_key_end - decryption_key_start;
#endif  // CONFIG_ENABLE_ENCRYPTED_OTA

bool isAccessoryCommissioned() {
  return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
bool isWifiConnected() {
  return chip::DeviceLayer::ConnectivityMgr().IsWiFiStationConnected();
}
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
bool isThreadConnected() {
  return chip::DeviceLayer::ConnectivityMgr().IsThreadAttached();
}
#endif

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
  switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
      log_i(
        "Interface %s Address changed", event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Assigned ? "IPv4" : "IPV6"
      );
      break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete: log_i("Commissioning complete"); break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired: log_i("Commissioning failed, fail safe timer expired"); break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted: log_i("Commissioning session started"); break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped: log_i("Commissioning session stopped"); break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened: log_i("Commissioning window opened"); break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed: log_i("Commissioning window closed"); break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
    {
      log_i("Fabric removed successfully");
      if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
        chip::CommissioningWindowManager &commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
        constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
        if (!commissionMgr.IsCommissioningWindowOpen()) {
          /* After removing last fabric, this example does not remove the Wi-Fi credentials
                     * and still has IP connectivity so, only advertising on DNS-SD.
                     */
          CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds, chip::CommissioningWindowAdvertisement::kDnssdOnly);
          if (err != CHIP_NO_ERROR) {
            log_e("Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
          }
        }
      }
      break;
    }

    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved: log_i("Fabric will be removed"); break;

    case chip::DeviceLayer::DeviceEventType::kFabricUpdated: log_i("Fabric is updated"); break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted: log_i("Fabric is committed"); break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized: log_i("BLE deinitialized and memory reclaimed"); break;

    default: break;
  }
}

esp_err_t matter_light_attribute_update(
  app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val
) {
  esp_err_t err = ESP_OK;
  if (endpoint_id == light_endpoint_id) {
    void *led = (void *)driver_handle;
    if (cluster_id == OnOff::Id) {
      if (attribute_id == OnOff::Attributes::OnOff::Id) {
        err = light_accessory_set_power(led, val->val.b);
      }
    } else if (cluster_id == LevelControl::Id) {
      if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
        err = light_accessory_set_brightness(led, val->val.u8);
      }
    } else if (cluster_id == ColorControl::Id) {
      if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
        err = light_accessory_set_hue(led, val->val.u8);
      } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
        err = light_accessory_set_saturation(led, val->val.u8);
      } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
        err = light_accessory_set_temperature(led, val->val.u16);
      }
    }
  }
  return err;
}

esp_err_t matter_light_set_defaults(uint16_t endpoint_id) {
  esp_err_t err = ESP_OK;

  void *led = endpoint::get_priv_data(endpoint_id);
  node_t *node = node::get();
  endpoint_t *endpoint = endpoint::get(node, endpoint_id);
  cluster_t *cluster = NULL;
  attribute_t *attribute = NULL;
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);

  /* Setting brightness */
  cluster = cluster::get(endpoint, LevelControl::Id);
  attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);
  attribute::get_val(attribute, &val);
  err |= light_accessory_set_brightness(led, val.val.u8);

  /* Setting color */
  cluster = cluster::get(endpoint, ColorControl::Id);
  attribute = attribute::get(cluster, ColorControl::Attributes::ColorMode::Id);
  attribute::get_val(attribute, &val);
  if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation) {
    /* Setting hue */
    attribute = attribute::get(cluster, ColorControl::Attributes::CurrentHue::Id);
    attribute::get_val(attribute, &val);
    err |= light_accessory_set_hue(led, val.val.u8);
    /* Setting saturation */
    attribute = attribute::get(cluster, ColorControl::Attributes::CurrentSaturation::Id);
    attribute::get_val(attribute, &val);
    err |= light_accessory_set_saturation(led, val.val.u8);
  } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kColorTemperature) {
    /* Setting temperature */
    attribute = attribute::get(cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);
    attribute::get_val(attribute, &val);
    err |= light_accessory_set_temperature(led, val.val.u16);
  } else {
    log_e("Color mode not supported");
  }

  /* Setting power */
  cluster = cluster::get(endpoint, OnOff::Id);
  attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);
  attribute::get_val(attribute, &val);
  err |= light_accessory_set_power(led, val.val.b);

  return err;
}

void button_driver_init() {
  /* Initialize button */
  pinMode(button_gpio, INPUT_PULLUP);
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(
  attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data
) {
  esp_err_t err = ESP_OK;

  if (type == PRE_UPDATE) {
    /* Driver update */
    app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
    err = matter_light_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
  }

  return err;
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
  log_i("Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
  return ESP_OK;
}

void setup() {
  esp_err_t err = ESP_OK;

  /* Initialize driver */
  app_driver_handle_t light_handle = light_accessory_init();
  button_driver_init();

  /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
  node::config_t node_config;

  // node handle can be used to add/modify other endpoints.
  node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
  if (node == nullptr) {
    log_e("Failed to create Matter node");
    abort();
  }

  extended_color_light::config_t light_config;
  light_config.on_off.on_off = DEFAULT_POWER;
  light_config.on_off.lighting.start_up_on_off = nullptr;
  light_config.level_control.current_level = DEFAULT_BRIGHTNESS;
  light_config.level_control.lighting.start_up_current_level = DEFAULT_BRIGHTNESS;
  light_config.color_control.color_mode = (uint8_t)ColorControl::ColorMode::kColorTemperature;
  light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::kColorTemperature;
  light_config.color_control.color_temperature.startup_color_temperature_mireds = nullptr;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = extended_color_light::create(node, &light_config, ENDPOINT_FLAG_NONE, light_handle);
  if (endpoint == nullptr) {
    log_e("Failed to create extended color light endpoint");
    abort();
  }

  light_endpoint_id = endpoint::get_id(endpoint);
  log_i("Light created with endpoint_id %d", light_endpoint_id);

  /* Mark deferred persistence for some attributes that might be changed rapidly */
  cluster_t *level_control_cluster = cluster::get(endpoint, LevelControl::Id);
  attribute_t *current_level_attribute = attribute::get(level_control_cluster, LevelControl::Attributes::CurrentLevel::Id);
  attribute::set_deferred_persistence(current_level_attribute);

  cluster_t *color_control_cluster = cluster::get(endpoint, ColorControl::Id);
  attribute_t *current_x_attribute = attribute::get(color_control_cluster, ColorControl::Attributes::CurrentX::Id);
  attribute::set_deferred_persistence(current_x_attribute);
  attribute_t *current_y_attribute = attribute::get(color_control_cluster, ColorControl::Attributes::CurrentY::Id);  // codespell:ignore
  attribute::set_deferred_persistence(current_y_attribute);
  attribute_t *color_temp_attribute = attribute::get(color_control_cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);
  attribute::set_deferred_persistence(color_temp_attribute);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  /* Set OpenThread platform config */
  esp_openthread_platform_config_t config = {
    .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
    .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
    .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
  };
  set_openthread_platform_config(&config);
#endif

  /* Matter start */
  err = esp_matter::start(app_event_cb);
  if (err != ESP_OK) {
    log_e("Failed to start Matter, err:%d", err);
    abort();
  }

#if CONFIG_ENABLE_ENCRYPTED_OTA
  err = esp_matter_ota_requestor_encrypted_init(s_decryption_key, s_decryption_key_len);
  if (err != ESP_OK) {
    log_e("Failed to initialized the encrypted OTA, err: %d", err);
    abort();
  }
#endif  // CONFIG_ENABLE_ENCRYPTED_OTA

#if CONFIG_ENABLE_CHIP_SHELL
  esp_matter::console::diagnostics_register_commands();
  esp_matter::console::wifi_register_commands();
#if CONFIG_OPENTHREAD_CLI
  esp_matter::console::otcli_register_commands();
#endif
  esp_matter::console::init();
#endif
}

void loop() {
  static uint32_t button_time_stamp = 0;
  static bool button_state = false;
  static bool started = false;

  if (!isAccessoryCommissioned()) {
    log_w("Accessory not commissioned yet. Waiting for commissioning.");
#ifdef RGB_BUILTIN
    rgbLedWrite(RGB_BUILTIN, 48, 0, 20);  // Purple indicates accessory not commissioned
#endif
    delay(5000);
    return;
  }

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  if (!isWifiConnected()) {
    log_w("Wi-Fi not connected yet. Waiting for connection.");
#ifdef RGB_BUILTIN
    rgbLedWrite(RGB_BUILTIN, 48, 20, 0);  // Orange indicates accessory not connected to Wi-Fi
#endif
    delay(5000);
    return;
  }
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  if (!isThreadConnected()) {
    log_w("Thread not connected yet. Waiting for connection.");
#ifdef RGB_BUILTIN
    rgbLedWrite(RGB_BUILTIN, 0, 20, 48);  // Blue indicates accessory not connected to Trhead
#endif
    delay(5000);
    return;
  }
#endif

  // Once all network connections are established, the accessory is ready for use
  // Run it only once
  if (!started) {
    log_i("Accessory is commissioned and connected to Wi-Fi. Ready for use.");
    started = true;
    // Starting driver with default values
    matter_light_set_defaults(light_endpoint_id);
  }

  // Check if the button is pressed and toggle the light right away
  if (digitalRead(button_gpio) == LOW && !button_state) {
    // deals with button debounce
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.

    // Toggle button is pressed - toggle the light
    log_i("Toggle button pressed");

    endpoint_t *endpoint = endpoint::get(node::get(), light_endpoint_id);
    cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
    attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
  }

  // Check if the button is released and handle the factory reset
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > 100 && digitalRead(button_gpio) == HIGH) {
    button_state = false;  // released. It can be pressed again after 100ms debounce.

    // Factory reset is triggered if the button is pressed for more than 10 seconds
    if (time_diff > 10000) {
      log_i("Factory reset triggered. Light will restored to factory settings.");
      esp_matter::factory_reset();
    }
  }

  delay(50);  // WDT is happier with a delay
}
