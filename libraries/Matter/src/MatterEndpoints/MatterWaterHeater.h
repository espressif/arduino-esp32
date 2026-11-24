// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
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
#include <app-common/zap-generated/cluster-enums.h>

namespace chip {
namespace System {
class Layer;
}  // namespace System
}  // namespace chip

/**
 * @brief Matter water heater endpoint (device type 0x050F).
 *
 * Combines Water Heater Management, Water Heater Mode, and a heating-only
 * Thermostat cluster. Supports system mode, Boost, tank attributes, and
 * automatic HeatDemand sync. Compatible with Home Assistant, Apple Home,
 * Amazon Alexa, and Google Home.
 */
class MatterWaterHeater : public MatterEndPoint {
public:
  /**
   * @brief Water Heater Mode (Off / Manual / Eco).
   */
  enum WaterHeaterMode_t {
    WATER_HEATER_MODE_OFF = 0,     ///< Off.
    WATER_HEATER_MODE_MANUAL = 1,  ///< Manual.
    WATER_HEATER_MODE_ECO = 2,     ///< Eco.
  };

  /**
   * @brief Thermostat SystemMode (heating-only). Controller writes of any other mode are rejected.
   */
  enum SystemMode_t {
    SYSTEM_MODE_OFF = 0,   ///< Off.
    SYSTEM_MODE_HEAT = 4,  ///< Heat.
  };

  /**
   * @brief Heater type bitmap. HeatDemand reuses the same bit values for sources currently drawing power.
   */
  enum HeaterType_t {
    IMMERSION_ELEMENT_1 = 0x01,  ///< Immersion element 1.
    IMMERSION_ELEMENT_2 = 0x02,  ///< Immersion element 2.
    HEAT_PUMP = 0x04,            ///< Heat pump.
    BOILER = 0x08,               ///< Boiler.
    OTHER = 0x10,                ///< Other heat source.
  };

  /**
   * @brief Boost session state.
   */
  enum BoostState_t {
    BOOST_INACTIVE = 0,  ///< Boost inactive.
    BOOST_ACTIVE = 1,    ///< Boost active.
  };

  /**
   * @brief Create a new Matter water heater endpoint.
   */
  MatterWaterHeater();
  /**
   * @brief Destroy the water heater endpoint object.
   */
  ~MatterWaterHeater();

  /**
   * @brief Create the Water Heater device type and install the WHM delegate (Boost / CancelBoost).
   *
   * Provisions EnergyManagement and TankPercent so TankVolume, EstimatedHeatRequired,
   * and TankPercentage exist. Call before @c Matter.begin(). A call after the stack
   * has started fails. On failure after partial create, the endpoint is destroyed
   * so begin() may be retried.
   *
   * @return true if successful, false otherwise.
   */
  bool begin();
  /**
   * @brief Stop sketch-side updates and tear down delegates.
   *
   * If a Boost session is active, restores any temporary heating setpoint and
   * clears Boost (no BoostEnded event). Before Matter.begin() destroys the
   * endpoint so begin() can run again. After Matter.begin() the Matter endpoint
   * stays until reboot; a later begin() fails.
   */
  void end();

  /**
   * @brief Report local water temperature in degrees Celsius.
   * @param temperature Temperature in Celsius.
   * @return true if successful, false otherwise.
   */
  bool setLocalTemperature(float temperature);
  /**
   * @brief Get local water temperature in degrees Celsius.
   * @return Temperature in Celsius.
   */
  float getLocalTemperature();

  /**
   * @brief Report local water temperature in Thermostat cluster units (hundredths of a degree Celsius).
   * @param temperature Temperature in hundredths of a degree Celsius.
   * @return true if successful, false otherwise.
   */
  bool setLocalTemperatureRaw(int16_t temperature);
  /**
   * @brief Get local water temperature in hundredths of a degree Celsius.
   * @return Temperature in hundredths of a degree Celsius.
   */
  int16_t getLocalTemperatureRaw();

  /**
   * @brief Set the occupied heating setpoint in degrees Celsius.
   * @param temperature Setpoint in Celsius.
   * @return true if successful, false otherwise.
   */
  bool setHeatingSetpoint(float temperature);
  /**
   * @brief Get the occupied heating setpoint in degrees Celsius.
   * @return Setpoint in Celsius.
   */
  float getHeatingSetpoint();

  /**
   * @brief Set the occupied heating setpoint in hundredths of a degree Celsius.
   * @param temperature Setpoint in hundredths of a degree Celsius.
   * @return true if successful, false otherwise.
   */
  bool setHeatingSetpointRaw(int16_t temperature);
  /**
   * @brief Get the occupied heating setpoint in hundredths of a degree Celsius.
   * @return Setpoint in hundredths of a degree Celsius.
   */
  int16_t getHeatingSetpointRaw();

  /**
   * @brief Set AbsMinHeatSetpointLimit in Celsius. Call absolute setters first when narrowing the product range.
   * @param temperature Absolute minimum in Celsius (20–85).
   * @return true if successful, false otherwise.
   */
  bool setAbsoluteMinimumHeatingSetpoint(float temperature);
  /**
   * @brief Set MinHeatSetpointLimit in Celsius. Must stay inside AbsMin / AbsMax.
   * @param temperature Minimum setpoint in Celsius.
   * @return true if successful, false otherwise.
   */
  bool setMinimumHeatingSetpoint(float temperature);
  /**
   * @brief Set AbsMaxHeatSetpointLimit in Celsius.
   * @param temperature Absolute maximum in Celsius (20–85).
   * @return true if successful, false otherwise.
   */
  bool setAbsoluteMaximumHeatingSetpoint(float temperature);
  /**
   * @brief Set MaxHeatSetpointLimit in Celsius. Must stay inside AbsMin / AbsMax.
   * @param temperature Maximum setpoint in Celsius.
   * @return true if successful, false otherwise.
   */
  bool setMaximumHeatingSetpoint(float temperature);

  /**
   * @brief Get AbsMinHeatSetpointLimit in Celsius.
   * @return Absolute minimum setpoint (°C).
   */
  float getAbsoluteMinimumHeatingSetpoint();
  /**
   * @brief Get MinHeatSetpointLimit in Celsius.
   * @return Minimum setpoint (°C).
   */
  float getMinimumHeatingSetpoint();
  /**
   * @brief Get AbsMaxHeatSetpointLimit in Celsius.
   * @return Absolute maximum setpoint (°C).
   */
  float getAbsoluteMaximumHeatingSetpoint();
  /**
   * @brief Get MaxHeatSetpointLimit in Celsius.
   * @return Maximum setpoint (°C).
   */
  float getMaximumHeatingSetpoint();

  /**
   * @brief Set Thermostat SystemMode (Off or Heat). Updates HeatDemand.
   * @param mode SYSTEM_MODE_OFF or SYSTEM_MODE_HEAT.
   * @return true if successful, false otherwise.
   */
  bool setSystemMode(SystemMode_t mode);
  /**
   * @brief Get Thermostat SystemMode.
   * @return Current system mode.
   */
  SystemMode_t getSystemMode();

  /**
   * @brief Set HeaterTypes (product capability: which heat sources exist). Syncs HeatDemand.
   * @param heaterTypes Bitmap of @ref HeaterType_t bits.
   * @return true if successful, false otherwise.
   */
  bool setHeaterTypes(uint8_t heaterTypes);
  /**
   * @brief Get HeaterTypes bitmap.
   * @return Heater type bits.
   */
  uint8_t getHeaterTypes();

  /**
   * @brief Override HeatDemand (sources currently drawing power). Bits outside HeaterTypes are rejected.
   *
   * HeatDemand is kept in sync automatically by setSystemMode(), setBoostState(),
   * setWaterHeaterMode() and setHeaterTypes(); call this only to override it.
   *
   * @param heatDemand Bitmap of @ref HeaterType_t bits (subset of HeaterTypes, or 0).
   * @return true if successful, false otherwise.
   */
  bool setHeatDemand(uint8_t heatDemand);
  /**
   * @brief Get HeatDemand bitmap (sources currently drawing power).
   * @return Heat demand bits.
   */
  uint8_t getHeatDemand();

  /**
   * @brief Set tank capacity in liters (code-driven WHM Delegate). Do not use updateAttributeVal().
   * @param tankVolume Tank volume in liters.
   * @return true if successful, false otherwise.
   */
  bool setTankVolume(uint16_t tankVolume);
  /**
   * @brief Get tank capacity in liters.
   * @return Tank volume in liters.
   */
  uint16_t getTankVolume();

  /**
   * @brief Set tank percentage (0–100). Values above 100 are rejected.
   * @param tankPercentage Tank fill percentage.
   * @return true if successful, false otherwise.
   */
  bool setTankPercentage(uint8_t tankPercentage);
  /**
   * @brief Get tank percentage (0–100).
   * @return Tank fill percentage.
   */
  uint8_t getTankPercentage();

  /**
   * @brief Set EstimatedHeatRequired in milliWatt-hours (remaining energy to the heating goal).
   *
   * Controllers often display kWh (1 kWh = 1_000_000 mWh). Values below 0 are rejected.
   *
   * @param energy_mWh Remaining energy in milliWatt-hours.
   * @return true if successful, false otherwise.
   */
  bool setEstimatedHeatRequired(int64_t energy_mWh);
  /**
   * @brief Get EstimatedHeatRequired in milliWatt-hours.
   * @return Remaining energy in milliWatt-hours.
   */
  int64_t getEstimatedHeatRequired();

  /**
   * @brief Set BoostState. INACTIVE cancels an active Boost session (timer, temporary setpoint, BoostEnded).
   *
   * Controller Boost/CancelBoost commands use the WHM delegate and then this same cache.
   * EmergencyBoost is ignored. TargetReheat is rejected.
   *
   * @param state BOOST_ACTIVE or BOOST_INACTIVE.
   * @return true if successful, false otherwise.
   */
  bool setBoostState(BoostState_t state);
  /**
   * @brief Get BoostState.
   * @return Current boost state.
   */
  BoostState_t getBoostState();

  /**
   * @brief Set Water Heater Mode (CHIP mode-base CurrentMode: Off / Manual / Eco).
   *
   * Mode Off stops normal heating demand; an active Boost still demands heat.
   * Before Matter.begin() the value is cached and applied when the mode server starts.
   *
   * @param mode Water heater mode.
   * @return true if successful, false otherwise.
   */
  bool setWaterHeaterMode(WaterHeaterMode_t mode);
  /**
   * @brief Get Water Heater Mode.
   * @return Current water heater mode.
   */
  WaterHeaterMode_t getWaterHeaterMode();

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
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) override;

private:
  bool initialized = false;

  int16_t localTemperature = 2000;
  int16_t heatingSetpoint = 4800;

  int16_t absoluteMinimumHeatingSetpoint = 2000;
  int16_t minimumHeatingSetpoint = 2000;
  int16_t absoluteMaximumHeatingSetpoint = 8500;
  int16_t maximumHeatingSetpoint = 8500;

  uint8_t systemMode = SYSTEM_MODE_HEAT;

  uint8_t heaterTypes = IMMERSION_ELEMENT_1;
  uint8_t heatDemand = 0;
  uint16_t tankVolume = 100;
  uint8_t tankPercentage = 100;
  int64_t estimatedHeatRequired = 0;
  uint8_t boostState = BOOST_INACTIVE;

  uint8_t waterHeaterMode = WATER_HEATER_MODE_MANUAL;
  // Set when the sketch calls setWaterHeaterMode() before the mode server exists, so Init() can apply it.
  bool waterHeaterModeSetByApp = false;

  // ModeBase::Delegate for Water Heater Mode. Defined in the .cpp. Lives for the endpoint lifetime
  // after the stack starts, because ModeBase::Instance keeps this pointer.
  class ModeDelegate;
  ModeDelegate *modeDelegate = nullptr;

  // CHIP WaterHeaterManagement::Delegate. Required so Boost/CancelBoost register a
  // WaterHeaterManagement::Instance and controllers read WHM attrs from this cache.
  class ManagementDelegate;
  ManagementDelegate *managementDelegate = nullptr;

  bool boostTimerArmed = false;
  bool boostOneShot = false;
  bool boostHasTemporarySetpoint = false;
  int16_t boostSavedHeatingSetpoint = 0;
  bool boostHasTargetPercentage = false;
  uint8_t boostTargetPercentage = 0;
  bool boostFinishing = false;
  // Set while HandleBoost writes the temporary setpoint, before the new session is armed.
  bool applyingBoostSetpoint = false;

  enum class DeferredAction : uint8_t {
    RestoreSetpoint,
    FinishOneShot,
  };

  // Attribute write, cache update, and one-shot check. The caller holds the CHIP lock after Matter.begin().
  bool setHeatingSetpointRawBody(int16_t temperature);

  static void boostTimerCallback(chip::System::Layer *layer, void *appState);
  // Returns false if the timer could not be armed; the caller must roll back the boost.
  bool armBoostTimer(uint32_t durationSeconds);
  void cancelBoostTimer();
  void finishBoost(bool emitEndedEvent);
  void finishBoostBody(bool emitEndedEvent);
  // Puts the pre-Boost setpoint back. On failure the saved target and flag are kept.
  bool restoreBoostSetpoint();
  // Retries restoreBoostSetpoint() once Boost is inactive and the saved target is inside Min/Max.
  void retryBoostSetpointRestore();
  void scheduleBoostSetpointRestore();
  // Posts action to the Matter task. The lambda uses the leaked WHM delegate and returns if it was detached.
  bool postToAttachedDelegate(DeferredAction action);
  // After OccupiedHeatingSetpoint commits, ends a one-shot Boost that has already reached its target.
  void scheduleOneShotCompletion();
  // Ends a one-shot after a sketch setpoint write. Skipped during Boost command setup and restore.
  void noteHeatingSetpointCommitted();
  bool heatDemandAllowed(uint8_t value) const;
  void detachDelegates();
  void maybeFinishOneShotBoost();
  bool applyBoostState(BoostState_t state);

  // Recomputes HeatDemand from the current heaterTypes/systemMode/boostState/waterHeaterMode and pushes it if changed.
  // Boost demands the heater sources regardless of mode; otherwise Heat mode (and Mode not Off) demands them.
  void syncHeatDemand();
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
