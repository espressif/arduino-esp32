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

/**
 * @brief Matter temperature controlled cabinet endpoint with setpoint or named-level control.
 *
 * Two mutually exclusive initialization modes:
 * - Temperature number: begin(tempSetpoint, minTemp, maxTemp, step) with 1/100th °C precision.
 * - Temperature level: begin(supportedLevels, levelCount, selectedLevel) (optional labels).
 *
 * Methods return false if called in the wrong feature mode. Compatible with
 * Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterTemperatureControlledCabinet : public MatterEndPoint {
  friend class ArduinoCabinetTemperatureLevelsDelegate;

public:
  /**
   * @brief Create a new Matter temperature controlled cabinet endpoint.
   */
  MatterTemperatureControlledCabinet();
  /**
   * @brief Destroy the temperature controlled cabinet endpoint object.
   */
  ~MatterTemperatureControlledCabinet();

  /**
   * @brief Initialize with temperature_number mode (setpoint, min/max, step). Mutually exclusive with temperature_level.
   * @param tempSetpoint Initial temperature setpoint in Celsius (default 0.00).
   * @param minTemperature Minimum allowed temperature in Celsius (default -10.0).
   * @param maxTemperature Maximum allowed temperature in Celsius (default 32.0).
   * @param step Initial temperature step in Celsius (default 0.50). Always enabled in this mode.
   * @return true if successful, false if min >= max, the setpoint is outside [min, max], or creation fails.
   * @note CHIP keeps min, max, and step as construction-time constants, so pass the final values here.
   */
  bool begin(double tempSetpoint = 0.00, double minTemperature = -10.0, double maxTemperature = 32.0, double step = 0.50);

  /**
   * @brief Maximum length of a temperature-level label string (CHIP encode buffer).
   */
  static constexpr uint8_t MAX_TEMPERATURE_LEVEL_LABEL_LENGTH = 32;

  /**
   * @brief Initialize with temperature_level mode. Mutually exclusive with temperature_number.
   *
   * Without a labels array, each uint8 is advertised as a decimal string (30 → "30").
   *
   * @param supportedLevels Pointer to array of temperature level values (uint8_t, 0–255). Copied internally.
   * @param levelCount Number of levels in the array (maximum 16).
   * @param selectedLevel Initial selected level; must be one of the values in supportedLevels (default 0).
   * @return true if successful, false otherwise.
   */
  bool begin(uint8_t *supportedLevels, uint16_t levelCount, uint8_t selectedLevel = 0);

  /**
   * @brief Initialize with temperature_level mode and hub-visible names.
   *
   * labels[i] is not copied; each non-empty pointer must remain valid while this
   * endpoint is running (string literals are fine). nullptr or "" falls back to
   * the decimal of supportedLevels[i].
   *
   * @param supportedLevels Pointer to array of temperature level values (copied internally).
   * @param labels Parallel array of C strings, same length as supportedLevels.
   * @param levelCount Number of levels (maximum 16).
   * @param selectedLevel Initial selected level; must be one of the values in supportedLevels (default 0).
   * @return true if successful, false if a label is longer than MAX_TEMPERATURE_LEVEL_LABEL_LENGTH or validation fails.
   */
  bool begin(uint8_t *supportedLevels, const char *const *labels, uint16_t levelCount, uint8_t selectedLevel = 0);

  /**
   * @brief Stop processing Matter temperature controlled cabinet events.
   */
  void end();

  /**
   * @brief Set the temperature setpoint. Requires temperature_number mode.
   * @param temperature Temperature setpoint in Celsius. Must be within min/max.
   * @return true if successful, false if out of range or wrong feature mode.
   */
  bool setTemperatureSetpoint(double temperature);
  /**
   * @brief Get the current temperature setpoint in Celsius.
   * @return Setpoint with 1/100th degree precision.
   */
  double getTemperatureSetpoint();

  /**
   * @brief Set the minimum allowed temperature. Requires temperature_number mode. Call before Matter.begin().
   * @param temperature Minimum temperature in Celsius. Must stay below max and must not move past the current setpoint.
   * @return true if successful, false after Matter.begin() or in temperature_level mode.
   */
  bool setMinTemperature(double temperature);
  /**
   * @brief Get the minimum allowed temperature in Celsius.
   * @return Minimum temperature with 1/100th degree precision.
   */
  double getMinTemperature();

  /**
   * @brief Set the maximum allowed temperature. Requires temperature_number mode. Call before Matter.begin().
   * @param temperature Maximum temperature in Celsius. Must stay above min and must not move past the current setpoint.
   * @return true if successful, false after Matter.begin() or in temperature_level mode.
   */
  bool setMaxTemperature(double temperature);
  /**
   * @brief Get the maximum allowed temperature in Celsius.
   * @return Maximum temperature with 1/100th degree precision.
   */
  double getMaxTemperature();

  /**
   * @brief Set the temperature step. Requires temperature_number mode. Call before Matter.begin().
   * @param step Temperature step in Celsius.
   * @return true if successful, false after Matter.begin() or in temperature_level mode.
   */
  bool setStep(double step);
  /**
   * @brief Get the current temperature step in Celsius.
   * @return Step with 1/100th degree precision.
   */
  double getStep();

  /**
   * @brief Set the selected temperature level. Requires temperature_level mode.
   * @param level Temperature level value from the supported-levels array (0–255).
   * @return true if successful, false otherwise (including temperature_number mode).
   * @note Stored in Matter as the index of that entry in SupportedTemperatureLevels.
   */
  bool setSelectedTemperatureLevel(uint8_t level);
  /**
   * @brief Get the current selected temperature level.
   * @return Selected temperature level (0–255).
   */
  uint8_t getSelectedTemperatureLevel();

  /**
   * @brief Set supported temperature levels (decimal labels). Requires temperature_level mode.
   *
   * The uint8 array is copied. Hub labels become the decimal of each value.
   *
   * @param levels Pointer to array of temperature level values.
   * @param count Number of levels (maximum 16).
   * @return true if successful, false otherwise.
   */
  bool setSupportedTemperatureLevels(uint8_t *levels, uint16_t count);
  /**
   * @brief Set supported temperature levels with hub-visible names. Requires temperature_level mode.
   *
   * Label pointers are not copied (string literals are fine). Passing labels == nullptr
   * clears any previously stored names (decimal fallback). After Matter.begin(), a
   * successful call marks SupportedTemperatureLevels dirty so a subscribed hub rereads the list.
   *
   * @param levels Pointer to array of temperature level values (copied internally).
   * @param labels Optional parallel name array. Same pointer lifetime as begin(..., labels, ...).
   * @param count Number of levels (maximum 16).
   * @return true if successful, false if count exceeds 16, a label is longer than 32 characters,
   *         the current selection is not in the new array, or wrong feature mode.
   */
  bool setSupportedTemperatureLevels(uint8_t *levels, const char *const *labels, uint16_t count);
  /**
   * @brief Get the count of supported temperature levels.
   * @return Number of supported temperature levels.
   */
  uint16_t getSupportedTemperatureLevelsCount();

  /**
   * @brief Called by the Matter internal event processor when an attribute changes.
   *
   * Can be overridden by the application if necessary.
   *
   * @param endpoint_id Endpoint ID where the attribute changed.
   * @param cluster_id Cluster ID of the changed attribute.
   * @param attribute_id Attribute ID that changed.
   * @param val Pointer to the new attribute value.
   * @return true if the change was handled successfully.
   */
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
