// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>

class MatterTemperatureControlledCabinet : public MatterEndPoint {
public:
  MatterTemperatureControlledCabinet();
  ~MatterTemperatureControlledCabinet();
  
  // begin with temperature_number feature (mutually exclusive with temperature_level)
  // This enables temperature setpoint control with min/max limits and optional step
  bool begin(double tempSetpoint = 0.00, double minTemperature = -10.0, double maxTemperature = 32.0, double step = 0.50);
  
  // begin with temperature_level feature (mutually exclusive with temperature_number)
  // This enables temperature level control with an array of supported levels
  bool begin(uint8_t *supportedLevels, uint16_t levelCount, uint8_t selectedLevel = 0);
  
  // this will stop processing Temperature Controlled Cabinet Matter events
  void end();

  // set the temperature setpoint
  bool setTemperatureSetpoint(double temperature);
  // returns the temperature setpoint in Celsius
  double getTemperatureSetpoint();

  // set the minimum temperature
  bool setMinTemperature(double temperature);
  // returns the minimum temperature in Celsius
  double getMinTemperature();

  // set the maximum temperature
  bool setMaxTemperature(double temperature);
  // returns the maximum temperature in Celsius
  double getMaxTemperature();

  // set the temperature step (optional, requires temperature_step feature)
  bool setStep(double step);
  // returns the temperature step in Celsius
  double getStep();

  // set the selected temperature level (optional, requires temperature_level feature)
  bool setSelectedTemperatureLevel(uint8_t level);
  // returns the selected temperature level
  uint8_t getSelectedTemperatureLevel();

  // set supported temperature levels (optional, requires temperature_level feature)
  bool setSupportedTemperatureLevels(uint8_t *levels, uint16_t count);
  // get supported temperature levels count
  uint16_t getSupportedTemperatureLevelsCount();

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  // Feature mode: true = temperature_number, false = temperature_level
  // Note: temperature_number and temperature_level are mutually exclusive
  bool useTemperatureNumber = true;
  
  // temperature in 1/100th Celsius (stored as int16_t by multiplying by 100)
  int16_t rawTempSetpoint = 0;
  int16_t rawMinTemperature = 0;
  int16_t rawMaxTemperature = 0;
  int16_t rawStep = 0;
  uint8_t selectedTempLevel = 0;
  // Fixed-size buffer for supported temperature levels (max 16 as per Matter spec: temperature_control::k_max_temp_level_count)
  uint8_t supportedLevelsArray[16];  // Size matches esp_matter::cluster::temperature_control::k_max_temp_level_count
  uint16_t supportedLevelsCount = 0;
  
  // internal functions to set the raw temperature values (Matter Cluster)
  bool setRawTemperatureSetpoint(int16_t _rawTemperature);
  bool setRawMinTemperature(int16_t _rawTemperature);
  bool setRawMaxTemperature(int16_t _rawTemperature);
  bool setRawStep(int16_t _rawStep);
  bool begin(int16_t _rawTempSetpoint, int16_t _rawMinTemperature, int16_t _rawMaxTemperature, int16_t _rawStep);
  bool beginInternal(uint8_t *supportedLevels, uint16_t levelCount, uint8_t selectedLevel);
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */

