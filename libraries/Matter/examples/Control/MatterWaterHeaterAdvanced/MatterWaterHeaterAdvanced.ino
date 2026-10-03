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

// Full Matter Water Heater (device type 0x050F).
// Start with MatterWaterHeater if you only need temperature, setpoint, and Boost.
//
// This sketch simulates a tank and shows every Arduino-facing feature:
//   - Water Heater Mode (Off / Manual / Eco) — independent of Thermostat System Mode
//   - Thermostat SystemMode (Off / Heat)
//   - Hub Boost / CancelBoost (duration, one-shot, optional temporary setpoint)
//   - TankVolume / TankPercentage
//   - EstimatedHeatRequired (energy still needed to reach the heating goal)
//   - HeatDemand (which heater types are drawing power; names every WHM bit)
//   - Thermostat setpoint limits (controller target dial, after begin())
//
// begin() installs a Water Heater Management delegate. The hub Boost command is
// handled in the library. This sketch reports temperature, tank %, and estimated
// heat so a one-shot Boost can finish when the tank is hot enough.
#include <Arduino.h>
#include <Matter.h>

MatterWaterHeater waterHeater;

// Fill these in when the board cannot commission over BLE (ESP32 / ESP32-S2).
// With CHIPoBLE the hub sends SSID and password — leave the placeholders unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

constexpr float COLD_WATER_TEMP = 16.0f;
constexpr float INITIAL_WATER_TEMP = 20.0f;
// Controller OccupiedHeatingSetpoint range (the target dial). Not LocalTemperature.
// begin() defaults are 20 C ... 85 C; setAbsolute*() rejects values outside that span.
constexpr float SETPOINT_DIAL_MIN_C = 25.0f;
constexpr float SETPOINT_DIAL_MAX_C = 60.0f;
constexpr float TANK_VOLUME_LITERS = 100.0f;
// Matter EstimatedHeatRequired is mWh. ~1.163 Wh raises 1 L of water by 1 C.
constexpr int64_t WATER_HEAT_MWH_PER_L_PER_C = 1163;
constexpr uint32_t UPDATE_INTERVAL_MS = 5000;
// Same rates as MatterWaterHeater: System Mode Heat vs Boost (2x).
constexpr float MANUAL_HEAT_STEP_C = 0.5f;
constexpr float HIGH_DEMAND_HEAT_STEP_C = 1.0f;
// Matter Water Heater Mode Eco: slower, and hold below the hub setpoint.
constexpr float ECO_MODE_HEAT_STEP_C = 0.25f;
constexpr float ECO_MAX_TEMP = 40.0f;
// Off / idle: lose heat at 0.25 C/s.
constexpr float COOL_RATE_C_PER_S = 0.25f;
// HeaterTypes = every WHM source bit (capability). HeatDemand is a subset.
constexpr uint8_t HEATER_TYPES = MatterWaterHeater::IMMERSION_ELEMENT_1 | MatterWaterHeater::IMMERSION_ELEMENT_2 |
                                 MatterWaterHeater::HEAT_PUMP | MatterWaterHeater::BOILER | MatterWaterHeater::OTHER;

const char *waterHeaterModeName(MatterWaterHeater::WaterHeaterMode_t mode) {
  switch (mode) {
    case MatterWaterHeater::WATER_HEATER_MODE_OFF:    return "Off";
    case MatterWaterHeater::WATER_HEATER_MODE_MANUAL: return "Manual";
    case MatterWaterHeater::WATER_HEATER_MODE_ECO:    return "Eco";
    default:                                          return "?";
  }
}

const char *operationName(
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

// HeatDemand is a HeaterTypes bitmap of sources drawing power (0 = idle).
static void appendHeatDemandName(char *out, size_t outSize, const char *name) {
  if (out[0] != '\0') {
    strlcat(out, "+", outSize);
  }
  strlcat(out, name, outSize);
}

const char *heatDemandName(uint8_t demand) {
  static char buf[64];
  if (demand == 0) {
    return "idle";
  }
  buf[0] = '\0';
  if (demand & MatterWaterHeater::IMMERSION_ELEMENT_1) {
    appendHeatDemandName(buf, sizeof(buf), "Immersion 1");
  }
  if (demand & MatterWaterHeater::IMMERSION_ELEMENT_2) {
    appendHeatDemandName(buf, sizeof(buf), "Immersion 2");
  }
  if (demand & MatterWaterHeater::HEAT_PUMP) {
    appendHeatDemandName(buf, sizeof(buf), "Heat pump");
  }
  if (demand & MatterWaterHeater::BOILER) {
    appendHeatDemandName(buf, sizeof(buf), "Boiler");
  }
  if (demand & MatterWaterHeater::OTHER) {
    appendHeatDemandName(buf, sizeof(buf), "Other");
  }
  if (buf[0] == '\0') {
    snprintf(buf, sizeof(buf), "unknown (0x%02X)", demand);
  }
  return buf;
}

// HeatDemand while the tank is below the goal. Boiler is the everyday source.
// Eco: heat pump. Boost: boiler plus both immersion elements. OTHER stays in
// HeaterTypes only so heatDemandName() still has a name for that bit.
uint8_t activeHeatSources(bool boostOn, MatterWaterHeater::WaterHeaterMode_t mode) {
  if (boostOn) {
    return MatterWaterHeater::BOILER | MatterWaterHeater::IMMERSION_ELEMENT_1 | MatterWaterHeater::IMMERSION_ELEMENT_2;
  }
  if (mode == MatterWaterHeater::WATER_HEATER_MODE_ECO) {
    return MatterWaterHeater::HEAT_PUMP;
  }
  return MatterWaterHeater::BOILER;
}

// Heating goal used for Eco cap and EstimatedHeatRequired. Boost / Manual use
// OccupiedHeatingSetpoint; Eco (no Boost) stops at ECO_MAX_TEMP.
float heatingGoalC() {
  float goal = waterHeater.getHeatingSetpoint();
  if (waterHeater.getBoostState() != MatterWaterHeater::BOOST_ACTIVE
      && waterHeater.getWaterHeaterMode() == MatterWaterHeater::WATER_HEATER_MODE_ECO && goal > ECO_MAX_TEMP) {
    goal = ECO_MAX_TEMP;
  }
  return goal;
}

// Remaining energy to reach heatingGoalC() for the whole tank. Not the heat rate:
// Eco reports less because the goal is 40 C; Off still reports energy if the tank
// is below the goal; the value falls as LocalTemperature rises.
void updateEstimatedHeatRequired() {
  float deltaC = heatingGoalC() - waterHeater.getLocalTemperature();
  if (deltaC < 0.0f) {
    deltaC = 0.0f;
  }
  const int64_t energy_mWh =
    static_cast<int64_t>(TANK_VOLUME_LITERS * deltaC * static_cast<float>(WATER_HEAT_MWH_PER_L_PER_C) + 0.5f);
  waterHeater.setEstimatedHeatRequired(energy_mWh);
}

// Map tank temperature to TankPercentage (0 % at cold water, 100 % at the current setpoint).
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
    "%s | Temp: %.1f C | Setpoint: %.1f C | Op: %s | Mode: %s | System: %s | Boost: %s | Tank: %u %% | HeatReq: %.3f kWh | HeatDemand: %s\r\n",
    reason, waterHeater.getLocalTemperature(), waterHeater.getHeatingSetpoint(),
    operationName(waterHeater.getBoostState(), waterHeater.getSystemMode(), waterHeater.getWaterHeaterMode()),
    waterHeaterModeName(waterHeater.getWaterHeaterMode()),
    waterHeater.getSystemMode() == MatterWaterHeater::SYSTEM_MODE_HEAT ? "Heat" : "Off",
    waterHeater.getBoostState() == MatterWaterHeater::BOOST_ACTIVE ? "on" : "off", waterHeater.getTankPercentage(),
    waterHeater.getEstimatedHeatRequired() / 1000000.0f, heatDemandName(waterHeater.getHeatDemand())
  );
}

void setup() {
  Serial.begin(115200);

#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  // begin() must run before Matter.begin(). It adds EnergyManagement / TankPercent,
  // Thermostat setpoint limits (controller heating range), and the
  // WHM delegate that accepts Boost / CancelBoost from the hub.
  if (!waterHeater.begin()) {
    Serial.println("Failed to create Matter Water Heater endpoint");
    while (true) {
      delay(1000);
    }
  }

  waterHeater.setHeaterTypes(HEATER_TYPES);
  waterHeater.setTankVolume(static_cast<uint16_t>(TANK_VOLUME_LITERS));
  // Narrow the controller setpoint dial. Tank temperature is independent
  // (starts at INITIAL_WATER_TEMP and can cool to COLD_WATER_TEMP).
  waterHeater.setAbsoluteMinimumHeatingSetpoint(SETPOINT_DIAL_MIN_C);
  waterHeater.setAbsoluteMaximumHeatingSetpoint(SETPOINT_DIAL_MAX_C);
  waterHeater.setMinimumHeatingSetpoint(SETPOINT_DIAL_MIN_C);
  waterHeater.setMaximumHeatingSetpoint(SETPOINT_DIAL_MAX_C);
  waterHeater.setLocalTemperature(INITIAL_WATER_TEMP);
  waterHeater.setHeatingSetpoint(48.0f);
  // setSystemMode() copies HeaterTypes into HeatDemand; override with Manual = Boiler.
  waterHeater.setSystemMode(MatterWaterHeater::SYSTEM_MODE_HEAT);
  waterHeater.setWaterHeaterMode(MatterWaterHeater::WATER_HEATER_MODE_MANUAL);
  waterHeater.setHeatDemand(activeHeatSources(false, MatterWaterHeater::WATER_HEATER_MODE_MANUAL));
  waterHeater.setTankPercentage(0);
  // EnergyManagement: remaining heat for 100 L from INITIAL_WATER_TEMP to 48 C.
  updateEstimatedHeatRequired();

  matterSetExampleIdentity("WaterHeater");
  Matter.begin();
  matterWaitUntilReady();

  Serial.println("Hub Boost / CancelBoost is handled by the endpoint.");
  Serial.println("A controller Boost command starts fast heat; CancelBoost or reaching the setpoint ends it.");
  Serial.println("Water Heater Mode Eco is a separate cluster (cap 40 C), not System Mode Heat.");
  Serial.println("CancelBoost, or a timed / one-shot Boost, returns Boost to off and restores any temporary setpoint.");
  Serial.println("HeaterTypes: Immersion 1, Immersion 2, Heat pump, Boiler, Other.");
  Serial.println("HeatDemand while heating: Eco=Heat pump, Manual=Boiler, Boost=Boiler+Immersion 1+Immersion 2.");
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
    logWaterHeater("ready");
  } else if (mode != prevMode || system != prevSystem || boost != prevBoost || setpoint != prevSetpoint) {
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
    Serial.printf("Operation: %s\r\n", operationName(boost, system, mode));
    const char *reason = "changed";
    if (boost != prevBoost) {
      reason = boostOn ? "boost-on" : "boost-off";
    }
    prevMode = mode;
    prevSystem = system;
    prevBoost = boost;
    prevSetpoint = setpoint;
    const bool heatingNow = boostOn || (system == MatterWaterHeater::SYSTEM_MODE_HEAT && mode != MatterWaterHeater::WATER_HEATER_MODE_OFF);
    if (heatingNow && waterHeater.getLocalTemperature() < heatingGoalC()) {
      waterHeater.setHeatDemand(activeHeatSources(boostOn, mode));
    } else {
      waterHeater.setHeatDemand(0);
    }
    updateEstimatedHeatRequired();
    logWaterHeater(reason);
  }

  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdate = millis();

  // Boost overrides Mode and SystemMode. Matter Mode Eco heats slower and stops at ECO_MAX_TEMP.
  // Off (and System Off with Boost inactive) lets the tank cool.
  // setLocalTemperature() is what a one-shot Boost uses to decide it is done.
  const bool systemHeat = system == MatterWaterHeater::SYSTEM_MODE_HEAT;
  const bool modeOff = mode == MatterWaterHeater::WATER_HEATER_MODE_OFF;
  const bool heating = boostOn || (systemHeat && !modeOff);

  float temperature = waterHeater.getLocalTemperature();
  const float target = heatingGoalC();
  float step = MANUAL_HEAT_STEP_C;
  if (boostOn) {
    step = HIGH_DEMAND_HEAT_STEP_C;
  } else if (mode == MatterWaterHeater::WATER_HEATER_MODE_ECO) {
    step = ECO_MODE_HEAT_STEP_C;
  }

  if (heating && temperature < target) {
    temperature += step;
    if (temperature > target) {
      temperature = target;
    }
    waterHeater.setLocalTemperature(temperature);
    waterHeater.setHeatDemand(activeHeatSources(boostOn, mode));
  } else if (heating) {
    waterHeater.setHeatDemand(0);
  } else {
    if (temperature > COLD_WATER_TEMP) {
      temperature -= COOL_RATE_C_PER_S * (UPDATE_INTERVAL_MS / 1000.0f);
      if (temperature < COLD_WATER_TEMP) {
        temperature = COLD_WATER_TEMP;
      }
      waterHeater.setLocalTemperature(temperature);
    }
    waterHeater.setHeatDemand(0);
  }

  updateTankPercentage();
  updateEstimatedHeatRequired();

  // A Boost command may have a long duration and no oneShot. End Boost at the heating
  // setpoint so System Mode Heat continues. oneShot / duration still finish in-library.
  if (waterHeater.getBoostState() == MatterWaterHeater::BOOST_ACTIVE && temperature >= setpoint) {
    Serial.println("Target reached. HeatDemand idle; leaving Boost for Heat.");
    waterHeater.setBoostState(MatterWaterHeater::BOOST_INACTIVE);
    // setBoostState() syncs HeatDemand to HeaterTypes; the tank is at the goal.
    waterHeater.setHeatDemand(0);
  } else {
    static bool wasAtTarget = false;
    const bool atTarget = heating && waterHeater.getHeatDemand() == 0;
    if (atTarget && !wasAtTarget) {
      Serial.printf(
        "Target reached. HeatDemand idle; operation stays %s.\r\n",
        operationName(waterHeater.getBoostState(), system, mode)
      );
    }
    wasAtTarget = atTarget;
  }

  logWaterHeater("tick");
}
