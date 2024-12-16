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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>
#include <app-common/zap-generated/cluster-objects.h>

using namespace chip::app::Clusters::FanControl;

// Matter Fan endpoint with On/Off, Mode and Speed control

class MatterFan : public MatterEndPoint {
public:
  // Fan feature constants
  static const uint8_t MAX_SPEED = 100;  // maximum High speed
  static const uint8_t MIN_SPEED = 1;    // minimum Low speed
  static const uint8_t OFF_SPEED = 0;    // speed set by Matter when FAN_MODE_OFF

  // Default Fan Modes: ON, SMART, HIGH and OFF

  // Other mode will depend on what is the configured Fan Mode Sequence
  enum FanMode_t {
    FAN_MODE_OFF = (uint8_t)FanModeEnum::kOff,
    FAN_MODE_LOW = (uint8_t)FanModeEnum::kLow,
    FAN_MODE_MEDIUM = (uint8_t)FanModeEnum::kMedium,
    FAN_MODE_HIGH = (uint8_t)FanModeEnum::kHigh,
    FAN_MODE_ON = (uint8_t)FanModeEnum::kOn,
    FAN_MODE_AUTO = (uint8_t)FanModeEnum::kAuto,
    FAN_MODE_SMART = (uint8_t)FanModeEnum::kSmart
  };

  // Menu will always have ON, OFF, HIGH and SMART.
  // AUTO will show up only when a AUTO SEQ is CONFIGURED
  // LOW and MEDIUM depend on the SEQ MODE configuration
  enum FanModeSequence_t {
    FAN_MODE_SEQ_OFF_LOW_MED_HIGH = (uint8_t)FanModeSequenceEnum::kOffLowMedHigh,
    FAN_MODE_SEQ_OFF_LOW_HIGH = (uint8_t)FanModeSequenceEnum::kOffLowHigh,
    FAN_MODE_SEQ_OFF_LOW_MED_HIGH_AUTO = (uint8_t)FanModeSequenceEnum::kOffLowMedHighAuto,
    FAN_MODE_SEQ_OFF_LOW_HIGH_AUTO = (uint8_t)FanModeSequenceEnum::kOffLowHighAuto,
    FAN_MODE_SEQ_OFF_HIGH_AUTO = (uint8_t)FanModeSequenceEnum::kOffHighAuto,
    FAN_MODE_SEQ_OFF_HIGH = (uint8_t)FanModeSequenceEnum::kOffHigh
  };

  MatterFan();
  ~MatterFan();
  virtual bool begin(uint8_t percent = 0, FanMode_t fanMode = FAN_MODE_OFF, FanModeSequence_t fanModeSeq = FAN_MODE_SEQ_OFF_HIGH);
  void end();  // this will just stop processing Matter events

  // returns a friendly string for the Fan Mode
  static const char *getFanModeString(uint8_t mode) {
    return fanModeString[mode];
  }

  // Fan Control of current On/Off state

  bool setOnOff(bool newState, bool performUpdate = true);  // sets Fan On/Off state
  bool getOnOff();                                          // returns current Fan state
  bool toggle(bool performUpdate = true);                   // toggle Fun On/Off state

  // Fan Control of current speed percent

  bool setSpeedPercent(uint8_t newPercent, bool performUpdate = true);  // returns true if successful
  uint8_t getSpeedPercent() {                                           // returns current Fan Speed Percent
    return currentPercent;
  }

  // Fan Control of current Fan Mode

  bool setMode(FanMode_t newMode, bool performUpdate = true);  // returns true if successful
  FanMode_t getMode() {                                        // returns current Fan Mode
    return currentFanMode;
  }
  // used to update the state of the Fan using the current Matter Fan internal state
  // It is necessary to set a user callback function using onChange() to handle the physical Fan motor state

  void updateAccessory() {
    if (_onChangeCB != NULL) {
      _onChangeCB(currentFanMode, currentPercent);
    }
  }

  // returns current Fan speed percent
  operator uint8_t() {
    return getSpeedPercent();
  }

  // User Callback for whenever the Fan Mode (state) is changed by the Matter Controller
  using EndPointModeCB = std::function<bool(FanMode_t)>;
  void onChangeMode(EndPointModeCB onChangeCB) {
    _onChangeModeCB = onChangeCB;
  }

  // User Callback for whenever the Fan Speed Percentage value [0..100] is changed by the Matter Controller
  using EndPointSpeedCB = std::function<bool(uint8_t)>;
  void onChangeSpeedPercent(EndPointSpeedCB onChangeCB) {
    _onChangeSpeedCB = onChangeCB;
  }

  // User Callback for whenever any parameter is changed by the Matter Controller
  using EndPointCB = std::function<bool(FanMode_t, uint8_t)>;
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  // sets Fan speed percent
  void operator=(uint8_t speedPercent) {
    setSpeedPercent(speedPercent);
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  uint8_t validFanModes = 0;  // bitmap for valid Fan Modes - index of fanModeSequence[]

  uint8_t currentPercent = 0;               // current speed percent
  FanMode_t currentFanMode = FAN_MODE_OFF;  // current Fan Mode
  EndPointModeCB _onChangeModeCB = NULL;
  EndPointSpeedCB _onChangeSpeedCB = NULL;
  EndPointCB _onChangeCB = NULL;

  // bitmap for Fan Sequence Modes (OFF, LOW, MEDIUM, HIGH, AUTO)
  static const uint8_t fanSeqModeOff = 0x01;
  static const uint8_t fanSeqModeLow = 0x02;
  static const uint8_t fanSeqModeMedium = 0x04;
  static const uint8_t fanSeqModeHigh = 0x08;
  static const uint8_t fanSeqModeOn = 0x10;
  static const uint8_t fanSeqModeAuto = 0x20;
  static const uint8_t fanSeqModeSmart = 0x40;

  // bitmap for common modes: ON, OFF, HIGH and SMART
  static const uint8_t fanSeqCommonModes = fanSeqModeOff | fanSeqModeOn | fanSeqModeHigh | fanSeqModeSmart;

  static const uint8_t fanSeqModeOffLowMedHigh = fanSeqCommonModes | fanSeqModeLow | fanSeqModeMedium;
  static const uint8_t fanSeqModeOffLowHigh = fanSeqCommonModes | fanSeqModeLow;
  static const uint8_t fanSeqModeOffLowMedHighAuto = fanSeqCommonModes | fanSeqModeLow | fanSeqModeMedium | fanSeqModeAuto;
  static const uint8_t fanSeqModeOffLowHighAuto = fanSeqCommonModes | fanSeqModeLow | fanSeqModeAuto;
  static const uint8_t fanSeqModeOffHighAuto = fanSeqCommonModes | fanSeqModeAuto;
  static const uint8_t fanSeqModeOffHigh = fanSeqCommonModes;

  // bitmap for valid Fan Modes based on order defined in Zap Generated Cluster Enums
  static const uint8_t fanModeSequence[6];

  // string helper for the FAN MODE
  static const char *fanModeString[7];
};

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
