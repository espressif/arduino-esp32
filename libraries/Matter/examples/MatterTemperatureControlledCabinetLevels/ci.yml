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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <string.h>
#include <Matter.h>
#include <app/server/Server.h>
#include <MatterEndpoints/MatterTemperatureControlledCabinet.h>
#include <esp_matter_attribute.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

// Custom endpoint for temperature_level_controlled_cabinet device
// This endpoint uses temperature_level feature instead of temperature_number
namespace esp_matter {
using namespace cluster;
namespace endpoint {
namespace temperature_level_controlled_cabinet {
typedef struct config {
  cluster::descriptor::config_t descriptor;
  cluster::temperature_control::config_t temperature_control;
} config_t;

uint32_t get_device_type_id() {
  return ESP_MATTER_TEMPERATURE_CONTROLLED_CABINET_DEVICE_TYPE_ID;
}

uint8_t get_device_type_version() {
  return ESP_MATTER_TEMPERATURE_CONTROLLED_CABINET_DEVICE_TYPE_VERSION;
}

esp_err_t add(endpoint_t *endpoint, config_t *config) {
  if (!endpoint) {
    log_e("Endpoint cannot be NULL");
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = add_device_type(endpoint, get_device_type_id(), get_device_type_version());
  if (err != ESP_OK) {
    log_e("Failed to add device type id:%" PRIu32 ",err: %d", get_device_type_id(), err);
    return err;
  }

  // Create temperature_control cluster with temperature_level feature
  // Note: temperature_number and temperature_level are mutually exclusive
  temperature_control::create(endpoint, &(config->temperature_control), CLUSTER_FLAG_SERVER, temperature_control::feature::temperature_level::get_id());

  return ESP_OK;
}

endpoint_t *create(node_t *node, config_t *config, uint8_t flags, void *priv_data) {
  endpoint_t *endpoint = endpoint::create(node, flags, priv_data);
  add(endpoint, config);
  return endpoint;
}
}  // namespace temperature_level_controlled_cabinet
}  // namespace endpoint
}  // namespace esp_matter

bool MatterTemperatureControlledCabinet::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Temperature Controlled Cabinet device has not begun.");
    return false;
  }

  log_d("Temperature Controlled Cabinet Attr update callback: endpoint: %u, cluster: %u, attribute: %u", endpoint_id, cluster_id, attribute_id);

  // Handle TemperatureControl cluster attribute changes from Matter controller
  if (cluster_id == TemperatureControl::Id) {
    switch (attribute_id) {
      case TemperatureControl::Attributes::TemperatureSetpoint::Id:
        if (useTemperatureNumber) {
          rawTempSetpoint = val->val.i16;
          log_i("Temperature setpoint changed to %.02fC", (float)rawTempSetpoint / 100.0);
        } else {
          log_w("Temperature setpoint change ignored - temperature_level feature is active");
        }
        break;

      case TemperatureControl::Attributes::MinTemperature::Id:
        if (useTemperatureNumber) {
          rawMinTemperature = val->val.i16;
          log_i("Min temperature changed to %.02fC", (float)rawMinTemperature / 100.0);
        } else {
          log_w("Min temperature change ignored - temperature_level feature is active");
        }
        break;

      case TemperatureControl::Attributes::MaxTemperature::Id:
        if (useTemperatureNumber) {
          rawMaxTemperature = val->val.i16;
          log_i("Max temperature changed to %.02fC", (float)rawMaxTemperature / 100.0);
        } else {
          log_w("Max temperature change ignored - temperature_level feature is active");
        }
        break;

      case TemperatureControl::Attributes::Step::Id:
        if (useTemperatureNumber) {
          rawStep = val->val.i16;
          log_i("Temperature step changed to %.02fC", (float)rawStep / 100.0);
        } else {
          log_w("Temperature step change ignored - temperature_level feature is active");
        }
        break;

      case TemperatureControl::Attributes::SelectedTemperatureLevel::Id:
        if (!useTemperatureNumber) {
          selectedTempLevel = val->val.u8;
          log_i("Selected temperature level changed to %u", selectedTempLevel);
        } else {
          log_w("Selected temperature level change ignored - temperature_number feature is active");
        }
        break;

      case TemperatureControl::Attributes::SupportedTemperatureLevels::Id:
        // SupportedTemperatureLevels is read-only (managed via iterator delegate)
        // But if a controller tries to write it, we should log it
        log_w("SupportedTemperatureLevels change attempted - this attribute is read-only");
        break;

      default: log_d("Unhandled TemperatureControl Attribute ID: %u", attribute_id); break;
    }
  }

  return ret;
}

MatterTemperatureControlledCabinet::MatterTemperatureControlledCabinet() {}

MatterTemperatureControlledCabinet::~MatterTemperatureControlledCabinet() {
  end();
}

bool MatterTemperatureControlledCabinet::begin(double tempSetpoint, double minTemperature, double maxTemperature, double step) {
  int16_t rawSetpoint = static_cast<int16_t>(tempSetpoint * 100.0);
  int16_t rawMin = static_cast<int16_t>(minTemperature * 100.0);
  int16_t rawMax = static_cast<int16_t>(maxTemperature * 100.0);
  int16_t rawStepValue = static_cast<int16_t>(step * 100.0);
  return begin(rawSetpoint, rawMin, rawMax, rawStepValue);
}

bool MatterTemperatureControlledCabinet::begin(int16_t _rawTempSetpoint, int16_t _rawMinTemperature, int16_t _rawMaxTemperature, int16_t _rawStep) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Temperature Controlled Cabinet with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  // Note: esp-matter automatically creates all attributes from the config struct when features are enabled
  // - temperature_number feature creates: TemperatureSetpoint, MinTemperature, MaxTemperature
  // - temperature_step feature creates: Step (always enabled for temperature_number mode to allow setStep() later)
  // No need to manually set attributes here as they are already created with the config values

  temperature_controlled_cabinet::config_t cabinet_config;
  cabinet_config.temperature_control.temperature_number.temp_setpoint = _rawTempSetpoint;
  cabinet_config.temperature_control.temperature_number.min_temperature = _rawMinTemperature;
  cabinet_config.temperature_control.temperature_number.max_temperature = _rawMaxTemperature;
  cabinet_config.temperature_control.temperature_step.step = _rawStep;
  cabinet_config.temperature_control.temperature_level.selected_temp_level = 0;

  // Enable temperature_number feature (required)
  // Note: temperature_number and temperature_level are mutually exclusive.
  // Only one of them can be enabled at a time.
  cabinet_config.temperature_control.features = temperature_control::feature::temperature_number::get_id();

  // Always enable temperature_step feature to allow setStep() to be called later
  // Note: temperature_step requires temperature_number feature (which is always enabled for this mode)
  // The step value can be set initially via begin() or later via setStep()
  cabinet_config.temperature_control.features |= temperature_control::feature::temperature_step::get_id();

  // endpoint handles can be used to add/modify clusters
  endpoint_t *endpoint = temperature_controlled_cabinet::create(node::get(), &cabinet_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Temperature Controlled Cabinet endpoint");
    return false;
  }

  rawTempSetpoint = _rawTempSetpoint;
  rawMinTemperature = _rawMinTemperature;
  rawMaxTemperature = _rawMaxTemperature;
  rawStep = _rawStep;
  selectedTempLevel = 0;
  useTemperatureNumber = true;  // Set feature mode to temperature_number

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Temperature Controlled Cabinet created with temperature_number feature, endpoint_id %d", getEndPointId());

  // Workaround: Manually create Step attribute if it wasn't created automatically
  // This handles the case where temperature_step::add() fails due to feature map timing issue
  // The feature map check in temperature_step::add() may not see the temperature_number feature
  // immediately after it's added, causing the Step attribute to not be created
  cluster_t *cluster = cluster::get(endpoint, TemperatureControl::Id);
  if (cluster != nullptr) {
    attribute_t *step_attr = attribute::get(cluster, TemperatureControl::Attributes::Step::Id);
    if (step_attr == nullptr) {
      // Step attribute wasn't created, manually create it
      log_w("Step attribute not found after endpoint creation, manually creating it");
      step_attr = temperature_control::attribute::create_step(cluster, _rawStep);
      if (step_attr != nullptr) {
        // Update the feature map to include temperature_step feature
        // This ensures the feature is properly registered even though the attribute was created manually
        esp_matter_attr_val_t feature_map_val = esp_matter_invalid(NULL);
        attribute_t *feature_map_attr = attribute::get(cluster, Globals::Attributes::FeatureMap::Id);
        if (feature_map_attr != nullptr && attribute::get_val(feature_map_attr, &feature_map_val) == ESP_OK) {
          feature_map_val.val.u32 |= temperature_control::feature::temperature_step::get_id();
          attribute::set_val(feature_map_attr, &feature_map_val);
        }
        log_i("Step attribute manually created with value %.02fC", (float)_rawStep / 100.0);
      } else {
        log_e("Failed to manually create Step attribute");
      }
    }
  }

  started = true;
  return true;
}

bool MatterTemperatureControlledCabinet::begin(uint8_t *supportedLevels, uint16_t levelCount, uint8_t selectedLevel) {
  if (supportedLevels == nullptr || levelCount == 0) {
    log_e("Invalid supportedLevels array or levelCount. Must provide at least one level.");
    return false;
  }

  // Validate against maximum from ESP-Matter
  if (levelCount > temperature_control::k_max_temp_level_count) {
    log_e("Level count %u exceeds maximum %u", levelCount, temperature_control::k_max_temp_level_count);
    return false;
  }

  // Validate that selectedLevel exists in supportedLevels array
  bool levelFound = false;
  for (uint16_t i = 0; i < levelCount; i++) {
    if (supportedLevels[i] == selectedLevel) {
      levelFound = true;
      break;
    }
  }
  if (!levelFound) {
    log_e("Selected level %u is not in the supported levels array", selectedLevel);
    return false;
  }
  return beginInternal(supportedLevels, levelCount, selectedLevel);
}

bool MatterTemperatureControlledCabinet::beginInternal(uint8_t *supportedLevels, uint16_t levelCount, uint8_t selectedLevel) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Temperature Controlled Cabinet with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  // Use custom temperature_level_controlled_cabinet endpoint that supports temperature_level feature
  temperature_level_controlled_cabinet::config_t cabinet_config;
  // Initialize temperature_number config (not used but required for struct)
  cabinet_config.temperature_control.temperature_number.temp_setpoint = 0;
  cabinet_config.temperature_control.temperature_number.min_temperature = 0;
  cabinet_config.temperature_control.temperature_number.max_temperature = 0;
  cabinet_config.temperature_control.temperature_step.step = 0;
  cabinet_config.temperature_control.temperature_level.selected_temp_level = selectedLevel;

  // Enable temperature_level feature
  // Note: temperature_number and temperature_level are mutually exclusive.
  // Only one of them can be enabled at a time.
  cabinet_config.temperature_control.features = temperature_control::feature::temperature_level::get_id();

  // endpoint handles can be used to add/modify clusters
  endpoint_t *endpoint = temperature_level_controlled_cabinet::create(node::get(), &cabinet_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Temperature Level Controlled Cabinet endpoint");
    return false;
  }

  // Copy supported levels array into internal buffer
  memcpy(supportedLevelsArray, supportedLevels, levelCount * sizeof(uint8_t));
  supportedLevelsCount = levelCount;
  selectedTempLevel = selectedLevel;
  useTemperatureNumber = false;  // Set feature mode to temperature_level

  // Initialize temperature_number values to 0 (not used in this mode)
  rawTempSetpoint = 0;
  rawMinTemperature = 0;
  rawMaxTemperature = 0;
  rawStep = 0;

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Temperature Level Controlled Cabinet created with temperature_level feature, endpoint_id %d", getEndPointId());

  // Set started flag before calling setter methods (they check for started)
  started = true;

  // Set supported temperature levels using internal copy
  if (!setSupportedTemperatureLevels(supportedLevelsArray, levelCount)) {
    log_e("Failed to set supported temperature levels");
    started = false;  // Reset on failure
    return false;
  }

  // Set selected temperature level
  if (!setSelectedTemperatureLevel(selectedLevel)) {
    log_e("Failed to set selected temperature level");
    started = false;  // Reset on failure
    return false;
  }

  return true;
}

void MatterTemperatureControlledCabinet::end() {
  started = false;
  useTemperatureNumber = true;  // Reset to default
  supportedLevelsCount = 0;
  // No need to clear array - it's a fixed-size buffer
}

bool MatterTemperatureControlledCabinet::setRawTemperatureSetpoint(int16_t _rawTemperature) {
  if (!started) {
    log_e("Matter Temperature Controlled Cabinet device has not begun.");
    return false;
  }

  if (!useTemperatureNumber) {
    log_e("Temperature setpoint methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return false;
  }

  // Validate against min/max
  if (_rawTemperature < rawMinTemperature || _rawTemperature > rawMaxTemperature) {
    log_e(
      "Temperature setpoint %.02fC is out of range [%.02fC, %.02fC]", (float)_rawTemperature / 100.0, (float)rawMinTemperature / 100.0,
      (float)rawMaxTemperature / 100.0
    );
    return false;
  }

  // avoid processing if there was no change
  if (rawTempSetpoint == _rawTemperature) {
    return true;
  }

  esp_matter_attr_val_t tempVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::TemperatureSetpoint::Id, &tempVal)) {
    log_e("Failed to get Temperature Controlled Cabinet Temperature Setpoint Attribute.");
    return false;
  }
  if (tempVal.val.i16 != _rawTemperature) {
    tempVal.val.i16 = _rawTemperature;
    bool ret;
    ret = updateAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::TemperatureSetpoint::Id, &tempVal);
    if (!ret) {
      log_e("Failed to update Temperature Controlled Cabinet Temperature Setpoint Attribute.");
      return false;
    }
    rawTempSetpoint = _rawTemperature;
  }
  log_v("Temperature Controlled Cabinet setpoint set to %.02fC", (float)_rawTemperature / 100.00);

  return true;
}

bool MatterTemperatureControlledCabinet::setTemperatureSetpoint(double temperature) {
  int16_t rawValue = static_cast<int16_t>(temperature * 100.0);
  return setRawTemperatureSetpoint(rawValue);
}

double MatterTemperatureControlledCabinet::getTemperatureSetpoint() {
  if (!useTemperatureNumber) {
    log_e("Temperature setpoint methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return 0.0;
  }

  esp_matter_attr_val_t tempVal = esp_matter_invalid(NULL);
  if (getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::TemperatureSetpoint::Id, &tempVal)) {
    rawTempSetpoint = tempVal.val.i16;
  }
  return (double)rawTempSetpoint / 100.0;
}

bool MatterTemperatureControlledCabinet::setRawMinTemperature(int16_t _rawTemperature) {
  if (!started) {
    log_e("Matter Temperature Controlled Cabinet device has not begun.");
    return false;
  }

  if (!useTemperatureNumber) {
    log_e("Min temperature methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return false;
  }

  if (rawMinTemperature == _rawTemperature) {
    return true;
  }

  esp_matter_attr_val_t tempVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::MinTemperature::Id, &tempVal)) {
    log_e("Failed to get Temperature Controlled Cabinet Min Temperature Attribute.");
    return false;
  }
  if (tempVal.val.i16 != _rawTemperature) {
    tempVal.val.i16 = _rawTemperature;
    bool ret;
    ret = updateAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::MinTemperature::Id, &tempVal);
    if (!ret) {
      log_e("Failed to update Temperature Controlled Cabinet Min Temperature Attribute.");
      return false;
    }
    rawMinTemperature = _rawTemperature;
  }
  log_v("Temperature Controlled Cabinet min temperature set to %.02fC", (float)_rawTemperature / 100.00);

  return true;
}

bool MatterTemperatureControlledCabinet::setMinTemperature(double temperature) {
  int16_t rawValue = static_cast<int16_t>(temperature * 100.0);
  return setRawMinTemperature(rawValue);
}

double MatterTemperatureControlledCabinet::getMinTemperature() {
  if (!useTemperatureNumber) {
    log_e("Min temperature methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return 0.0;
  }

  esp_matter_attr_val_t tempVal = esp_matter_invalid(NULL);
  if (getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::MinTemperature::Id, &tempVal)) {
    rawMinTemperature = tempVal.val.i16;
  }
  return (double)rawMinTemperature / 100.0;
}

bool MatterTemperatureControlledCabinet::setRawMaxTemperature(int16_t _rawTemperature) {
  if (!started) {
    log_e("Matter Temperature Controlled Cabinet device has not begun.");
    return false;
  }

  if (!useTemperatureNumber) {
    log_e("Max temperature methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return false;
  }

  if (rawMaxTemperature == _rawTemperature) {
    return true;
  }

  esp_matter_attr_val_t tempVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::MaxTemperature::Id, &tempVal)) {
    log_e("Failed to get Temperature Controlled Cabinet Max Temperature Attribute.");
    return false;
  }
  if (tempVal.val.i16 != _rawTemperature) {
    tempVal.val.i16 = _rawTemperature;
    bool ret;
    ret = updateAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::MaxTemperature::Id, &tempVal);
    if (!ret) {
      log_e("Failed to update Temperature Controlled Cabinet Max Temperature Attribute.");
      return false;
    }
    rawMaxTemperature = _rawTemperature;
  }
  log_v("Temperature Controlled Cabinet max temperature set to %.02fC", (float)_rawTemperature / 100.00);

  return true;
}

bool MatterTemperatureControlledCabinet::setMaxTemperature(double temperature) {
  int16_t rawValue = static_cast<int16_t>(temperature * 100.0);
  return setRawMaxTemperature(rawValue);
}

double MatterTemperatureControlledCabinet::getMaxTemperature() {
  if (!useTemperatureNumber) {
    log_e("Max temperature methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return 0.0;
  }

  esp_matter_attr_val_t tempVal = esp_matter_invalid(NULL);
  if (getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::MaxTemperature::Id, &tempVal)) {
    rawMaxTemperature = tempVal.val.i16;
  }
  return (double)rawMaxTemperature / 100.0;
}

bool MatterTemperatureControlledCabinet::setRawStep(int16_t _rawStep) {
  if (!started) {
    log_e("Matter Temperature Controlled Cabinet device has not begun.");
    return false;
  }

  if (!useTemperatureNumber) {
    log_e("Temperature step methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return false;
  }

  if (rawStep == _rawStep) {
    return true;
  }

  esp_matter_attr_val_t stepVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::Step::Id, &stepVal)) {
    log_e("Failed to get Temperature Controlled Cabinet Step Attribute. Temperature_step feature may not be enabled.");
    return false;
  }
  if (stepVal.val.i16 != _rawStep) {
    stepVal.val.i16 = _rawStep;
    bool ret;
    ret = updateAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::Step::Id, &stepVal);
    if (!ret) {
      log_e("Failed to update Temperature Controlled Cabinet Step Attribute.");
      return false;
    }
    rawStep = _rawStep;
  }
  log_v("Temperature Controlled Cabinet step set to %.02fC", (float)_rawStep / 100.00);

  return true;
}

bool MatterTemperatureControlledCabinet::setStep(double step) {
  int16_t rawValue = static_cast<int16_t>(step * 100.0);
  return setRawStep(rawValue);
}

double MatterTemperatureControlledCabinet::getStep() {
  if (!useTemperatureNumber) {
    log_e("Temperature step methods require temperature_number feature. Use begin(tempSetpoint, minTemp, maxTemp, step) instead.");
    return 0.0;
  }

  // Read from attribute (should always exist after begin() due to workaround)
  // If read fails, use stored rawStep value from begin()
  esp_matter_attr_val_t stepVal = esp_matter_invalid(NULL);
  if (getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::Step::Id, &stepVal)) {
    rawStep = stepVal.val.i16;
  }
  return (double)rawStep / 100.0;
}

bool MatterTemperatureControlledCabinet::setSelectedTemperatureLevel(uint8_t level) {
  if (!started) {
    log_e("Matter Temperature Controlled Cabinet device has not begun.");
    return false;
  }

  if (useTemperatureNumber) {
    log_e("Temperature level methods require temperature_level feature. Use begin(supportedLevels, levelCount, selectedLevel) instead.");
    return false;
  }

  // Validate that level is in supported levels array
  bool levelFound = false;
  for (uint16_t i = 0; i < supportedLevelsCount; i++) {
    if (supportedLevelsArray[i] == level) {
      levelFound = true;
      break;
    }
  }
  if (!levelFound) {
    log_e("Temperature level %u is not in the supported levels array", level);
    return false;
  }
  if (selectedTempLevel == level) {
    return true;
  }

  esp_matter_attr_val_t levelVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::SelectedTemperatureLevel::Id, &levelVal)) {
    log_e("Failed to get Temperature Controlled Cabinet Selected Temperature Level Attribute.");
    return false;
  }
  if (levelVal.val.u8 != level) {
    levelVal.val.u8 = level;
    bool ret;
    ret = updateAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::SelectedTemperatureLevel::Id, &levelVal);
    if (!ret) {
      log_e("Failed to update Temperature Controlled Cabinet Selected Temperature Level Attribute.");
      return false;
    }
    selectedTempLevel = level;
  }
  log_v("Temperature Controlled Cabinet selected temperature level set to %u", level);

  return true;
}

uint8_t MatterTemperatureControlledCabinet::getSelectedTemperatureLevel() {
  if (useTemperatureNumber) {
    log_e("Temperature level methods require temperature_level feature. Use begin(supportedLevels, levelCount, selectedLevel) instead.");
    return 0;
  }

  esp_matter_attr_val_t levelVal = esp_matter_invalid(NULL);
  if (getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::SelectedTemperatureLevel::Id, &levelVal)) {
    selectedTempLevel = levelVal.val.u8;
  }
  return selectedTempLevel;
}

bool MatterTemperatureControlledCabinet::setSupportedTemperatureLevels(uint8_t *levels, uint16_t count) {
  if (!started) {
    log_e("Matter Temperature Controlled Cabinet device has not begun.");
    return false;
  }

  if (useTemperatureNumber) {
    log_e("Temperature level methods require temperature_level feature. Use begin(supportedLevels, levelCount, selectedLevel) instead.");
    return false;
  }

  if (levels == nullptr || count == 0) {
    log_e("Invalid levels array or count.");
    return false;
  }

  // Validate against maximum from ESP-Matter
  if (count > temperature_control::k_max_temp_level_count) {
    log_e("Level count %u exceeds maximum %u", count, temperature_control::k_max_temp_level_count);
    return false;
  }

  // Copy the array into internal buffer
  memcpy(supportedLevelsArray, levels, count * sizeof(uint8_t));
  supportedLevelsCount = count;

  // Use internal copy for Matter attribute update
  // Use esp_matter_array helper function which properly initializes the structure
  esp_matter_attr_val_t levelsVal = esp_matter_array(supportedLevelsArray, sizeof(uint8_t), count);

  bool ret = updateAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::SupportedTemperatureLevels::Id, &levelsVal);
  if (!ret) {
    log_e("Failed to update Temperature Controlled Cabinet Supported Temperature Levels Attribute.");
    return false;
  }
  log_v("Temperature Controlled Cabinet supported temperature levels updated, count: %u", count);

  return true;
}

uint16_t MatterTemperatureControlledCabinet::getSupportedTemperatureLevelsCount() {
  if (useTemperatureNumber) {
    log_e("Temperature level methods require temperature_level feature. Use begin(supportedLevels, levelCount, selectedLevel) instead.");
    return 0;
  }

  esp_matter_attr_val_t levelsVal = esp_matter_invalid(NULL);
  if (getAttributeVal(TemperatureControl::Id, TemperatureControl::Attributes::SupportedTemperatureLevels::Id, &levelsVal)) {
    return levelsVal.val.a.n;  // a.n is the count (number of elements)
  }
  return 0;
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
