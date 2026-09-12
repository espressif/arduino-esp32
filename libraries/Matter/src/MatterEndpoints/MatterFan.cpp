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
#include <MatterEndpoints/MatterFan.h>
#include <app/util/attribute-storage-null-handling.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

static bool isPercentSettingNull(const esp_matter_attr_val_t *val) {
  if (val == nullptr) {
    return true;
  }
  if (val->type == ESP_MATTER_VAL_TYPE_NULLABLE_UINT8) {
    return chip::app::NumericAttributeTraits<uint8_t>::IsNullValue(val->val.u8);
  }
  return false;
}

MatterFan::FanMode_t MatterFan::resolveFanMode(FanMode_t mode) const {
  if (mode == FAN_MODE_ON) {
    return FAN_MODE_HIGH;
  }
  if (mode == FAN_MODE_SMART) {
    return (validFanModes & fanSeqModeAuto) ? FAN_MODE_AUTO : FAN_MODE_HIGH;
  }
  return mode;
}

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

  log_d(
    "Fan Attr update callback: endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %" PRIu32, endpoint_id, cluster_id, attribute_id, val->val.u32
  );

  if (endpoint_id == getEndPointId() && cluster_id == FanControl::Id) {
    switch (attribute_id) {
      case FanControl::Attributes::FanMode::Id: {
        FanMode_t newMode = resolveFanMode((FanMode_t)val->val.u8);
        log_v("FanControl Fan Mode changed to %s (%x)", getFanModeString(newMode), (uint8_t)newMode);
        if (_onChangeModeCB != NULL) {
          ret &= _onChangeModeCB(newMode);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(newMode, currentPercent);
        }
        if (ret == true) {
          currentFanMode = newMode;
        }
        break;
      }
      case FanControl::Attributes::PercentSetting::Id:
        // PercentSetting is nullable (null in Auto). Do not copy that val into PercentCurrent.
        if (isPercentSettingNull(val)) {
          log_v("FanControl PercentSetting is null (auto)");
          break;
        }
        if (val->val.u8 > MAX_SPEED) {
          log_e("FanControl PercentSetting %u is out of range", val->val.u8);
          return false;
        }
        log_v("FanControl PercentSetting changed to %u", val->val.u8);
        if (_onChangeSpeedCB != NULL) {
          ret &= _onChangeSpeedCB(val->val.u8);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(currentFanMode, val->val.u8);
        }
        if (ret == true) {
          currentPercent = val->val.u8;
          esp_matter_attr_val_t currentVal = esp_matter_uint8(currentPercent);
          setAttributeVal(FanControl::Id, FanControl::Attributes::PercentCurrent::Id, &currentVal);
        }
        break;
      case FanControl::Attributes::PercentCurrent::Id:
        if (val->val.u8 > MAX_SPEED) {
          log_e("FanControl PercentCurrent %u is out of range", val->val.u8);
          return false;
        }
        log_v("FanControl PercentCurrent changed to %u", val->val.u8);
        if (_onChangeSpeedCB != NULL) {
          ret &= _onChangeSpeedCB(val->val.u8);
        }
        if (_onChangeCB != NULL) {
          ret &= _onChangeCB(currentFanMode, val->val.u8);
        }
        if (ret == true) {
          currentPercent = val->val.u8;
        }
        break;
    }
  }

  return ret;
}

bool MatterFan::begin(uint8_t percent, FanMode_t fanMode, FanModeSequence_t fanModeSeq) {
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Fan with Endpoint Id %u device has already been created.", getEndPointId());
    return false;
  }

  if (percent > MAX_SPEED) {
    log_e("Invalid Fan speed percent %u (0-100).", percent);
    return false;
  }
  if (fanModeSeq > FAN_MODE_SEQ_OFF_HIGH) {
    log_e("Invalid Fan Mode Sequence.");
    return false;
  }

  validFanModes = fanModeSequence[fanModeSeq];
  fanMode = resolveFanMode(fanMode);
  if (!(validFanModes & (1 << fanMode))) {
    log_e("Invalid Fan Mode %s for the current Fan Mode Sequence.", getFanModeString(fanMode));
    return false;
  }

  // CHIP: Off zeros both percents; Auto nulls PercentSetting.
  if (fanMode == FAN_MODE_OFF) {
    percent = 0;
  }

  // endpoint handles can be used to add/modify clusters.
  fan::config_t fan_config;
  fan_config.fan_control.fan_mode = fanMode;
  fan_config.fan_control.percent_current = percent;
  if (fanMode == FAN_MODE_AUTO) {
    fan_config.fan_control.percent_setting = nullable<uint8_t>();
  } else {
    fan_config.fan_control.percent_setting = percent;
  }
  fan_config.fan_control.fan_mode_sequence = fanModeSeq;

  endpoint_t *endpoint = fan::create(node::get(), &fan_config, ENDPOINT_FLAG_NONE, (void *)this);

  if (endpoint == nullptr) {
    log_e("Failed to create Fan endpoint");
    return false;
  }

  currentFanMode = fanMode;
  currentPercent = percent;

  setEndPointId(endpoint::get_id(endpoint));

  log_i("Fan created with endpoint_id %u", getEndPointId());

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
  newMode = resolveFanMode(newMode);

  // avoid processing if there was no change
  if (currentFanMode == newMode) {
    return true;
  }

  // check if the remapped mode is valid for the sequence used in begin()
  if (!(validFanModes & (1 << newMode))) {
    log_e("Invalid Fan Mode %s for the current Fan Mode Sequence.", getFanModeString(newMode));
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
  if (!applyModePercentRules(currentFanMode, performUpdate)) {
    return false;
  }
  log_v("Fan Mode %s to %s ==> onOffState[%s]", performUpdate ? "updated" : "set", getFanModeString(currentFanMode), getOnOff() ? "ON" : "OFF");
  return true;
}

bool MatterFan::applyModePercentRules(FanMode_t mode, bool performUpdate) {
  if (mode == FAN_MODE_OFF) {
    esp_matter_attr_val_t settingVal = esp_matter_nullable_uint8(0);
    bool ret = performUpdate ? updateAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &settingVal)
                             : setAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &settingVal);
    if (!ret) {
      log_e("Failed to %s Fan PercentSetting Attribute.", performUpdate ? "update" : "set");
      return false;
    }
    esp_matter_attr_val_t currentVal = esp_matter_uint8(0);
    if (!setAttributeVal(FanControl::Id, FanControl::Attributes::PercentCurrent::Id, &currentVal)) {
      log_e("Failed to set Fan PercentCurrent Attribute.");
      return false;
    }
    currentPercent = 0;
    return true;
  }

  if (mode == FAN_MODE_AUTO) {
    // Null PercentSetting; PercentCurrent stays the actual speed.
    esp_matter_attr_val_t settingVal = esp_matter_nullable_uint8(nullable<uint8_t>());
    bool ret = performUpdate ? updateAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &settingVal)
                             : setAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &settingVal);
    if (!ret) {
      log_e("Failed to %s Fan PercentSetting Attribute.", performUpdate ? "update" : "set");
      return false;
    }
  }
  return true;
}

// this function will change the Fan Speed by calling the user application callback
// it is up to the application to decide to turn on, off or change the speed of the fan
bool MatterFan::setSpeedPercent(uint8_t newPercent, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }
  if (newPercent > MAX_SPEED) {
    log_e("Invalid Fan speed percent %u (0-100).", newPercent);
    return false;
  }
  // avoid processing if there was no change
  if (currentPercent == newPercent) {
    return true;
  }

  esp_matter_attr_val_t settingVal = esp_matter_nullable_uint8(newPercent);
  bool ret;
  if (performUpdate) {
    ret = updateAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &settingVal);
  } else {
    ret = setAttributeVal(FanControl::Id, FanControl::Attributes::PercentSetting::Id, &settingVal);
  }
  if (!ret) {
    log_e("Failed to %s Fan PercentSetting Attribute.", performUpdate ? "update" : "set");
    return false;
  }

  // PercentCurrent is not nullable; keep it in sync with the requested speed.
  esp_matter_attr_val_t currentVal = esp_matter_uint8(newPercent);
  if (!setAttributeVal(FanControl::Id, FanControl::Attributes::PercentCurrent::Id, &currentVal)) {
    log_e("Failed to set Fan PercentCurrent Attribute.");
    return false;
  }
  currentPercent = newPercent;
  log_v("Fan Speed %s to %u ==> onOffState[%s]", performUpdate ? "updated" : "set", currentPercent, getOnOff() ? "ON" : "OFF");
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

  FanMode_t newMode = newState ? FAN_MODE_ON : FAN_MODE_OFF;
  if (!setMode(newMode, performUpdate)) {
    return false;
  }
  log_v(
    "Fan State %s to %s :: Mode[%s]|Speed[%u]", performUpdate ? "updated" : "set", getOnOff() ? "ON" : "OFF", getFanModeString(currentFanMode), currentPercent
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
