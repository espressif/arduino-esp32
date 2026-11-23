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
 * The device joins as an end device and exposes the Thermostat cluster (0x0201):
 * system mode, occupied heating and cooling setpoints, local temperature and running state.
 * Home Assistant and Zigbee2MQTT write those attributes, and can also send SetpointRaiseOrLower.
 *
 * There is no room sensor here. loop() simulates one: the room starts at 18°C, below the
 * 20°C heating setpoint, and moves 0.5°C/s toward the active setpoint. Auto and Heat warm
 * it. Cool places it above the cooling setpoint so it has somewhere to cool from.
 * A short BOOT press raises the room by one degree more than the gap between the heating
 * and cooling setpoints, so one press leaves that band. Hold BOOT for about a second to
 * step the same amount down. Hold it for 3 seconds to factory-reset.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#include <Arduino.h>
#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
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
static float room_c = 18.0f;
static uint16_t last_running_state = HVAC_RUNNING_OFF;
static volatile bool hvac_dirty = false;

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

static const char *running_name(uint16_t state) {
  if (state & HVAC_RUNNING_HEAT) {
    return "heat";
  }
  if (state & HVAC_RUNNING_COOL) {
    return "cool";
  }
  if (state & HVAC_RUNNING_FAN) {
    return "fan";
  }
  return "idle";
}

// Runs in the Zigbee task. Set a flag and return. LED and serial run from loop().
static void on_hvac_change(ZigbeeHVACSystemMode mode, float heating_setpoint, float cooling_setpoint) {
  (void)mode;
  (void)heating_setpoint;
  (void)cooling_setpoint;
  hvac_dirty = true;
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

// Move the simulated room 0.5°C toward the setpoint of the active mode.
static void simulate_room() {
  float target = room_c;
  switch (hvac_mode) {
    case HVAC_SYSTEM_MODE_HEAT:
    case HVAC_SYSTEM_MODE_EMERGENCY_HEAT:
      if (room_c < heat_setpoint) {
        target = heat_setpoint;
      }
      break;
    case HVAC_SYSTEM_MODE_COOL:
    case HVAC_SYSTEM_MODE_PRECOOLING:
      if (room_c > cool_setpoint) {
        target = cool_setpoint;
      }
      break;
    case HVAC_SYSTEM_MODE_AUTO:
      if (room_c < heat_setpoint) {
        target = heat_setpoint;
      } else if (room_c > cool_setpoint) {
        target = cool_setpoint;
      }
      break;
    default: break;
  }

  if (room_c < target) {
    room_c += 0.5f;
    if (room_c > target) {
      room_c = target;
    }
  } else if (room_c > target) {
    room_c -= 0.5f;
    if (room_c < target) {
      room_c = target;
    }
  }
}

static void publish_temperature() {
  zbHVAC.setLocalTemperature(room_c);

  uint16_t running_state = running_state_for(room_c);
  if (running_state != last_running_state) {
    zbHVAC.setRunningState(running_state);
    last_running_state = running_state;
  }
}

static void print_room() {
  Serial.printf("Local temperature %.2f°C  %s\r\n", room_c, running_name(running_state_for(room_c)));
}

// A mode whose setpoint the room already satisfies never moves the number. Put the room
// where that mode has work, then let simulate_room() walk it back.
static void place_room_for_mode() {
  switch (hvac_mode) {
    case HVAC_SYSTEM_MODE_HEAT:
    case HVAC_SYSTEM_MODE_EMERGENCY_HEAT:
      if (room_c >= heat_setpoint) {
        room_c = heat_setpoint - 2.0f;
      }
      break;
    case HVAC_SYSTEM_MODE_COOL:
    case HVAC_SYSTEM_MODE_PRECOOLING:
      if (room_c <= cool_setpoint) {
        room_c = cool_setpoint + 2.0f;
      }
      break;
    case HVAC_SYSTEM_MODE_AUTO:
      if (room_c >= heat_setpoint && room_c <= cool_setpoint) {
        room_c = heat_setpoint - 2.0f;
      }
      break;
    default: break;
  }
}

// One degree past the heat/cool gap. From anywhere between the setpoints, one step lands outside.
static float room_step() {
  float span = cool_setpoint - heat_setpoint;
  if (span < 1.0f) {
    span = 1.0f;
  }
  return (float)((int)(span + 0.999f)) + 1.0f;
}

static void nudge_room(bool lower) {
  float delta = lower ? -room_step() : room_step();
  room_c += delta;
  Serial.printf("Button %+.0f°C\r\n", delta);
  print_room();
  publish_temperature();
  if (zbHVAC.bound() && !zbHVAC.report()) {
    Serial.println("HVAC report was rejected");
  }
}

static void apply_hvac_change() {
  if (!hvac_dirty) {
    return;
  }
  hvac_dirty = false;
  ZigbeeHVACSystemMode mode = zbHVAC.getSystemMode();
  bool mode_changed = mode != hvac_mode;
  hvac_mode = mode;
  heat_setpoint = zbHVAC.getOccupiedHeatingSetpoint();
  cool_setpoint = zbHVAC.getOccupiedCoolingSetpoint();
  // Only a new mode relocates the room. A setpoint write, and ZHA's retry of that same write,
  // would otherwise pull the room 2°C below the target every time it is already there.
  if (mode_changed) {
    place_room_for_mode();
  }
  indicate_mode(hvac_mode);
  Serial.printf("HVAC %s  heat %.2f°C  cool %.2f°C  room %.2f°C\r\n", hvac_mode_name(hvac_mode), heat_setpoint, cool_setpoint, room_c);
  publish_temperature();
}

void setup() {
  Serial.begin(115200);
  rgbLedWrite(led, 0, 0, 0);
  pinMode(button, INPUT_PULLUP);

  if (!Zigbee.role(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to init!");
    Serial.println("Rebooting...");
    delay(1000);
    ESP.restart();
  }

  zbHVAC.setManufacturerAndModel("Espressif", "ZigbeeHVAC");
  // After role() and before begin(). Makes ThermostatRunningState reportable.
  zbHVAC.enableReporting();
  zbHVAC.setControlSequence(HVAC_SEQ_COOLING_AND_HEATING);
  zbHVAC.setSystemMode(HVAC_SYSTEM_MODE_OFF);
  zbHVAC.setLocalTemperature(room_c);
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
  Serial.printf(
    "BOOT: short press %+.0f°C, hold 1s %+.0f°C, hold 3s to factory-reset.\r\n", room_step(), -room_step()
  );
  print_room();
}

void loop() {
  apply_hvac_change();

  static uint32_t last_publish_ms = 0;
  if (millis() - last_publish_ms >= 1000) {
    last_publish_ms = millis();
    float before = room_c;
    simulate_room();
    print_room();
    if (room_c != before) {
      publish_temperature();
    }
  }

  if (digitalRead(button) == LOW) {
    uint32_t startTime = millis();
    delay(100);
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    // Released before the factory reset. A short press raises, a ~1s hold lowers.
    nudge_room((millis() - startTime) >= 1000);
  }
  delay(100);
}
