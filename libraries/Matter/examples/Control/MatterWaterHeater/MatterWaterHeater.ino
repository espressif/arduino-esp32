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

// Basic Matter Water Heater (device type 0x050F).
// For Eco, Boost session details, tank percentage, and richer logging see MatterWaterHeaterAdvanced.
#include <Arduino.h>
#include <Matter.h>

MatterWaterHeater waterHeater;

// Fill these in when the board cannot commission over BLE (ESP32 / ESP32-S2).
// With CHIPoBLE the hub sends SSID and password — leave the placeholders unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

constexpr float COLD_WATER_TEMP = 16.0f;
constexpr uint32_t UPDATE_INTERVAL_MS = 5000;
// System Mode Heat without Boost vs Boost (double that step).
constexpr float HEAT_STEP_C = 0.5f;
constexpr float BOOST_HEAT_STEP_C = 1.0f;
// Off / idle: lose heat at 0.25 C/s.
constexpr float COOL_RATE_C_PER_S = 0.25f;

static const char *waterHeaterModeName(MatterWaterHeater::WaterHeaterMode_t mode) {
  switch (mode) {
    case MatterWaterHeater::WATER_HEATER_MODE_OFF:    return "Off";
    case MatterWaterHeater::WATER_HEATER_MODE_MANUAL: return "Manual";
    case MatterWaterHeater::WATER_HEATER_MODE_ECO:    return "Eco";
    default:                                          return "?";
  }
}

static const char *operationName(
  MatterWaterHeater::BoostState_t boost, MatterWaterHeater::SystemMode_t system, MatterWaterHeater::WaterHeaterMode_t mode
) {
  if (boost == MatterWaterHeater::BOOST_ACTIVE) {
    return "Boost";
  }
  if (system == MatterWaterHeater::SYSTEM_MODE_HEAT && mode != MatterWaterHeater::WATER_HEATER_MODE_OFF) {
    return "Heat";
  }
  return "Off";
}

void setup() {
  Serial.begin(115200);

#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // begin() must run before Matter.begin() so tank attributes exist on the hub.
  if (!waterHeater.begin()) {
    Serial.println("Failed to create Matter Water Heater endpoint");
    while (true) {
      delay(1000);
    }
  }

  waterHeater.setLocalTemperature(COLD_WATER_TEMP);
  waterHeater.setHeatingSetpoint(48.0f);
  waterHeater.setSystemMode(MatterWaterHeater::SYSTEM_MODE_HEAT);

  matterSetExampleIdentity("WaterHeater");
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  const MatterWaterHeater::WaterHeaterMode_t mode = waterHeater.getWaterHeaterMode();
  const MatterWaterHeater::SystemMode_t system = waterHeater.getSystemMode();
  const MatterWaterHeater::BoostState_t boost = waterHeater.getBoostState();
  const float setpoint = waterHeater.getHeatingSetpoint();
  const bool boostOn = boost == MatterWaterHeater::BOOST_ACTIVE;

  static bool haveSnapshot = false;
  static MatterWaterHeater::WaterHeaterMode_t prevMode = MatterWaterHeater::WATER_HEATER_MODE_MANUAL;
  static MatterWaterHeater::SystemMode_t prevSystem = MatterWaterHeater::SYSTEM_MODE_HEAT;
  static MatterWaterHeater::BoostState_t prevBoost = MatterWaterHeater::BOOST_INACTIVE;
  static float prevSetpoint = 0.0f;

  if (!haveSnapshot) {
    haveSnapshot = true;
    prevMode = mode;
    prevSystem = system;
    prevBoost = boost;
    prevSetpoint = setpoint;
    Serial.printf(
      "Ready | operation: %s | target: %.1f C | water heater mode: %s\r\n", operationName(boost, system, mode), setpoint,
      waterHeaterModeName(mode)
    );
  } else {
    if (boost != prevBoost) {
      Serial.println(boostOn ? "Controller: Boost on" : "Controller: Cancel Boost");
    }
    if (system != prevSystem) {
      Serial.printf("Controller: system mode %s\r\n", system == MatterWaterHeater::SYSTEM_MODE_HEAT ? "Heat" : "Off");
    }
    if (mode != prevMode) {
      Serial.printf("Controller: water heater mode %s\r\n", waterHeaterModeName(mode));
    }
    if (setpoint != prevSetpoint) {
      Serial.printf("Controller: target temperature %.1f C\r\n", setpoint);
    }
    if (boost != prevBoost || system != prevSystem || mode != prevMode) {
      Serial.printf("Operation: %s\r\n", operationName(boost, system, mode));
    }
    prevMode = mode;
    prevSystem = system;
    prevBoost = boost;
    prevSetpoint = setpoint;
  }

  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdate = millis();

  float temperature = waterHeater.getLocalTemperature();
  // Boost heats even if System or Mode is Off. Mode Off with Boost inactive lets the tank cool.
  const bool heating = boostOn
                       || (system == MatterWaterHeater::SYSTEM_MODE_HEAT && mode != MatterWaterHeater::WATER_HEATER_MODE_OFF);

  if (heating && temperature < setpoint) {
    temperature += boostOn ? BOOST_HEAT_STEP_C : HEAT_STEP_C;
    if (temperature > setpoint) {
      temperature = setpoint;
    }
    waterHeater.setLocalTemperature(temperature);
    waterHeater.setHeatDemand(waterHeater.getHeaterTypes());
  } else {
    if (!heating && temperature > COLD_WATER_TEMP) {
      temperature -= COOL_RATE_C_PER_S * (UPDATE_INTERVAL_MS / 1000.0f);
      if (temperature < COLD_WATER_TEMP) {
        temperature = COLD_WATER_TEMP;
      }
      waterHeater.setLocalTemperature(temperature);
    }
    waterHeater.setHeatDemand(0);
  }

  // A Boost command may have a long duration and no oneShot. End Boost at the setpoint so
  // System Mode Heat continues without Boost. Off is only when the controller writes System Mode Off.
  if (waterHeater.getBoostState() == MatterWaterHeater::BOOST_ACTIVE && temperature >= setpoint) {
    Serial.println("Target reached. HeatDemand idle; leaving Boost for Heat.");
    waterHeater.setBoostState(MatterWaterHeater::BOOST_INACTIVE);
  } else {
    const bool demandOnNow = waterHeater.getHeatDemand() != 0;
    static bool wasAtTarget = false;
    const bool atTarget = heating && !demandOnNow;
    if (atTarget && !wasAtTarget) {
      Serial.printf(
        "Target reached. HeatDemand idle; operation stays %s.\r\n",
        operationName(waterHeater.getBoostState(), system, mode)
      );
    }
    wasAtTarget = atTarget;
  }

  Serial.printf(
    "Temp: %.1f C | Setpoint: %.1f C | HeatDemand: %s | %s\r\n", temperature, setpoint,
    waterHeater.getHeatDemand() != 0 ? "on" : "idle", operationName(waterHeater.getBoostState(), system, mode)
  );
}
