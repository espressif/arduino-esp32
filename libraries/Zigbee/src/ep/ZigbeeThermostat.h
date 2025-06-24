/* Class of Zigbee Temperature sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

//define the thermostat configuration to avoid narrowing conversion issue in zigbee-sdk
#define ZB_DEFAULT_THERMOSTAT_CONFIG()                                                               \
  {                                                                                                  \
    .basic_cfg =                                                                                     \
      {                                                                                              \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                                   \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                                 \
      },                                                                                             \
    .identify_cfg =                                                                                  \
      {                                                                                              \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                            \
      },                                                                                             \
    .thermostat_cfg = {                                                                              \
      .local_temperature = (int16_t)ESP_ZB_ZCL_THERMOSTAT_LOCAL_TEMPERATURE_DEFAULT_VALUE,           \
      .occupied_cooling_setpoint = ESP_ZB_ZCL_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_DEFAULT_VALUE,    \
      .occupied_heating_setpoint = ESP_ZB_ZCL_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_DEFAULT_VALUE,    \
      .control_sequence_of_operation = ESP_ZB_ZCL_THERMOSTAT_CONTROL_SEQ_OF_OPERATION_DEFAULT_VALUE, \
      .system_mode = ESP_ZB_ZCL_THERMOSTAT_CONTROL_SYSTEM_MODE_DEFAULT_VALUE,                        \
    },                                                                                               \
  }
class ZigbeeThermostat : public ZigbeeEP {
public:
  ZigbeeThermostat(uint8_t endpoint);
  ~ZigbeeThermostat() {}

  void onTempReceive(void (*callback)(float)) {
    _on_temp_receive = callback;
  }
  void onTempReceiveWithSource(void (*callback)(float, uint8_t, esp_zb_zcl_addr_t)) {
    _on_temp_receive_with_source = callback;
  }
  void onConfigReceive(void (*callback)(float, float, float)) {
    _on_config_receive = callback;
  }

  void getTemperature();
  void getTemperature(uint16_t group_addr);
  void getTemperature(uint8_t endpoint, uint16_t short_addr);
  void getTemperature(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

  void getSensorSettings();
  void getSensorSettings(uint16_t group_addr);
  void getSensorSettings(uint8_t endpoint, uint16_t short_addr);
  void getSensorSettings(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

  void setTemperatureReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  void setTemperatureReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  void setTemperatureReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  void setTemperatureReporting(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta);

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeThermostat *_instance;
  zb_device_params_t *_device;

  void (*_on_temp_receive)(float);
  void (*_on_temp_receive_with_source)(float, uint8_t, esp_zb_zcl_addr_t);
  void (*_on_config_receive)(float, float, float);
  float _min_temp;
  float _max_temp;
  float _tolerance;

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
  void bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);
  static void bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  static void findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

  void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address) override;
};

#endif  // CONFIG_ZB_ENABLED
