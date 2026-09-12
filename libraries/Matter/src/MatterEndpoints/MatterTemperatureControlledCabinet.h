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

class ArduinoCabinetTemperatureLevelsDelegate;

class MatterTemperatureControlledCabinet : public MatterEndPoint {
  friend class ArduinoCabinetTemperatureLevelsDelegate;
public:
  MatterTemperatureControlledCabinet();
  ~MatterTemperatureControlledCabinet();

  // begin with temperature_number feature (mutually exclusive with temperature_level)
  // This enables temperature setpoint control with min/max limits and optional step
  bool begin(double tempSetpoint = 0.00, double minTemperature = -10.0, double maxTemperature = 32.0, double step = 0.50);

  // CHIP SupportedTemperatureLevels encode buffer (temperature-control-server.cpp).
  static constexpr uint8_t MAX_TEMPERATURE_LEVEL_LABEL_LENGTH = 32;

  // begin with temperature_level feature (mutually exclusive with temperature_number)
  // This enables temperature level control with an array of supported levels.
  // Matter SupportedTemperatureLevels is a list of strings; each uint8 is advertised as a
  // decimal label unless an optional labels array is passed (see the overload below).
  bool begin(uint8_t *supportedLevels, uint16_t levelCount, uint8_t selectedLevel = 0);

  // Same as begin(supportedLevels, levelCount, selectedLevel), with hub-visible names.
  // labels[i] is not copied; each non-empty pointer must remain valid while this endpoint
  // is running (string literals are fine). nullptr or "" falls back to the decimal of
  // supportedLevels[i]. A label longer than MAX_TEMPERATURE_LEVEL_LABEL_LENGTH is rejected.
  bool begin(uint8_t *supportedLevels, const char *const *labels, uint16_t levelCount, uint8_t selectedLevel = 0);

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
  // The uint8 array is copied. Hub labels become the decimal of each value.
  bool setSupportedTemperatureLevels(uint8_t *levels, uint16_t count);
  // Same, with hub-visible names. Label pointers are not copied (string literals are fine).
  // Passing labels == nullptr clears any previously stored names (decimal fallback).
  bool setSupportedTemperatureLevels(uint8_t *levels, const char *const *labels, uint16_t count);
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
  // Not copied. nullptr means advertise the uint8 as a decimal string.
  const char *supportedLevelLabels[16] = {};
  uint16_t supportedLevelsCount = 0;
  bool temperatureLevelsDelegateHeld = false;

  // internal functions to set the raw temperature values (Matter Cluster)
  bool setRawTemperatureSetpoint(int16_t _rawTemperature);
  bool setRawMinTemperature(int16_t _rawTemperature);
  bool setRawMaxTemperature(int16_t _rawTemperature);
  bool setRawStep(int16_t _rawStep);
  bool begin(int16_t _rawTempSetpoint, int16_t _rawMinTemperature, int16_t _rawMaxTemperature, int16_t _rawStep);
  bool beginInternal(uint8_t *supportedLevels, const char *const *labels, uint16_t levelCount, uint8_t selectedLevel);
  bool labelsFitChipBuffer(const char *const *labels, uint16_t count) const;
  void assignSupportedLevels(uint8_t *levels, const char *const *labels, uint16_t count);
  void reportSupportedTemperatureLevels();
  bool indexOfSupportedLevel(uint8_t level, uint8_t *index) const;
  bool writeSelectedTemperatureLevelIndex(uint8_t index);
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
