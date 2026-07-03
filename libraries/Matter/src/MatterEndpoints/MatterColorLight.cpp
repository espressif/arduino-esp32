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

bool updateColorControlAttribute(uint16_t endpoint_id, uint32_t attribute_id, esp_matter_attr_val_t val) {
  return attribute::update(endpoint_id, ColorControl::Id, attribute_id, &val) == ESP_OK;
}

void syncHsvToColorCluster(uint16_t endpoint_id, espHsvColor_t hsv) {
  espXyColor_t xy = hsvToXyColor(hsv);

  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  val.type = ESP_MATTER_VAL_TYPE_UINT8;
  val.val.u8 = hsv.h;
  updateColorControlAttribute(endpoint_id, ColorControl::Attributes::CurrentHue::Id, val);

  val.val.u8 = hsv.s;
  updateColorControlAttribute(endpoint_id, ColorControl::Attributes::CurrentSaturation::Id, val);

  val.type = ESP_MATTER_VAL_TYPE_UINT16;
  val.val.u16 = xy.x;
  updateColorControlAttribute(endpoint_id, ColorControl::Attributes::CurrentX::Id, val);

  val.val.u16 = xy.y;
  updateColorControlAttribute(endpoint_id, ColorControl::Attributes::CurrentY::Id, val);  // codespell:ignore

  val.type = ESP_MATTER_VAL_TYPE_UINT8;
  val.val.u8 = hsv.v;
  attribute::update(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, &val);
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
          esp_matter_attr_val_t xVal = esp_matter_invalid(NULL);
          esp_matter_attr_val_t yVal = esp_matter_invalid(NULL);
          getAttributeVal(ColorControl::Id, ColorControl::Attributes::CurrentX::Id, &xVal);
          getAttributeVal(ColorControl::Id, ColorControl::Attributes::CurrentY::Id, &yVal);  // codespell:ignore
          espRgbColor_t rgb = espXYToRgbColor(colorHSV.v, xVal.val.u16, yVal.val.u16, true);
          colorHSV = espRgbColorToHsvColor(rgb);
          log_d("RGB Light XY changed — HSV updated to h=%u s=%u", colorHSV.h, colorHSV.s);
        } else {
          log_i("Color Control Attribute ID [%]" PRIx32 " not processed.", attribute_id);
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

  extended_color_light::config_t light_config;
  light_config.on_off.on_off = initialState;
  light_config.on_off_lighting.start_up_on_off = nullptr;
  onOffState = initialState;

  light_config.level_control.current_level = _colorHSV.v;
  light_config.level_control_lighting.start_up_current_level = nullptr;

  light_config.color_control.color_mode = (uint8_t)ColorControl::ColorMode::kCurrentXAndCurrentY;
  light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::kCurrentXAndCurrentY;
  light_config.color_control_xy.current_x = xy.x;
  light_config.color_control_xy.current_y = xy.y;
  colorHSV = {_colorHSV.h, _colorHSV.s, _colorHSV.v};

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = extended_color_light::create(node::get(), &light_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create RGB Color light endpoint");
    return false;
  }

  // Hue/saturation for the Arduino HSV API (official extended_color_light uses XY + color temperature)
  color_control::feature::hue_saturation::config_t hs_config;
  hs_config.current_hue = _colorHSV.h;
  hs_config.current_saturation = _colorHSV.s;
  cluster_t *color_control_cluster = cluster::get(endpoint, ColorControl::Id);
  color_control::feature::hue_saturation::add(color_control_cluster, &hs_config);

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
