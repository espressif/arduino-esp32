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
#include <MatterEndpoints/MatterFan.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

// string helper for the FAN MODE
const char *MatterFan::fanModeString[7] = {"OFF", "LOW", "MEDIUM", "HIGH", "ON", "AUTO", "SMART"};
// bitmap for valid Fan Modes based on order defined in Zap Generated Cluster Enums
const uint8_t MatterFan::fanModeSequence[6] = {fanSeqModeOffLowMedHigh,  fanSeqModeOffLowHigh,  fanSeqModeOffLowMedHighAuto,
                                               fanSeqModeOffLowHighAuto, fanSeqModeOffHighAuto, fanSeqModeOffHigh};

// Constructor and Method Definitions
MatterFan::MatterFan() {}

MatterFan::~MatterFan() {
  end();
}

bool MatterFan::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  bool ret = true;

  if (!started) {
    log_e("Matter Fan device has not begun.");
    return false;
  }

  log_d("Fan Attr update callback: endpoint: %u, cluster: %u, attribute: %u, val: %u", endpoint_id, cluster_id, attribute_id, val->val.u32);

  if (endpoint_id == getEndPointId() && cluster_id == FanControl::Id) {
    switch (attribute_id) {
      case FanControl::Attributes::FanMode::Id:
        log_v("FanControl Fan Mode changed to %s (%x)", val->val.u8 < 7 ? fanModeString[val->val.u8] : "Unknown", val->val.u8);
        if (_onChangeModeCB != NULL) {
          ret &= _onChangeModeCB((FanMode_t)val->val.u8);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB((FanMode_t)val->val.u8, currentPercent);
        }
        if (ret == true) {
          currentFanMode = (FanMode_t)val->val.u8;
        }
        break;
      case FanControl::Attributes::PercentSetting::Id:
      case FanControl::Attributes::PercentCurrent::Id:
        log_v("FanControl Percent %s changed to %d", attribute_id == FanControl::Attributes::PercentSetting::Id ? "SETTING" : "CURRENT", val->val.u8);
        if (_onChangeSpeedCB != NULL) {
          ret &= _onChangeSpeedCB(val->val.u8);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(currentFanMode, val->val.u8);
        }
        if (ret == true) {
          // change setting speed percent
          currentPercent = val->val.u8;
          setAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, val);
          setAttributeVal(FanControl::Id, FanControl::Attributes::PercentCurrent::Id, val);
        }
        break;
    }
  }

  return ret;
}

bool MatterFan::begin(uint8_t percent, FanMode_t fanMode, FanModeSequence_t fanModeSeq) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Fan with Endpoint Id %d device has already been created.", getEndPointId());
    return false;
  }

  // endpoint handles can be used to add/modify clusters.
  fan::config_t fan_config;
  fan_config.fan_control.fan_mode = fanMode;
  fan_config.fan_control.percent_current = percent;
  fan_config.fan_control.percent_setting = percent;
  fan_config.fan_control.fan_mode_sequence = fanModeSeq;
  validFanModes = fanModeSequence[fanModeSeq];

  endpoint_t *endpoint = fan::create(node::get(), &fan_config, ENDPOINT_FLAG_NONE, (void *)this);

  if (endpoint == nullptr) {
    log_e("Failed to create Fan endpoint");
    return false;
  }

  currentFanMode = fanMode;
  currentPercent = percent;

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Fan created with endpoint_id %d", getEndPointId());
  started = true;
  return true;
}

void MatterFan::end() {
  started = false;
}

bool MatterFan::setMode(FanMode_t newMode, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }
  // avoid processing if there was no change
  if (currentFanMode == newMode) {
    return true;
  }

  // check if the mode is valid based on the sequence used in its creation
  if (!(validFanModes & (1 << newMode))) {
    log_e("Invalid Fan Mode %s for the current Fan Mode Sequence.", fanModeString[newMode]);
    return false;
  }

  esp_matter_attr_val_t modeVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(FanControl::Id, FanControl::Attributes::FanMode::Id, &modeVal)) {
    log_e("Failed to get Fan Mode Attribute.");
    return false;
  }
  if (modeVal.val.u8 != (uint8_t)newMode) {
    modeVal.val.u8 = (uint8_t)newMode;
    bool ret;
    if (performUpdate) {
      ret = updateAttributeVal(FanControl::Id, FanControl::Attributes::FanMode::Id, &modeVal);
    } else {
      ret = setAttributeVal(FanControl::Id, FanControl::Attributes::FanMode::Id, &modeVal);
    }
    if (!ret) {
      log_e("Failed to %s Fan Mode Attribute.", performUpdate ? "update" : "set");
      return false;
    }
  }
  currentFanMode = newMode;
  log_v("Fan Mode %s to %s ==> onOffState[%s]", performUpdate ? "updated" : "set", fanModeString[currentFanMode], getOnOff() ? "ON" : "OFF");
  return true;
}

// this function will change the Fan Speed by calling the user application callback
// it is up to the application to decide to turn on, off or change the speed of the fan
bool MatterFan::setSpeedPercent(uint8_t newPercent, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }
  // avoid processing if there was no change
  if (currentPercent == newPercent) {
    return true;
  }

  esp_matter_attr_val_t speedVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &speedVal)) {
    log_e("Failed to get Fan Speed Percent Attribute.");
    return false;
  }
  if (speedVal.val.u8 != newPercent) {
    speedVal.val.u8 = newPercent;
    bool ret;
    if (performUpdate) {
      ret = updateAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &speedVal);
    } else {
      ret = setAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &speedVal);
      ret = setAttributeVal(FanControl::Id, FanControl::Attributes::PercentCurrent::Id, &speedVal);
    }
    if (!ret) {
      log_e("Failed to %s Fan Speed Percent Attribute.", performUpdate ? "update" : "set");
      return false;
    }
  }
  currentPercent = newPercent;
  log_v("Fan Speed %s to %d ==> onOffState[%s]", performUpdate ? "updated" : "set", currentPercent, getOnOff() ? "ON" : "OFF");
  return true;
}

bool MatterFan::setOnOff(bool newState, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }
  // avoid processing if there was no change
  if (getOnOff() == newState) {
    return true;
  }

  esp_matter_attr_val_t modeVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(FanControl::Id, FanControl::Attributes::FanMode::Id, &modeVal)) {
    log_e("Failed to get Fan Mode Attribute.");
    return false;
  }
  if (modeVal.val.u8 != (uint8_t)newState) {
    FanMode_t newMode = newState ? FAN_MODE_ON : FAN_MODE_OFF;
    if (!setMode(newMode, performUpdate)) {
      return false;
    }
  }
  log_v(
    "Fan State %s to %s :: Mode[%s]|Speed[%d]", performUpdate ? "updated" : "set", getOnOff() ? "ON" : "OFF", fanModeString[currentFanMode], currentPercent
  );
  return true;
}

bool MatterFan::getOnOff() {
  return currentFanMode == FAN_MODE_OFF ? false : true;
}

bool MatterFan::toggle(bool performUpdate) {
  if (getOnOff() == true) {
    return setOnOff(false, performUpdate);
  } else {
    return setOnOff(true, performUpdate);
  }
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
