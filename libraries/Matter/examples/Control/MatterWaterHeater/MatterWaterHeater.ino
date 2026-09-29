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

// Matter Manager
#include <Arduino.h>
#include <Matter.h>

// List of Matter Endpoints for this Node
// Water Heater Endpoint
MatterWaterHeater waterHeater;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

constexpr float COLD_WATER_TEMP = 20.0f;
constexpr float INITIAL_WATER_TEMP = 20.0f;
constexpr float TANK_VOLUME_LITERS = 100.0f;
// Eco holds the tank below the hub setpoint so the mode is visible on the hub and in the log.
constexpr float ECO_MAX_TEMP = 40.0f;

const char *waterHeaterModeName(MatterWaterHeater::WaterHeaterMode_t mode) {
  switch (mode) {
    case MatterWaterHeater::WATER_HEATER_MODE_OFF:    return "Off";
    case MatterWaterHeater::WATER_HEATER_MODE_MANUAL: return "Manual";
    case MatterWaterHeater::WATER_HEATER_MODE_ECO:    return "Eco";
    default:                                          return "?";
  }
}

void updateTankPercentage() {
  const float current = waterHeater.getLocalTemperature();
  const float target = waterHeater.getHeatingSetpoint();

  if (target <= COLD_WATER_TEMP) {
    waterHeater.setTankPercentage(0);
    return;
  }

  float percentage = ((current - COLD_WATER_TEMP) / (target - COLD_WATER_TEMP)) * 100.0f;
  percentage = constrain(percentage, 0.0f, 100.0f);
  waterHeater.setTankPercentage(static_cast<uint8_t>(percentage));
}

void logWaterHeater(const char *reason) {
  Serial.printf(
    "%s | Temp: %.1f C | Setpoint: %.1f C | Mode: %s | System: %s | Boost: %s | Tank: %u %% | Demand: 0x%02X\r\n", reason,
    waterHeater.getLocalTemperature(), waterHeater.getHeatingSetpoint(), waterHeaterModeName(waterHeater.getWaterHeaterMode()),
    waterHeater.getSystemMode() == MatterWaterHeater::SYSTEM_MODE_HEAT ? "Heat" : "Off",
    waterHeater.getBoostState() == MatterWaterHeater::BOOST_ACTIVE ? "on" : "off", waterHeater.getTankPercentage(),
    waterHeater.getHeatDemand()
  );
}

void setup() {
  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  if (!waterHeater.begin()) {
    Serial.println("Failed to create Matter Water Heater endpoint");
    while (true) {
      delay(1000);
    }
  }

  waterHeater.setHeaterTypes(MatterWaterHeater::IMMERSION_ELEMENT_1);
  waterHeater.setTankVolume(static_cast<uint16_t>(TANK_VOLUME_LITERS));
  waterHeater.setLocalTemperature(INITIAL_WATER_TEMP);
  waterHeater.setHeatingSetpoint(48.0f);
  // setSystemMode() also updates HeatDemand to reflect the heater types now enabled.
  waterHeater.setSystemMode(MatterWaterHeater::SYSTEM_MODE_HEAT);
  waterHeater.setWaterHeaterMode(MatterWaterHeater::WATER_HEATER_MODE_MANUAL);
  waterHeater.setTankPercentage(0);

  matterSetExampleIdentity("WaterHeater");
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  // Hub writes update the endpoint cache immediately. Print when mode, system, boost, or setpoint changes
  // so the sketch shows those features without waiting for the next heating step.
  static bool haveSnapshot = false;
  static MatterWaterHeater::WaterHeaterMode_t prevMode = MatterWaterHeater::WATER_HEATER_MODE_MANUAL;
  static MatterWaterHeater::SystemMode_t prevSystem = MatterWaterHeater::SYSTEM_MODE_HEAT;
  static MatterWaterHeater::BoostState_t prevBoost = MatterWaterHeater::BOOST_INACTIVE;
  static float prevSetpoint = 0.0f;

  const MatterWaterHeater::WaterHeaterMode_t mode = waterHeater.getWaterHeaterMode();
  const MatterWaterHeater::SystemMode_t system = waterHeater.getSystemMode();
  const MatterWaterHeater::BoostState_t boost = waterHeater.getBoostState();
  const float setpoint = waterHeater.getHeatingSetpoint();
  if (!haveSnapshot || mode != prevMode || system != prevSystem || boost != prevBoost || setpoint != prevSetpoint) {
    haveSnapshot = true;
    prevMode = mode;
    prevSystem = system;
    prevBoost = boost;
    prevSetpoint = setpoint;
    logWaterHeater("changed");
  }

  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < 5000) {
    return;
  }
  lastUpdate = millis();

  // Boost overrides mode and system mode. Eco heats slower and stops at ECO_MAX_TEMP.
  // Off (and System Off with boost inactive) lets the tank cool. Replace this with a real heater output.
  const bool boostOn = boost == MatterWaterHeater::BOOST_ACTIVE;
  const bool systemHeat = system == MatterWaterHeater::SYSTEM_MODE_HEAT;
  const bool modeOff = mode == MatterWaterHeater::WATER_HEATER_MODE_OFF;
  const bool heating = boostOn || (systemHeat && !modeOff);

  float temperature = waterHeater.getLocalTemperature();
  float target = setpoint;
  float step = 0.5f;
  if (boostOn) {
    step = 1.5f;
  } else if (mode == MatterWaterHeater::WATER_HEATER_MODE_ECO) {
    step = 0.25f;
    if (target > ECO_MAX_TEMP) {
      target = ECO_MAX_TEMP;
    }
  }

  if (heating && temperature < target) {
    temperature += step;
    if (temperature > target) {
      temperature = target;
    }
    waterHeater.setLocalTemperature(temperature);
    waterHeater.setHeatDemand(waterHeater.getHeaterTypes());
  } else if (heating) {
    // Target reached: SystemMode can stay Heat while no element is drawing power.
    waterHeater.setHeatDemand(0);
  } else {
    if (temperature > COLD_WATER_TEMP) {
      temperature -= 0.25f;
      if (temperature < COLD_WATER_TEMP) {
        temperature = COLD_WATER_TEMP;
      }
      waterHeater.setLocalTemperature(temperature);
    }
    waterHeater.setHeatDemand(0);
  }

  updateTankPercentage();
  logWaterHeater("tick");
}
