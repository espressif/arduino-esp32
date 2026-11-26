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
#include <app-common/zap-generated/cluster-objects.h>

using namespace chip::app::Clusters::FanControl;

/**
 * @brief Matter fan endpoint with on/off, speed (0–100%), and mode control.
 *
 * @c FAN_MODE_ON and @c FAN_MODE_SMART are remapped, not stored. Compatible with
 * Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterFan : public MatterEndPoint {
public:
  /**
   * @brief Maximum speed value (100%).
   */
  static const uint8_t MAX_SPEED = 100;
  /**
   * @brief Minimum speed value (1%).
   */
  static const uint8_t MIN_SPEED = 1;
  /**
   * @brief Speed value when the fan is off (0%).
   */
  static const uint8_t OFF_SPEED = 0;

  /**
   * @brief Fan mode. Matter FanModeSequence never stores On or Smart.
   *
   * @c FAN_MODE_ON is stored as HIGH. @c FAN_MODE_SMART is stored as AUTO if the
   * sequence includes Auto, otherwise HIGH. CHIP remaps those writes the same way.
   */
  enum FanMode_t {
    FAN_MODE_OFF = (uint8_t)FanModeEnum::kOff,       ///< Fan is off.
    FAN_MODE_LOW = (uint8_t)FanModeEnum::kLow,       ///< Low speed.
    FAN_MODE_MEDIUM = (uint8_t)FanModeEnum::kMedium, ///< Medium speed.
    FAN_MODE_HIGH = (uint8_t)FanModeEnum::kHigh,     ///< High speed.
    FAN_MODE_ON = (uint8_t)FanModeEnum::kOn,         ///< Alias: stored as FAN_MODE_HIGH.
    FAN_MODE_AUTO = (uint8_t)FanModeEnum::kAuto,     ///< Auto mode (only valid in an Auto sequence).
    FAN_MODE_SMART = (uint8_t)FanModeEnum::kSmart    ///< Alias: stored as AUTO if the sequence includes Auto, otherwise HIGH.
  };

  /**
   * @brief Fan mode sequence advertised at @ref begin. Sequences are Off plus High, and optionally Low / Medium / Auto.
   */
  enum FanModeSequence_t {
    FAN_MODE_SEQ_OFF_LOW_MED_HIGH = (uint8_t)FanModeSequenceEnum::kOffLowMedHigh,           ///< OFF, LOW, MEDIUM, HIGH.
    FAN_MODE_SEQ_OFF_LOW_HIGH = (uint8_t)FanModeSequenceEnum::kOffLowHigh,                   ///< OFF, LOW, HIGH.
    FAN_MODE_SEQ_OFF_LOW_MED_HIGH_AUTO = (uint8_t)FanModeSequenceEnum::kOffLowMedHighAuto,   ///< OFF, LOW, MEDIUM, HIGH, AUTO.
    FAN_MODE_SEQ_OFF_LOW_HIGH_AUTO = (uint8_t)FanModeSequenceEnum::kOffLowHighAuto,          ///< OFF, LOW, HIGH, AUTO.
    FAN_MODE_SEQ_OFF_HIGH_AUTO = (uint8_t)FanModeSequenceEnum::kOffHighAuto,                 ///< OFF, HIGH, AUTO.
    FAN_MODE_SEQ_OFF_HIGH = (uint8_t)FanModeSequenceEnum::kOffHigh                           ///< OFF, HIGH.
  };

  /**
   * @brief Create a new Matter fan endpoint.
   */
  MatterFan();
  /**
   * @brief Destroy the fan endpoint object.
   */
  ~MatterFan();
  /**
   * @brief Initialize the Matter fan endpoint.
   * @param percent Initial speed percentage (0–100, default 0). Forced to 0 when fanMode is OFF.
   *                In Auto, this is PercentCurrent only; PercentSetting is null.
   * @param fanMode Initial fan mode (default OFF).
   * @param fanModeSeq Fan mode sequence (default OFF_HIGH).
   * @return true if successful, false otherwise.
   */
  virtual bool begin(uint8_t percent = 0, FanMode_t fanMode = FAN_MODE_OFF, FanModeSequence_t fanModeSeq = FAN_MODE_SEQ_OFF_HIGH);
  /**
   * @brief Stop processing Matter fan events.
   */
  void end();

  /**
   * @brief Get a friendly string for a fan mode value.
   * @param mode Fan mode (see @ref FanMode_t).
   * @return Mode name, or "Unknown" if out of range.
   */
  static const char *getFanModeString(uint8_t mode) {
    if (mode >= (sizeof(fanModeString) / sizeof(fanModeString[0]))) {
      return "Unknown";
    }
    return fanModeString[mode];
  }

  /**
   * @brief Set the on/off state of the fan.
   * @param newState New state (true = on, false = off).
   * @param performUpdate Perform update after setting (default true).
   * @return true if successful, false otherwise.
   */
  bool setOnOff(bool newState, bool performUpdate = true);
  /**
   * @brief Get the current on/off state.
   * @return true if the fan is on.
   */
  bool getOnOff();
  /**
   * @brief Toggle the on/off state.
   * @param performUpdate Perform update after setting (default true).
   * @return true if successful, false otherwise.
   */
  bool toggle(bool performUpdate = true);

  /**
   * @brief Set the fan speed percentage.
   *
   * Writes nullable PercentSetting and non-nullable PercentCurrent separately.
   * In FAN_MODE_AUTO this updates PercentCurrent only so PercentSetting stays null.
   * After Matter.begin(), a non-zero setting while Off turns the cluster on;
   * 0 turns it Off. PercentCurrent stays 0–100.
   *
   * @param newPercent Speed percentage (0–100).
   * @param performUpdate Perform update after setting (default true).
   * @return true if successful, false otherwise.
   */
  bool setSpeedPercent(uint8_t newPercent, bool performUpdate = true);
  /**
   * @brief Get the current speed percentage.
   * @return Speed percentage (0–100).
   */
  uint8_t getSpeedPercent() {
    return currentPercent;
  }

  /**
   * @brief Set the fan mode.
   *
   * FAN_MODE_ON and FAN_MODE_SMART are remapped as described under @ref FanMode_t.
   * OFF sets PercentSetting and PercentCurrent to 0; AUTO nulls PercentSetting
   * and leaves PercentCurrent as the actual speed.
   *
   * @param newMode Fan mode to set.
   * @param performUpdate Perform update after setting (default true).
   * @return true if successful, false if the remapped mode is not in the sequence passed to begin().
   */
  bool setMode(FanMode_t newMode, bool performUpdate = true);
  /**
   * @brief Get the current fan mode.
   * @return Current fan mode.
   */
  FanMode_t getMode() {
    return currentFanMode;
  }

  /**
   * @brief Apply the current Matter Fan internal state to the accessory.
   *
   * Set a user callback with @ref onChange to handle the physical fan motor.
   */
  void updateAccessory() {
    if (_onChangeCB != NULL) {
      _onChangeCB(currentFanMode, currentPercent);
    }
  }

  /**
   * @brief Conversion — current fan speed percentage.
   * @return Speed percentage (0–100).
   */
  operator uint8_t() {
    return getSpeedPercent();
  }

  /**
   * @brief Callback invoked when fan mode changes.
   * @param newMode New fan mode.
   * @return true if the change was handled successfully.
   */
  using EndPointModeCB = std::function<bool(FanMode_t)>;
  /**
   * @brief Register a callback for mode changes from the Matter controller.
   * @param onChangeCB Function to call when mode changes.
   */
  void onChangeMode(EndPointModeCB onChangeCB) {
    _onChangeModeCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when fan speed percentage changes.
   * @param newPercent New speed percentage (0–100).
   * @return true if the change was handled successfully.
   */
  using EndPointSpeedCB = std::function<bool(uint8_t)>;
  /**
   * @brief Register a callback for speed changes from the Matter controller.
   * @param onChangeCB Function to call when speed changes.
   */
  void onChangeSpeedPercent(EndPointSpeedCB onChangeCB) {
    _onChangeSpeedCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when any fan parameter changes.
   * @param newMode New fan mode.
   * @param newPercent New speed percentage (0–100).
   * @return true if the change was handled successfully.
   */
  using EndPointCB = std::function<bool(FanMode_t, uint8_t)>;
  /**
   * @brief Register a callback for any parameter change from the Matter controller.
   * @param onChangeCB Function to call when state changes.
   */
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  /**
   * @brief Assignment — set the speed percentage.
   * @param speedPercent Speed percentage (0–100).
   */
  void operator=(uint8_t speedPercent) {
    setSpeedPercent(speedPercent);
  }

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
  static const uint8_t fanSeqModeAuto = 0x20;

  // Every Matter sequence includes Off and High.
  static const uint8_t fanSeqCommonModes = fanSeqModeOff | fanSeqModeHigh;

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

  FanMode_t resolveFanMode(FanMode_t mode) const;
  bool applyModePercentRules(FanMode_t mode, bool performUpdate);
  bool reportPercentCurrent(uint8_t percent);
  void onStackStarted() override;
};

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
