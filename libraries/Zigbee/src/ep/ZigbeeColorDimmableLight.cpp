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

#include <algorithm>
#include "ZigbeeColorDimmableLight.h"
#if CONFIG_ZB_ENABLED

ZigbeeColorDimmableLight::ZigbeeColorDimmableLight(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID;

  esp_zb_color_dimmable_light_cfg_t light_cfg = ZIGBEE_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
  _cluster_list = esp_zb_color_dimmable_light_clusters_create(&light_cfg);

  //Add support for hue and saturation
  uint8_t hue = 0;
  uint8_t saturation = 0;

  //Add support for Hue and Saturation
  uint16_t color_attr = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
  uint16_t min_temp = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_DEFAULT_VALUE;
  uint16_t max_temp = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_DEFAULT_VALUE;

  esp_zb_attribute_list_t *color_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, &hue);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID, &saturation);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &color_attr);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, &min_temp);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, &max_temp);
  uint8_t color_mode = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_MODE_DEFAULT_VALUE;
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, &color_mode);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID, .app_device_version = 0
  };

  //set default values
  _current_state = false;
  _current_level = 255;
  _current_color = {255, 255, 255};
  _current_hsv = {0, 0, 255};
  _current_color_temperature = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
  _current_color_mode = ZIGBEE_COLOR_MODE_CURRENT_X_Y; //default XY color mode
  _color_capabilities = ZIGBEE_COLOR_CAPABILITY_X_Y; //default XY color supported only

  // Initialize callbacks to nullptr
  _on_light_change = nullptr;
  _on_light_change_rgb = nullptr;
  _on_light_change_hsv = nullptr;
  _on_light_change_temp = nullptr;
}

uint16_t ZigbeeColorDimmableLight::getCurrentColorX() {
  return (*(uint16_t *)esp_zb_zcl_get_attribute(
             _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID
  )
             ->data_p);
}

uint16_t ZigbeeColorDimmableLight::getCurrentColorY() {
  return (*(uint16_t *)esp_zb_zcl_get_attribute(
             _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID
  )
             ->data_p);
}

uint8_t ZigbeeColorDimmableLight::getCurrentColorHue() {
  return (*(uint8_t *)esp_zb_zcl_get_attribute(
             _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID
  )
             ->data_p);
}

uint8_t ZigbeeColorDimmableLight::getCurrentColorSaturation() {
  return (*(uint8_t *)esp_zb_zcl_get_attribute(
             _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID
  )
             ->data_p);
}

uint16_t ZigbeeColorDimmableLight::getCurrentColorTemperature() {
  return (*(uint16_t *)esp_zb_zcl_get_attribute(
             _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID
  )
             ->data_p);
}

//set attribute method -> method overridden in child class
void ZigbeeColorDimmableLight::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      if (_current_state != *(bool *)message->attribute.data.value) {
        _current_state = *(bool *)message->attribute.data.value;
        lightChangedByMode();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for On/Off Light", message->attribute.id);
    }
  } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      if (_current_level != *(uint8_t *)message->attribute.data.value) {
        _current_level = *(uint8_t *)message->attribute.data.value;
        // Update HSV value if in HSV mode
        if (_current_color_mode == ZIGBEE_COLOR_MODE_HUE_SATURATION) {
          _current_hsv.v = _current_level;
        }
        lightChangedByMode();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Level Control", message->attribute.id);
    }
  } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM) {
      uint8_t new_color_mode = (*(uint8_t *)message->attribute.data.value);
      if (new_color_mode > ZIGBEE_COLOR_MODE_TEMPERATURE) {
        log_w("Invalid color mode received: %d", new_color_mode);
        return;
      }
      
      // Validate that the requested color mode is supported by capabilities
      bool mode_supported = false;
      switch (new_color_mode) {
        case ZIGBEE_COLOR_MODE_CURRENT_X_Y:
          mode_supported = (_color_capabilities & ZIGBEE_COLOR_CAPABILITY_X_Y) != 0;
          break;
        case ZIGBEE_COLOR_MODE_HUE_SATURATION:
          mode_supported = (_color_capabilities & ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION) != 0;
          break;
        case ZIGBEE_COLOR_MODE_TEMPERATURE:
          mode_supported = (_color_capabilities & ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP) != 0;
          break;
      }
      
      if (!mode_supported) {
        log_w("Color mode %d not supported by current capabilities: 0x%04x", new_color_mode, _color_capabilities);
        return;
      }
      
      _current_color_mode = new_color_mode;
      log_v("Color mode changed to: %d", _current_color_mode);
      // Don't call setLightColorMode() here - the attribute was already set externally
      // Just update our internal state
      return;
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      // Validate XY capability
      if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_X_Y)) {
        log_w("XY color capability not enabled, but XY attribute received. Current capabilities: 0x%04x", _color_capabilities);
        return;
      }
      uint16_t light_color_x = (*(uint16_t *)message->attribute.data.value);
      uint16_t light_color_y = getCurrentColorY();
      // Update color mode to XY if not already
      if (_current_color_mode != ZIGBEE_COLOR_MODE_CURRENT_X_Y) {
        setLightColorMode(ZIGBEE_COLOR_MODE_CURRENT_X_Y);
      }
      //calculate RGB from XY and call RGB callback
      _current_color = espXYToRgbColor(255, light_color_x, light_color_y, false);
      lightChangedRgb();
      return;

    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      // Validate XY capability
      if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_X_Y)) {
        log_w("XY color capability not enabled, but XY attribute received. Current capabilities: 0x%04x", _color_capabilities);
        return;
      }
      uint16_t light_color_x = getCurrentColorX();
      uint16_t light_color_y = (*(uint16_t *)message->attribute.data.value);
      // Update color mode to XY if not already
      if (_current_color_mode != ZIGBEE_COLOR_MODE_CURRENT_X_Y) {
        setLightColorMode(ZIGBEE_COLOR_MODE_CURRENT_X_Y);
      }
      //calculate RGB from XY and call RGB callback
      _current_color = espXYToRgbColor(255, light_color_x, light_color_y, false);
      lightChangedRgb();
      return;
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      // Validate Hue/Saturation capability
      if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION)) {
        log_w("Hue/Saturation color capability not enabled, but Hue attribute received. Current capabilities: 0x%04x", _color_capabilities);
        return;
      }
      uint8_t light_color_hue = (*(uint8_t *)message->attribute.data.value);
      // Update color mode to HS if not already
      if (_current_color_mode != ZIGBEE_COLOR_MODE_HUE_SATURATION) {
        setLightColorMode(ZIGBEE_COLOR_MODE_HUE_SATURATION);
      }
      // Store HSV values and call HSV callback (don't convert to RGB)
      _current_hsv.h = light_color_hue;
      _current_hsv.s = getCurrentColorSaturation();
      _current_hsv.v = _current_level; // Use level as value
      lightChangedHsv();
      return;
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      // Validate Hue/Saturation capability
      if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION)) {
        log_w("Hue/Saturation color capability not enabled, but Saturation attribute received. Current capabilities: 0x%04x", _color_capabilities);
        return;
      }
      uint8_t light_color_saturation = (*(uint8_t *)message->attribute.data.value);
      // Update color mode to HS if not already
      if (_current_color_mode != ZIGBEE_COLOR_MODE_HUE_SATURATION) {
        setLightColorMode(ZIGBEE_COLOR_MODE_HUE_SATURATION);
      }
      // Store HSV values and call HSV callback (don't convert to RGB)
      _current_hsv.h = getCurrentColorHue();
      _current_hsv.s = light_color_saturation;
      _current_hsv.v = _current_level; // Use level as value
      lightChangedHsv();
      return;
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      // Validate Color Temperature capability
      if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP)) {
        log_w("Color temperature capability not enabled, but Temperature attribute received. Current capabilities: 0x%04x", _color_capabilities);
        return;
      }
      uint16_t light_color_temp = (*(uint16_t *)message->attribute.data.value);
      // Update color mode to TEMP if not already
      if (_current_color_mode != ZIGBEE_COLOR_MODE_TEMPERATURE) {
        setLightColorMode(ZIGBEE_COLOR_MODE_TEMPERATURE);
      }
      if (_current_color_temperature != light_color_temp) {
        _current_color_temperature = light_color_temp;
        lightChangedTemp();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Color Control", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Color dimmable Light", message->info.cluster);
  }
}

void ZigbeeColorDimmableLight::lightChanged() {
  if (_on_light_change) {
    _on_light_change(_current_state, _current_color.r, _current_color.g, _current_color.b, _current_level);
  }
}

void ZigbeeColorDimmableLight::lightChangedRgb() {
  if (_on_light_change_rgb) {
    _on_light_change_rgb(_current_state, _current_color.r, _current_color.g, _current_color.b, _current_level);
  } else if (_on_light_change) { 
    // Fallback to legacy callback (deprecated) to maintain backward compatibility.
    _on_light_change(_current_state, _current_color.r, _current_color.g, _current_color.b, _current_level);
  }
}

void ZigbeeColorDimmableLight::lightChangedHsv() {
  if (_on_light_change_hsv) {
    _on_light_change_hsv(_current_state, _current_hsv.h, _current_hsv.s, _current_hsv.v, _current_level);
  }
}

void ZigbeeColorDimmableLight::lightChangedTemp() {
  if (_on_light_change_temp) {
    _on_light_change_temp(_current_state, _current_level, _current_color_temperature);
  }
}

void ZigbeeColorDimmableLight::lightChangedByMode() {
  // Call the appropriate callback based on current color mode
  switch (_current_color_mode) {
    case ZIGBEE_COLOR_MODE_CURRENT_X_Y:
      lightChangedRgb();
      break;
    case ZIGBEE_COLOR_MODE_HUE_SATURATION:
      lightChangedHsv();
      break;
    case ZIGBEE_COLOR_MODE_TEMPERATURE:
      lightChangedTemp();
      break;
    default:
      log_e("Unknown color mode: %d", _current_color_mode);
      break;
  }
}

bool ZigbeeColorDimmableLight::setLight(bool state, uint8_t level, uint8_t red, uint8_t green, uint8_t blue) {
  // Check if XY color capability is enabled
  if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_X_Y)) {
    log_e("XY color capability not enabled. Current capabilities: 0x%04x", _color_capabilities);
    return false;
  }
  if (!setLightColorMode(ZIGBEE_COLOR_MODE_CURRENT_X_Y)) {
    log_e("Failed to set light color mode: %d", ZIGBEE_COLOR_MODE_CURRENT_X_Y);
    return false;
  }
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  // Update all attributes
  _current_state = state;
  _current_level = level;
  _current_color = {red, green, blue};
  lightChangedRgb();

  espXyColor_t xy_color = espRgbColorToXYColor(_current_color);
  espHsvColor_t hsv_color = espRgbColorToHsvColor(_current_color);
  uint8_t hue = std::min((uint8_t)hsv_color.h, (uint8_t)254);         // Clamp to 0-254
  uint8_t saturation = std::min((uint8_t)hsv_color.s, (uint8_t)254);  // Clamp to 0-254
  // Update HSV state
  _current_hsv = hsv_color;

  log_v("Updating light state: %d, level: %d, color: %d, %d, %d", state, level, red, green, blue);
  /* Update light clusters */
  esp_zb_lock_acquire(portMAX_DELAY);
  //set on/off state
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &_current_state, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light state: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set level
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, &_current_level, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light level: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set x color
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, &xy_color.x, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light xy color: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set y color
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, &xy_color.y, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light y color: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set hue (for compatibility)
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, &hue, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light hue: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set saturation (for compatibility)
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID, &saturation, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light saturation: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

bool ZigbeeColorDimmableLight::setLightState(bool state) {
  return setLight(state, _current_level, _current_color.r, _current_color.g, _current_color.b);
}

bool ZigbeeColorDimmableLight::setLightLevel(uint8_t level) {
  return setLight(_current_state, level, _current_color.r, _current_color.g, _current_color.b);
}

bool ZigbeeColorDimmableLight::setLightColor(uint8_t red, uint8_t green, uint8_t blue) {
  return setLight(_current_state, _current_level, red, green, blue);
}

bool ZigbeeColorDimmableLight::setLightColor(espRgbColor_t rgb_color) {
  return setLight(_current_state, _current_level, rgb_color.r, rgb_color.g, rgb_color.b);
}

bool ZigbeeColorDimmableLight::setLightColor(espHsvColor_t hsv_color) {
  // Check if Hue/Saturation color capability is enabled
  if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION)) {
    log_e("Hue/Saturation color capability not enabled. Current capabilities: 0x%04x", _color_capabilities);
    return false;
  }
  
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  if (!setLightColorMode(ZIGBEE_COLOR_MODE_HUE_SATURATION)) {
    log_e("Failed to set light color mode: %d", ZIGBEE_COLOR_MODE_HUE_SATURATION);
    return false;
  }
  // Update HSV state
  _current_hsv = hsv_color;
  lightChangedHsv();

  uint8_t hue = std::min((uint8_t)hsv_color.h, (uint8_t)254);         // Clamp to 0-254
  uint8_t saturation = std::min((uint8_t)hsv_color.s, (uint8_t)254);  // Clamp to 0-254

  log_v("Updating light HSV: H=%d, S=%d, V=%d", hue, saturation, hsv_color.v);
  /* Update light clusters */
  esp_zb_lock_acquire(portMAX_DELAY);
  //set hue
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, &hue, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light hue: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set saturation
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID, &saturation, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light saturation: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

bool ZigbeeColorDimmableLight::setLightColorTemperature(uint16_t color_temperature) {
  // Check if Color Temperature capability is enabled
  if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP)) {
    log_e("Color temperature capability not enabled. Current capabilities: 0x%04x", _color_capabilities);
    return false;
  }
  if (!setLightColorMode(ZIGBEE_COLOR_MODE_TEMPERATURE)) {
    log_e("Failed to set light color mode: %d", ZIGBEE_COLOR_MODE_TEMPERATURE);
    return false;
  }

  _current_color_temperature = color_temperature;
  lightChangedTemp();

  log_v("Updating light color temperature: %d", color_temperature);

  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  /* Update light clusters */
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &_current_color_temperature, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light color temperature: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

bool ZigbeeColorDimmableLight::setLightColorMode(uint8_t color_mode) {
  if (color_mode > ZIGBEE_COLOR_MODE_TEMPERATURE) {
    log_e("Invalid color mode: %d", color_mode);
    return false;
  }
  
  // Check if the requested color mode is supported by capabilities
  bool mode_supported = false;
  switch (color_mode) {
    case ZIGBEE_COLOR_MODE_CURRENT_X_Y:
      mode_supported = (_color_capabilities & ZIGBEE_COLOR_CAPABILITY_X_Y) != 0;
      break;
    case ZIGBEE_COLOR_MODE_HUE_SATURATION:
      mode_supported = (_color_capabilities & ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION) != 0;
      break;
    case ZIGBEE_COLOR_MODE_TEMPERATURE:
      mode_supported = (_color_capabilities & ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP) != 0;
      break;
  }
  
  if (!mode_supported) {
    log_e("Color mode %d not supported by current capabilities: 0x%04x", color_mode, _color_capabilities);
    return false;
  }
  
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  log_v("Setting color mode: %d", color_mode);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, &color_mode, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light color mode: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _current_color_mode = color_mode;
  return true;
}

bool ZigbeeColorDimmableLight::setLightColorCapabilities(uint16_t capabilities) {
  esp_zb_attribute_list_t *color_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (!color_cluster) {
    log_e("Color control cluster not found");
    return false;
  }
  
  // Validate capabilities (max value is 0x001f per ZCL spec)
  if (capabilities > 0x001f) {
    log_e("Invalid color capabilities value: 0x%04x (max: 0x001f)", capabilities);
    return false;
  }
  
  _color_capabilities = capabilities;
  
  esp_err_t ret = esp_zb_cluster_update_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_CAPABILITIES_ID, &_color_capabilities);
  if (ret != ESP_OK) {
    log_e("Failed to set color capabilities: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  
  log_v("Color capabilities set to: 0x%04x", _color_capabilities);
  return true;
}

bool ZigbeeColorDimmableLight::setLightColorTemperatureRange(uint16_t min_temp, uint16_t max_temp) {
  esp_zb_attribute_list_t *color_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (!color_cluster) {
    log_e("Color control cluster not found");
    return false;
  }
  if (!(_color_capabilities & ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP)) {
    log_e("Color temperature capability not enabled. Current capabilities: 0x%04x", _color_capabilities);
    return false;
  }
  esp_err_t ret = esp_zb_cluster_update_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, &min_temp);
  if (ret != ESP_OK) {
    log_e("Failed to set min value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, &max_temp);
  if (ret != ESP_OK) {
    log_e("Failed to set max value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

#endif  // CONFIG_ZB_ENABLED
