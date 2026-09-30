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

constexpr float COLD_WATER_TEMP = 20.0f;

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

  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < 5000) {
    return;
  }
  lastUpdate = millis();

  float temperature = waterHeater.getLocalTemperature();
  const float setpoint = waterHeater.getHeatingSetpoint();
  // Boost heats even if System or Mode is Off. Mode Off with Boost inactive lets the tank cool.
  const bool heating = waterHeater.getBoostState() == MatterWaterHeater::BOOST_ACTIVE
                       || (waterHeater.getSystemMode() == MatterWaterHeater::SYSTEM_MODE_HEAT
                           && waterHeater.getWaterHeaterMode() != MatterWaterHeater::WATER_HEATER_MODE_OFF);

  if (heating && temperature < setpoint) {
    temperature += 0.5f;
    if (temperature > setpoint) {
      temperature = setpoint;
    }
    waterHeater.setLocalTemperature(temperature);
    waterHeater.setHeatDemand(waterHeater.getHeaterTypes());
  } else {
    if (!heating && temperature > COLD_WATER_TEMP) {
      temperature -= 0.25f;
      if (temperature < COLD_WATER_TEMP) {
        temperature = COLD_WATER_TEMP;
      }
      waterHeater.setLocalTemperature(temperature);
    }
    waterHeater.setHeatDemand(0);
  }

  Serial.printf(
    "Temp: %.1f C | Setpoint: %.1f C | Heat: %s\r\n", temperature, setpoint,
    waterHeater.getHeatDemand() != 0 ? "on" : "off"
  );
}
