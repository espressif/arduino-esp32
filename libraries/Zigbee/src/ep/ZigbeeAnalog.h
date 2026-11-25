// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

/* Class of Zigbee Analog sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

/**
 * @brief Bit flags for analog clusters added to a ZigbeeAnalog endpoint.
 */
enum zigbee_analog_clusters {
  ANALOG_INPUT = 1, /**< Analog Input cluster. */
  ANALOG_OUTPUT = 2 /**< Analog Output cluster. */
};

/**
 * @brief Analog input/output endpoint (Zigbee HA analog signal processing).
 *
 * Analog Input (AI) reports sensor values to the coordinator. Analog Output (AO)
 * is controlled by the coordinator for actuators such as dimmers or valves.
 */
class ZigbeeAnalog : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee analog endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeAnalog(uint8_t endpoint);
  ~ZigbeeAnalog() {}

  /**
   * @brief Add the analog input cluster to the endpoint.
   * @return true if successful.
   */
  bool addAnalogInput();
  /**
   * @brief Add the analog output cluster to the endpoint.
   * @return true if successful.
   */
  bool addAnalogOutput();

  /**
   * @brief Set the application type for the analog input.
   * @param application_type Application type constant (see esp_zigbee_zcl_analog_input.h).
   * @return true if successful.
   */
  bool setAnalogInputApplication(uint32_t application_type);
  /**
   * @brief Set a custom description for the analog input.
   * @param description Description string.
   * @return true if successful.
   */
  bool setAnalogInputDescription(const char *description);
  /**
   * @brief Set the resolution for the analog input.
   * @param resolution Resolution value.
   * @return true if successful.
   */
  bool setAnalogInputResolution(float resolution);

  /**
   * @brief Set the application type for the analog output.
   * @param application_type Application type constant (see esp_zigbee_zcl_analog_output.h).
   * @return true if successful.
   */
  bool setAnalogOutputApplication(uint32_t application_type);
  /**
   * @brief Set a custom description for the analog output.
   * @param description Description string.
   * @return true if successful.
   */
  bool setAnalogOutputDescription(const char *description);
  /**
   * @brief Set the resolution for the analog output.
   * @param resolution Resolution value.
   * @return true if successful.
   */
  bool setAnalogOutputResolution(float resolution);

  /**
   * @brief Set the minimum and maximum values for the analog output.
   * @param min Minimum value.
   * @param max Maximum value.
   * @return true if successful.
   */
  bool setAnalogOutputMinMax(float min, float max);
  /**
   * @brief Set the minimum and maximum values for the analog input.
   * @param min Minimum value.
   * @param max Maximum value.
   * @return true if successful.
   */
  bool setAnalogInputMinMax(float min, float max);

  /**
   * @brief Set a callback invoked when the analog output value changes.
   * @param callback Function called with the new analog output value.
   */
  void onAnalogOutputChange(void (*callback)(float analog)) {
    _on_analog_output_change = callback;
  }

  /**
   * @brief Set the analog input value.
   * @param analog Analog input value.
   * @return true if successful.
   */
  bool setAnalogInput(float analog);
  /**
   * @brief Set the analog output value.
   * @param analog Analog output value.
   * @return true if successful.
   */
  bool setAnalogOutput(float analog);

  /**
   * @brief Get the analog input value.
   * @param analog Output parameter receiving the current analog input value.
   * @return true if successful.
   */
  bool getAnalogInput(float &analog);
  /**
   * @brief Get the current analog output value.
   * @return Current analog output value.
   */
  float getAnalogOutput() {
    return _output_state;
  }

  /**
   * @brief Manually report the current analog input value.
   * @return true if successful.
   */
  bool reportAnalogInput();
  /**
   * @brief Manually report the current analog output value.
   * @return true if successful.
   */
  bool reportAnalogOutput();

  /**
   * @brief Set the reporting configuration for analog input.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in value to trigger a report.
   * @return true if successful.
   */
  bool setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta);

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;

  void (*_on_analog_output_change)(float);
  void analogOutputChanged();

  uint8_t _analog_clusters;
  float _output_state;

  uint32_t _ai_application_type;
  uint32_t _ao_application_type;
  char _ai_description[ZB_MAX_NAME_LENGTH + 2];
  char _ao_description[ZB_MAX_NAME_LENGTH + 2];
  float _ai_resolution;
  float _ao_resolution;
  float _ai_min;
  float _ai_max;
  float _ao_min;
  float _ao_max;
};

#endif  // CONFIG_ZB_ENABLED
