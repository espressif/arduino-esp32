/* Class of Zigbee Temperature sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#if SOC_IEEE802154_SUPPORTED

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
  ~ZigbeeThermostat();

  void onTempRecieve(void (*callback)(float)) {
    _on_temp_recieve = callback;
  }
  void onConfigRecieve(void (*callback)(float, float, float)) {
    _on_config_recieve = callback;
  }

  void getTemperature();
  void getSensorSettings();
  void setTemperatureReporting(uint16_t min_interval, uint16_t max_interval, float delta);

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeThermostat *_instance;

  void (*_on_temp_recieve)(float);
  void (*_on_config_recieve)(float, float, float);
  float _min_temp;
  float _max_temp;
  float _tolerance;

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
  static void bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  static void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

  void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute) override;
};

#endif  //SOC_IEEE802154_SUPPORTED
