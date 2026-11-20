// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief This example demonstrates a Zigbee HA thermostat server.
 *
 * The device joins as a router and exposes the Thermostat cluster (0x0201):
 * system mode, occupied heating and cooling setpoints, local temperature and running state.
 * Home Assistant and Zigbee2MQTT write those attributes, and can also send SetpointRaiseOrLower.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee ZCZR mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define ZIGBEE_HVAC_ENDPOINT 1

#ifdef RGB_BUILTIN
uint8_t led = RGB_BUILTIN;
#else
uint8_t led = 2;
#endif

uint8_t button = BOOT_PIN;

ZigbeeHVAC zbHVAC = ZigbeeHVAC(ZIGBEE_HVAC_ENDPOINT);

static ZigbeeHVACSystemMode hvac_mode = HVAC_SYSTEM_MODE_OFF;
static float heat_setpoint = 20.0f;
static float cool_setpoint = 26.0f;
static uint16_t last_running_state = HVAC_RUNNING_OFF;

static const char *hvac_mode_name(ZigbeeHVACSystemMode mode) {
  switch (mode) {
    case HVAC_SYSTEM_MODE_OFF:             return "OFF";
    case HVAC_SYSTEM_MODE_AUTO:            return "AUTO";
    case HVAC_SYSTEM_MODE_COOL:            return "COOL";
    case HVAC_SYSTEM_MODE_HEAT:            return "HEAT";
    case HVAC_SYSTEM_MODE_EMERGENCY_HEAT:  return "EMERGENCY_HEAT";
    case HVAC_SYSTEM_MODE_PRECOOLING:      return "PRECOOLING";
    case HVAC_SYSTEM_MODE_FAN_ONLY:        return "FAN_ONLY";
    case HVAC_SYSTEM_MODE_DRY:             return "DRY";
    case HVAC_SYSTEM_MODE_SLEEP:           return "SLEEP";
    default:                               return "UNKNOWN";
  }
}

static void indicate_mode(ZigbeeHVACSystemMode mode) {
  switch (mode) {
    case HVAC_SYSTEM_MODE_OFF:            rgbLedWrite(led, 0, 0, 0); break;
    case HVAC_SYSTEM_MODE_HEAT:           rgbLedWrite(led, 255, 0, 0); break;
    case HVAC_SYSTEM_MODE_EMERGENCY_HEAT: rgbLedWrite(led, 255, 40, 0); break;
    case HVAC_SYSTEM_MODE_COOL:           rgbLedWrite(led, 0, 0, 255); break;
    case HVAC_SYSTEM_MODE_PRECOOLING:     rgbLedWrite(led, 80, 0, 255); break;
    case HVAC_SYSTEM_MODE_AUTO:           rgbLedWrite(led, 0, 180, 0); break;
    case HVAC_SYSTEM_MODE_FAN_ONLY:       rgbLedWrite(led, 255, 255, 255); break;
    case HVAC_SYSTEM_MODE_DRY:            rgbLedWrite(led, 0, 180, 180); break;
    case HVAC_SYSTEM_MODE_SLEEP:          rgbLedWrite(led, 0, 0, 40); break;
    default:                              rgbLedWrite(led, 0, 0, 0); break;
  }
}

// Runs from the Zigbee stack callback. Keep it short and do not call ZigbeeHVAC setters here.
static void on_hvac_change(ZigbeeHVACSystemMode mode, float heating_setpoint, float cooling_setpoint) {
  hvac_mode = mode;
  heat_setpoint = heating_setpoint;
  cool_setpoint = cooling_setpoint;
  indicate_mode(mode);
  Serial.printf("HVAC %s  heat %.2f°C  cool %.2f°C\r\n", hvac_mode_name(mode), heating_setpoint, cooling_setpoint);
}

static uint16_t running_state_for(float temperature) {
  switch (hvac_mode) {
    case HVAC_SYSTEM_MODE_HEAT:
    case HVAC_SYSTEM_MODE_EMERGENCY_HEAT: return temperature < heat_setpoint ? HVAC_RUNNING_HEAT : HVAC_RUNNING_OFF;
    case HVAC_SYSTEM_MODE_COOL:
    case HVAC_SYSTEM_MODE_PRECOOLING:     return temperature > cool_setpoint ? HVAC_RUNNING_COOL : HVAC_RUNNING_OFF;
    case HVAC_SYSTEM_MODE_AUTO:
      if (temperature < heat_setpoint) {
        return HVAC_RUNNING_HEAT;
      }
      if (temperature > cool_setpoint) {
        return HVAC_RUNNING_COOL;
      }
      return HVAC_RUNNING_OFF;
    case HVAC_SYSTEM_MODE_FAN_ONLY: return HVAC_RUNNING_FAN;
    default:                        return HVAC_RUNNING_OFF;
  }
}

static void publish_temperature() {
  // temperatureRead() is the chip sensor. Replace this with the room sensor the thermostat should publish.
  float temperature = temperatureRead();
  Serial.printf("Local temperature %.2f°C\r\n", temperature);
  zbHVAC.setLocalTemperature(temperature);

  uint16_t running_state = running_state_for(temperature);
  if (running_state != last_running_state) {
    zbHVAC.setRunningState(running_state);
    last_running_state = running_state;
  }
}

void setup() {
  Serial.begin(115200);
  rgbLedWrite(led, 0, 0, 0);
  pinMode(button, INPUT_PULLUP);

  if (!Zigbee.role(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  zbHVAC.setManufacturerAndModel("Espressif", "ZBHVAC");
  zbHVAC.setControlSequence(HVAC_SEQ_COOLING_AND_HEATING);
  zbHVAC.setSystemMode(HVAC_SYSTEM_MODE_OFF);
  zbHVAC.setLocalTemperature(temperatureRead());
  zbHVAC.onHVACChange(on_hvac_change);

  Zigbee.addEndpoint(&zbHVAC);

  Serial.println("Starting Zigbee...");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Zigbee started successfully!");
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  publish_temperature();
}

void loop() {
  static uint32_t last_publish_ms = 0;
  if (millis() - last_publish_ms >= 10000) {
    last_publish_ms = millis();
    publish_temperature();
  }

  if (digitalRead(button) == LOW) {
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }
  delay(100);
}
