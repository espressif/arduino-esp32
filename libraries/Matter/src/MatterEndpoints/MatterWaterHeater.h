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

// Matter Water Heater endpoint (device type 0x050F) - Water Heater Management, Water Heater Mode and Thermostat clusters.
class MatterWaterHeater : public MatterEndPoint {
public:
  enum WaterHeaterMode_t {
    WATER_HEATER_MODE_OFF = 0,
    WATER_HEATER_MODE_MANUAL = 1,
    WATER_HEATER_MODE_ECO = 2,
  };

  enum SystemMode_t {
    SYSTEM_MODE_OFF = 0,
    SYSTEM_MODE_HEAT = 4,
  };

  // Bitmap - a heater may report more than one type, and HeatDemand reuses the same bit values.
  enum HeaterType_t {
    IMMERSION_ELEMENT_1 = 0x01,
    IMMERSION_ELEMENT_2 = 0x02,
    HEAT_PUMP = 0x04,
    BOILER = 0x08,
    OTHER = 0x10,
  };

  enum BoostState_t {
    BOOST_INACTIVE = 0,
    BOOST_ACTIVE = 1,
  };

  MatterWaterHeater();
  ~MatterWaterHeater();

  bool begin();
  void end();

  // Thermostat / local temperature
  bool setLocalTemperature(float temperature);
  float getLocalTemperature();

  bool setLocalTemperatureRaw(int16_t temperature);
  int16_t getLocalTemperatureRaw();

  bool setHeatingSetpoint(float temperature);
  float getHeatingSetpoint();

  bool setHeatingSetpointRaw(int16_t temperature);
  int16_t getHeatingSetpointRaw();

  // Heating setpoint limits
  bool setAbsoluteMinimumHeatingSetpoint(float temperature);
  bool setMinimumHeatingSetpoint(float temperature);
  bool setAbsoluteMaximumHeatingSetpoint(float temperature);
  bool setMaximumHeatingSetpoint(float temperature);

  float getAbsoluteMinimumHeatingSetpoint();
  float getMinimumHeatingSetpoint();
  float getAbsoluteMaximumHeatingSetpoint();
  float getMaximumHeatingSetpoint();

  // Thermostat system mode
  bool setSystemMode(SystemMode_t mode);
  SystemMode_t getSystemMode();

  // Water Heater Management
  bool setHeaterTypes(uint8_t heaterTypes);
  uint8_t getHeaterTypes();

  // HeatDemand is a bitmap (same bit values as HeaterTypes) of the heat sources currently active.
  // It is kept in sync automatically by setSystemMode()/setBoostState(); call this only to override it.
  bool setHeatDemand(uint8_t heatDemand);
  uint8_t getHeatDemand();

  bool setTankVolume(uint16_t tankVolume);
  uint16_t getTankVolume();

  bool setTankPercentage(uint8_t tankPercentage);
  uint8_t getTankPercentage();

  bool setBoostState(BoostState_t state);
  BoostState_t getBoostState();

  // Water Heater Mode
  bool setWaterHeaterMode(WaterHeaterMode_t mode);
  WaterHeaterMode_t getWaterHeaterMode();

  // MatterEndPoint callback.
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
  uint8_t boostState = BOOST_INACTIVE;

  uint8_t waterHeaterMode = WATER_HEATER_MODE_MANUAL;

  // Recomputes HeatDemand from the current heaterTypes/systemMode/boostState and pushes it if changed.
  // Per Matter spec, HeatDemand reflects the heat sources currently active (Boost or normal heating).
  void syncHeatDemand();
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
