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
#include <app/server/Server.h>
#include <MatterEndpoints/MatterColorLight.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

namespace {
espXyColor_t hsvToXyColor(espHsvColor_t hsv) {
  return espRgbColorToXYColor(espHsvColorToRgbColor(hsv));
}

uint8_t clampColor254(uint8_t value) {
  return value > 254 ? 254 : value;
}

uint8_t clampCurrentLevel(uint8_t value) {
  if (value < 1) {
    return 1;
  }
  return clampColor254(value);
}

void reportAttribute(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t val) {
  attribute::report(endpoint_id, cluster_id, attribute_id, &val);
}

void syncHsvToColorCluster(uint16_t endpoint_id, espHsvColor_t hsv) {
  espXyColor_t xy = hsvToXyColor(hsv);
  const uint8_t colorMode = (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation;
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorMode::Id, esp_matter_enum8(colorMode));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::EnhancedColorMode::Id, esp_matter_enum8(colorMode));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id, esp_matter_uint8(clampColor254((uint8_t)hsv.h)));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id, esp_matter_uint8(clampColor254(hsv.s)));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentX::Id, esp_matter_uint16(xy.x));
  reportAttribute(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentY::Id, esp_matter_uint16(xy.y));  // codespell:ignore
  reportAttribute(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, esp_matter_nullable_uint8(clampCurrentLevel(hsv.v)));
}

// Matter 1.5 has no Color Light device type. extended_color_light always adds
// Color Temperature (Alexa shows a CT slider). Build dimmable lighting + HS/XY only.
endpoint_t *createRgbColorLightEndpoint(node_t *node, dimmable_light::config_t *config, void *priv_data, espHsvColor_t hsv, espXyColor_t xy) {
  endpoint_t *endpoint = esp_matter::endpoint::create(node, ENDPOINT_FLAG_NONE, priv_data);
  if (endpoint == nullptr) {
    return nullptr;
  }

  descriptor::create(endpoint, &(config->descriptor), CLUSTER_FLAG_SERVER);
  if (esp_matter::endpoint::add_device_type(endpoint, ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_ID, ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_VERSION) !=
      ESP_OK) {
    return nullptr;
  }

  cluster_t *identify_cluster = identify::create(endpoint, &(config->identify), CLUSTER_FLAG_SERVER);
  identify::command::create_trigger_effect(identify_cluster);
  groups::create(endpoint, &(config->groups), CLUSTER_FLAG_SERVER);

  cluster_t *on_off_cluster = on_off::create(endpoint, &(config->on_off), CLUSTER_FLAG_SERVER);
  on_off::feature::lighting::add(on_off_cluster, &(config->on_off_lighting));
  on_off::command::create_on(on_off_cluster);
  on_off::command::create_toggle(on_off_cluster);

  cluster_t *level_control_cluster = level_control::create(endpoint, &(config->level_control), CLUSTER_FLAG_SERVER);
  level_control::feature::on_off::add(level_control_cluster);
  level_control::feature::lighting::add(level_control_cluster, &(config->level_control_lighting));

  cluster_t *scenes_management_cluster = scenes_management::create(endpoint, &(config->scenes_management), CLUSTER_FLAG_SERVER);
  scenes_management::command::create_copy_scene(scenes_management_cluster);
  scenes_management::command::create_copy_scene_response(scenes_management_cluster);

  color_control::config_t cc_config;
  cc_config.color_mode = (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation;
  cc_config.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation;
  cluster_t *color_control_cluster = color_control::create(endpoint, &cc_config, CLUSTER_FLAG_SERVER);
  if (color_control_cluster == nullptr) {
    return nullptr;
  }

  color_control::feature::hue_saturation::config_t hs_config;
  hs_config.current_hue = (uint8_t)hsv.h;
  hs_config.current_saturation = hsv.s;
  color_control::feature::hue_saturation::add(color_control_cluster, &hs_config);

  color_control::feature::xy::config_t xy_config;
  xy_config.current_x = xy.x;
  xy_config.current_y = xy.y;
  color_control::feature::xy::add(color_control_cluster, &xy_config);

  color_control::attribute::create_remaining_time(color_control_cluster, 0);
  color_control::command::create_stop_move_step(color_control_cluster);
  return endpoint;
}
}  // namespace

bool MatterColorLight::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter RGB Color Light device has not begun.");
    return false;
  }

  log_d(
    "RGB Color Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32 ", type: %u", endpoint_id, cluster_id,
    attribute_id, val->val.u32, val->type
  );

  if (endpoint_id == getEndPointId()) {
    switch (cluster_id) {
      case OnOff::Id:
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
          log_d("RGB Color Light On/Off State changed to %u", val->val.b);
          if (_onChangeOnOffCB != NULL) {
            ret &= _onChangeOnOffCB(val->val.b);
          }
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(val->val.b, colorHSV);
          }
          if (ret == true) {
            onOffState = val->val.b;
          }
        }
        break;
      case LevelControl::Id:
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
          log_d("RGB Color Light Brightness changed to %u", val->val.u8);
          if (_onChangeColorCB != NULL) {
            ret &= _onChangeColorCB({colorHSV.h, colorHSV.s, val->val.u8});
          }
          if (_onChangeCB != NULL) {
            ret &= _onChangeCB(onOffState, {colorHSV.h, colorHSV.s, val->val.u8});
          }
          if (ret == true) {
            colorHSV.v = val->val.u8;
          }
        }
        break;
      case ColorControl::Id:
      {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
          log_d("RGB Light Hue changed to %u", val->val.u8);
          colorHSV.h = val->val.u8;
        } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
          log_d("RGB Light Saturation changed to %u", val->val.u8);
          colorHSV.s = val->val.u8;
        } else if (attribute_id == ColorControl::Attributes::CurrentX::Id || attribute_id == ColorControl::Attributes::CurrentY::Id) {  // codespell:ignore
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
          espRgbColor_t rgb = espXYToRgbColor(255, x, y, false);
          espHsvColor_t xyHsv = espRgbColorToHsvColor(rgb);
          colorHSV.h = (uint8_t)xyHsv.h;
          colorHSV.s = xyHsv.s;
          log_d("RGB Light XY changed — HSV updated to h=%u s=%u", colorHSV.h, colorHSV.s);
        } else if (
          attribute_id == ColorControl::Attributes::ColorMode::Id || attribute_id == ColorControl::Attributes::EnhancedColorMode::Id
          || attribute_id == ColorControl::Attributes::RemainingTime::Id || attribute_id == ColorControl::Attributes::Options::Id
        ) {
          log_d("RGB Light ColorMode/Options/RemainingTime attribute 0x%" PRIx32 " = %u", attribute_id, val->val.u16);
          break;
        } else {
          log_i("Color Control Attribute ID [0x%" PRIx32 "] not processed.", attribute_id);
          break;
        }
        if (_onChangeColorCB != NULL) {
          ret &= _onChangeColorCB(colorHSV);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(onOffState, colorHSV);
        }
        break;
      }
    }
  }
  return ret;
}

MatterColorLight::MatterColorLight() {}

MatterColorLight::~MatterColorLight() {
  end();
}

bool MatterColorLight::begin(bool initialState, espHsvColor_t _colorHSV) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter RGB Color Light with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  espXyColor_t xy = hsvToXyColor(_colorHSV);

  dimmable_light::config_t light_config;
  light_config.on_off.on_off = initialState;
  light_config.on_off_lighting.start_up_on_off = nullptr;
  onOffState = initialState;

  light_config.level_control.current_level = _colorHSV.v;
  light_config.level_control_lighting.start_up_current_level = nullptr;
  colorHSV = {_colorHSV.h, _colorHSV.s, _colorHSV.v};

  endpoint_t *endpoint = createRgbColorLightEndpoint(node::get(), &light_config, (void *)this, colorHSV, xy);
  if (endpoint == nullptr) {
    log_e("Failed to create RGB Color light endpoint");
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));
  log_i("RGB Color Light created with endpoint_id %u", getEndPointId());

  /* Mark deferred persistence for some attributes that might be changed rapidly */
  cluster_t *level_control_cluster = cluster::get(endpoint, LevelControl::Id);
  esp_matter::attribute_t *current_level_attribute = attribute::get(level_control_cluster, LevelControl::Attributes::CurrentLevel::Id);
  attribute::set_deferred_persistence(current_level_attribute);

  started = true;
  return true;
}

void MatterColorLight::end() {
  started = false;
}

bool MatterColorLight::setOnOff(bool newState) {
  if (!started) {
    log_e("Matter RGB Color Light device has not begun.");
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

void MatterColorLight::updateAccessory() {
  if (_onChangeCB != NULL) {
    _onChangeCB(onOffState, colorHSV);
  }
}

bool MatterColorLight::getOnOff() {
  return onOffState;
}

bool MatterColorLight::toggle() {
  return setOnOff(!onOffState);
}

bool MatterColorLight::setColorRGB(espRgbColor_t _rgbColor) {
  return setColorHSV(espRgbColorToHsvColor(_rgbColor));
}

espRgbColor_t MatterColorLight::getColorRGB() {
  return espHsvColorToRgbColor(colorHSV);
}

bool MatterColorLight::setColorHSV(espHsvColor_t _hsvColor) {

  if (!started) {
    log_w("Matter RGB Color Light device has not begun.");
    return false;
  }

  // avoid processing if there was no change
  if (colorHSV.h == _hsvColor.h && colorHSV.s == _hsvColor.s && colorHSV.v == _hsvColor.v) {
    return true;
  }

  colorHSV = {_hsvColor.h, _hsvColor.s, _hsvColor.v};
  syncHsvToColorCluster(endpoint_id, colorHSV);

  if (_onChangeColorCB != NULL) {
    _onChangeColorCB(colorHSV);
  }
  if (_onChangeCB != NULL) {
    _onChangeCB(onOffState, colorHSV);
  }
  return true;
}

espHsvColor_t MatterColorLight::getColorHSV() {
  return colorHSV;
}

MatterColorLight::operator bool() {
  return getOnOff();
}

void MatterColorLight::operator=(bool newState) {
  setOnOff(newState);
}
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
