// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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
using namespace chip::app::Clusters;

// endpoint for color light device
namespace esp_matter {
using namespace cluster;
namespace endpoint {
namespace rgb_color_light {
typedef struct config {
  cluster::descriptor::config_t descriptor;
  cluster::identify::config_t identify;
  cluster::groups::config_t groups;
  cluster::scenes_management::config_t scenes_management;
  cluster::on_off::config_t on_off;
  cluster::level_control::config_t level_control;
  cluster::color_control::config_t color_control;
} config_t;

uint32_t get_device_type_id() {
  return ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_ID;
}

uint8_t get_device_type_version() {
  return ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_VERSION;
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

  descriptor::create(endpoint, &(config->descriptor), CLUSTER_FLAG_SERVER);
  cluster_t *identify_cluster = identify::create(endpoint, &(config->identify), CLUSTER_FLAG_SERVER);
  identify::command::create_trigger_effect(identify_cluster);
  groups::create(endpoint, &(config->groups), CLUSTER_FLAG_SERVER);
  cluster_t *scenes_cluster = scenes_management::create(endpoint, &(config->scenes_management), CLUSTER_FLAG_SERVER);
  scenes_management::command::create_copy_scene(scenes_cluster);
  scenes_management::command::create_copy_scene_response(scenes_cluster);

  on_off::create(endpoint, &(config->on_off), CLUSTER_FLAG_SERVER, on_off::feature::lighting::get_id());
  level_control::create(
    endpoint, &(config->level_control), CLUSTER_FLAG_SERVER, level_control::feature::on_off::get_id() | level_control::feature::lighting::get_id()
  );
  color_control::create(endpoint, &(config->color_control), CLUSTER_FLAG_SERVER, color_control::feature::hue_saturation::get_id());
  return ESP_OK;
}

endpoint_t *create(node_t *node, config_t *config, uint8_t flags, void *priv_data) {
  endpoint_t *endpoint = endpoint::create(node, flags, priv_data);
  add(endpoint, config);
  return endpoint;
}
}  // namespace rgb_color_light
}  // namespace endpoint
}  // namespace esp_matter

bool MatterColorLight::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;
  if (!started) {
    log_e("Matter RGB Color Light device has not begun.");
    return false;
  }

  log_d(
    "RGB Color Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u, type: %u", endpoint_id, cluster_id, attribute_id, val->val.u32,
    val->type
  );

  if (endpoint_id == getEndPointId()) {
    switch (cluster_id) {
      case OnOff::Id:
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
          log_d("RGB Color Light On/Off State changed to %d", val->val.b);
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
          log_d("RGB Color Light Brightness changed to %d", val->val.u8);
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
        if (attribute_id != ColorControl::Attributes::CurrentHue::Id && attribute_id != ColorControl::Attributes::CurrentSaturation::Id) {
          log_i("Color Control Attribute ID [%x] not processed.", attribute_id);
          break;
        }
        espHsvColor_t hsvColor = {colorHSV.h, colorHSV.s, colorHSV.v};
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
          log_d("RGB Light Hue changed to %d", val->val.u8);
          hsvColor.h = val->val.u8;
        } else {  // attribute_id == ColorControl::Attributes::CurrentSaturation::Id)
          log_d("RGB Light Saturation changed to %d", val->val.u8);
          hsvColor.s = val->val.u8;
        }
        if (_onChangeColorCB != NULL) {
          ret &= _onChangeColorCB(hsvColor);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(onOffState, hsvColor);
        }
        if (ret == true) {
          colorHSV = {hsvColor.h, hsvColor.s, hsvColor.v};
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
    log_e("Matter RGB Color Light with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  rgb_color_light::config_t light_config;
  light_config.on_off.on_off = initialState;
  light_config.on_off.lighting.start_up_on_off = nullptr;
  onOffState = initialState;

  light_config.level_control.current_level = _colorHSV.v;
  light_config.level_control.lighting.start_up_current_level = nullptr;

  light_config.color_control.color_mode = (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation;
  light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation;
  light_config.color_control.hue_saturation.current_hue = _colorHSV.h;
  light_config.color_control.hue_saturation.current_saturation = _colorHSV.s;
  colorHSV = {_colorHSV.h, _colorHSV.s, _colorHSV.v};

  // endpoint handles can be used to add/modify clusters.
  endpoint_t *endpoint = rgb_color_light::create(node::get(), &light_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create RGB Color light endpoint");
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));
  log_i("RGB Color Light created with endpoint_id %d", getEndPointId());

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

  endpoint_t *endpoint = endpoint::get(node::get(), endpoint_id);
  cluster_t *cluster = cluster::get(endpoint, ColorControl::Id);
  // update hue
  esp_matter::attribute_t *attribute = attribute::get(cluster, ColorControl::Attributes::CurrentHue::Id);
  esp_matter_attr_val_t val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);
  if (val.val.u8 != colorHSV.h) {
    val.val.u8 = colorHSV.h;
    attribute::update(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id, &val);
  }
  // update saturation
  attribute = attribute::get(cluster, ColorControl::Attributes::CurrentSaturation::Id);
  val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);
  if (val.val.u8 != colorHSV.s) {
    val.val.u8 = colorHSV.s;
    attribute::update(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id, &val);
  }
  // update value (brightness)
  cluster = cluster::get(endpoint, LevelControl::Id);
  attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);
  val = esp_matter_invalid(NULL);
  attribute::get_val(attribute, &val);
  if (val.val.u8 != colorHSV.v) {
    val.val.u8 = colorHSV.v;
    attribute::update(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, &val);
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
