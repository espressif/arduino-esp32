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

#include <Matter.h>
#include <MatterEndpoints/MatterEnhancedColorLight.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

namespace {
espXyColor_t hsvToXyColor(espHsvColor_t hsv) {
  return espRgbColorToXYColor(espHsvColorToRgbColor(hsv));
}

// Matter HS is 0..254. CurrentLevel is 1..254 (255 is the nullable null sentinel).
uint8_t clampColor254(uint8_t value) {
  return value > 254 ? 254 : value;
}

uint8_t clampHue254(uint16_t hue) {
  return hue > 254 ? 254 : (uint8_t)hue;
}

uint8_t clampCurrentLevel(uint8_t value) {
  if (value < 1) {
    return 1;
  }
  return clampColor254(value);
}

espHsvColor_t clampHsvColor(espHsvColor_t hsv) {
  return {clampHue254(hsv.h), clampColor254(hsv.s), clampCurrentLevel(hsv.v)};
}

// report() notifies subscribers without PRE_UPDATE, so setColorHSV() does not
// invoke onChangeColorHSV() once per Hue/Sat/X/Y write.
void reportAttribute(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t val) {
  attribute::report(endpoint_id, cluster_id, attribute_id, &val);
}

void reportColorMode(uint16_t endpoint_id, ColorControl::ColorMode mode) {
  const uint8_t modeVal = (uint8_t)mode;
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorMode::Id, esp_matter_enum8(modeVal));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::EnhancedColorMode::Id, esp_matter_enum8(modeVal));
}

void syncHsvToColorCluster(uint16_t endpoint_id, espHsvColor_t hsv) {
  espXyColor_t xy = hsvToXyColor(hsv);
  reportColorMode(endpoint_id, ColorControl::ColorMode::kCurrentHueAndCurrentSaturation);
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id, esp_matter_uint8(clampHue254(hsv.h)));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id, esp_matter_uint8(clampColor254(hsv.s)));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentX::Id, esp_matter_uint16(xy.x));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentY::Id, esp_matter_uint16(xy.y));  // codespell:ignore
  // CurrentLevel is nullable uint8; ESP_MATTER_VAL_TYPE_UINT8 returns err 258.
  reportAttribute(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, esp_matter_nullable_uint8(clampCurrentLevel(hsv.v)));
}
}  // namespace

bool MatterEnhancedColorLight::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter Enhanced ColorLight device has not begun.");
    return false;
  }

  log_d(
    "Enhanced ColorAttr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32 ", type: %u", endpoint_id, cluster_id,
    attribute_id, val->val.u32, val->type
  );

  if (endpoint_id == getEndPointId()) {
    switch (cluster_id) {
      case OnOff::Id:
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
          log_d("Enhanced ColorLight On/Off State changed to %u", val->val.b);
          if (_onChangeOnOffCB != NULL) {
            ret &= _onChangeOnOffCB(val->val.b);
          }
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(val->val.b, colorHSV, brightnessLevel, colorTemperatureLevel);
          }
          if (ret == true) {
            onOffState = val->val.b;
          }
        }
        break;
      case LevelControl::Id:
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
          log_d("Enhanced ColorLight Brightness changed to %u", val->val.u8);
          if (_onChangeBrightnessCB != NULL) {
            ret &= _onChangeBrightnessCB(val->val.u8);
          }
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(onOffState, colorHSV, val->val.u8, colorTemperatureLevel);
          }
          if (ret == true) {
            brightnessLevel = val->val.u8;
            colorHSV.v = val->val.u8;
          }
        }
        break;
      case ColorControl::Id:
      {
        if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
          log_d("Enhanced ColorLight Temperature changed to %u", val->val.u16);
          if (_onChangeTemperatureCB != NULL) {
            ret &= _onChangeTemperatureCB(val->val.u16);
          }
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(onOffState, colorHSV, brightnessLevel, val->val.u16);
          }
          if (ret == true) {
            colorTemperatureLevel = val->val.u16;
          }
          break;
        }
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
          log_d("Enhanced ColorLight Hue changed to %u", val->val.u8);
          colorHSV.h = val->val.u8;
        } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
          log_d("Enhanced ColorLight Saturation changed to %u", val->val.u8);
          colorHSV.s = val->val.u8;
        } else if (attribute_id == ColorControl::Attributes::CurrentX::Id || attribute_id == ColorControl::Attributes::CurrentY::Id) {  // codespell:ignore
          // PRE_UPDATE still has the old stored value; use `val` for the attribute being written.
          uint16_t x, y;
          if (attribute_id == ColorControl::Attributes::CurrentX::Id) {
            esp_matter_attr_val_t yVal = esp_matter_invalid(NULL);
            x = val->val.u16;
            getAttributeVal(ColorControl::Id, ColorControl::Attributes::CurrentY::Id, &yVal);  // codespell:ignore
            y = yVal.val.u16;
          } else {
            esp_matter_attr_val_t xVal = esp_matter_invalid(NULL);
            getAttributeVal(ColorControl::Id, ColorControl::Attributes::CurrentX::Id, &xVal);
            x = xVal.val.u16;
            y = val->val.u16;
          }
          // Chromaticity only: keep CurrentLevel as V. Hue is uint16_t; take 8-bit HSV hue.
          espRgbColor_t rgb = espXYToRgbColor(255, x, y, false);
          espHsvColor_t xyHsv = espRgbColorToHsvColor(rgb);
          // uint8_t recovers the classic HSV wrap; then clamp 255 (reserved / full-scale).
          colorHSV.h = clampColor254((uint8_t)xyHsv.h);
          colorHSV.s = clampColor254(xyHsv.s);
          log_d("Enhanced ColorLight XY changed — HSV updated to h=%u s=%u", colorHSV.h, colorHSV.s);
        } else if (attribute_id == ColorControl::Attributes::ColorMode::Id || attribute_id == ColorControl::Attributes::EnhancedColorMode::Id
                   || attribute_id == ColorControl::Attributes::RemainingTime::Id || attribute_id == ColorControl::Attributes::Options::Id) {
          // ColorMode / RemainingTime / Options updates do not change the HSV cache.
          if (attribute_id == ColorControl::Attributes::RemainingTime::Id) {
            log_d("Enhanced ColorLight RemainingTime attribute 0x%" PRIx32 " = %u", attribute_id, val->val.u16);
          } else {
            log_d("Enhanced ColorLight ColorMode/Options attribute 0x%" PRIx32 " = %u", attribute_id, val->val.u8);
          }
          break;
        } else {
          log_i("Color Control Attribute ID [0x%" PRIx32 "] not processed.", attribute_id);
          break;
        }
        if (_onChangeColorCB != NULL) {
          ret &= _onChangeColorCB(colorHSV);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(onOffState, colorHSV, brightnessLevel, colorTemperatureLevel);
        }
        break;
      }
    }
  }
  return ret;
}

MatterEnhancedColorLight::MatterEnhancedColorLight() {}

MatterEnhancedColorLight::~MatterEnhancedColorLight() {
  end();
}

bool MatterEnhancedColorLight::begin(bool initialState, espHsvColor_t _colorHSV, uint8_t brightness, uint16_t ColorTemperature) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Enhanced ColorLight with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  colorHSV = clampHsvColor(_colorHSV);
  brightnessLevel = clampCurrentLevel(brightness);
  colorHSV.v = brightnessLevel;
  espXyColor_t xy = hsvToXyColor(colorHSV);

  extended_color_light::config_t light_config;
  light_config.on_off.on_off = initialState;
  light_config.on_off_lighting.start_up_on_off = nullptr;
  onOffState = initialState;

  light_config.level_control.current_level = brightnessLevel;
  light_config.level_control_lighting.start_up_current_level = nullptr;

  light_config.color_control.color_mode = (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation;
  light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation;
  light_config.color_control_xy.current_x = xy.x;
  light_config.color_control_xy.current_y = xy.y;
  light_config.color_control_color_temperature.color_temperature_mireds = ColorTemperature;
  light_config.color_control_color_temperature.start_up_color_temperature_mireds = nullptr;
  colorTemperatureLevel = ColorTemperature;

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = extended_color_light::create(node::get(), &light_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create Enhanced ColorLight endpoint");
    return false;
  }

  // Hue/saturation for the Arduino HSV API (official extended_color_light uses XY + color temperature)
  color_control::feature::hue_saturation::config_t hs_config;
  hs_config.current_hue = clampHue254(colorHSV.h);
  hs_config.current_saturation = clampColor254(colorHSV.s);
  cluster_t *color_control_cluster = cluster::get(endpoint, ColorControl::Id);
  color_control::feature::hue_saturation::add(color_control_cluster, &hs_config);

  setEndPointId(endpoint::get_id(endpoint));

  log_i("Enhanced ColorLight created with endpoint_id %u", getEndPointId());

  /* Mark deferred persistence for some attributes that might be changed rapidly */
  cluster_t *level_control_cluster = cluster::get(endpoint, LevelControl::Id);
  esp_matter::attribute_t *current_level_attribute = attribute::get(level_control_cluster, LevelControl::Attributes::CurrentLevel::Id);
  attribute::set_deferred_persistence(current_level_attribute);

  started = true;
  return true;
}

void MatterEnhancedColorLight::end() {
  started = false;
}

bool MatterEnhancedColorLight::setOnOff(bool newState) {
  if (!started) {
    log_e("Matter Enhanced ColorLight device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (onOffState == newState) {
    return true;
  }

  onOffState = newState;

  endpoint_t *endpoint = endpoint::get(node::get(), endpoint_id);
  cluster_t *cluster = cluster::get(endpoint, OnOff::Id);
  esp_matter::attribute_t *attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);

  if (val.val.b != onOffState) {
    val.val.b = onOffState;
    attribute::update(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
  }
  return true;
}

void MatterEnhancedColorLight::updateAccessory() {
  if (_onChangeCB != NULL) {
    _onChangeCB(onOffState, colorHSV, brightnessLevel, colorTemperatureLevel);
  }
}

bool MatterEnhancedColorLight::getOnOff() {
  return onOffState;
}

bool MatterEnhancedColorLight::toggle() {
  return setOnOff(!onOffState);
}

bool MatterEnhancedColorLight::setBrightness(uint8_t newBrightness) {
  if (!started) {
    log_w("Matter Enhanced ColorLight device has not begun.");
    return false;
  }

  const uint8_t brightness = clampCurrentLevel(newBrightness);

  // avoid processing if there was no change
  if (brightnessLevel == brightness) {
    return true;
  }

  brightnessLevel = brightness;
  colorHSV.v = brightness;

  endpoint_t *endpoint = endpoint::get(node::get(), endpoint_id);
  cluster_t *cluster = cluster::get(endpoint, LevelControl::Id);
  esp_matter::attribute_t *attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);

  if (val.val.u8 != brightnessLevel) {
    val.val.u8 = brightnessLevel;
    attribute::update(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, &val);
  }
  return true;
}

uint8_t MatterEnhancedColorLight::getBrightness() {
  return brightnessLevel;
}

bool MatterEnhancedColorLight::setColorTemperature(uint16_t newTemperature) {
  if (!started) {
    log_w("Matter Enhanced ColorLight device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (colorTemperatureLevel == newTemperature) {
    return true;
  }

  colorTemperatureLevel = newTemperature;

  endpoint_t *endpoint = endpoint::get(node::get(), endpoint_id);
  cluster_t *cluster = cluster::get(endpoint, ColorControl::Id);
  esp_matter::attribute_t *attribute = attribute::get(cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);

  if (val.val.u16 != colorTemperatureLevel) {
    reportColorMode(endpoint_id, ColorControl::ColorMode::kColorTemperature);
    val.val.u16 = colorTemperatureLevel;
    attribute::update(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id, &val);
  }
  return true;
}

uint16_t MatterEnhancedColorLight::getColorTemperature() {
  return colorTemperatureLevel;
}

bool MatterEnhancedColorLight::setColorRGB(espRgbColor_t _rgbColor) {
  return setColorHSV(espRgbColorToHsvColor(_rgbColor));
}

espRgbColor_t MatterEnhancedColorLight::getColorRGB() {
  return espHsvColorToRgbColor(colorHSV);
}

bool MatterEnhancedColorLight::setColorHSV(espHsvColor_t _hsvColor) {

  if (!started) {
    log_w("Matter Enhanced ColorLight device has not begun.");
    return false;
  }

  const espHsvColor_t hsvColor = clampHsvColor(_hsvColor);

  // avoid processing if there was no change
  if (colorHSV.h == hsvColor.h && colorHSV.s == hsvColor.s && colorHSV.v == hsvColor.v) {
    return true;
  }

  colorHSV = hsvColor;
  const bool brightnessChanged = (brightnessLevel != hsvColor.v);
  brightnessLevel = hsvColor.v;
  syncHsvToColorCluster(endpoint_id, colorHSV);

  if (_onChangeColorCB != NULL) {
    _onChangeColorCB(colorHSV);
  }
  if (brightnessChanged && _onChangeBrightnessCB != NULL) {
    _onChangeBrightnessCB(brightnessLevel);
  }
  if (_onChangeCB != NULL) {
    _onChangeCB(onOffState, colorHSV, brightnessLevel, colorTemperatureLevel);
  }
  return true;
}

espHsvColor_t MatterEnhancedColorLight::getColorHSV() {
  return colorHSV;
}

MatterEnhancedColorLight::operator bool() {
  return getOnOff();
}

void MatterEnhancedColorLight::operator=(bool newState) {
  setOnOff(newState);
}
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
