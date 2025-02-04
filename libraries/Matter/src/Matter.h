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

#include <Arduino.h>
#include <esp_matter.h>
#include <ColorFormat.h>
#include <MatterEndpoints/MatterGenericSwitch.h>
#include <MatterEndpoints/MatterOnOffLight.h>
#include <MatterEndpoints/MatterDimmableLight.h>
#include <MatterEndpoints/MatterColorTemperatureLight.h>
#include <MatterEndpoints/MatterColorLight.h>
#include <MatterEndpoints/MatterEnhancedColorLight.h>
#include <MatterEndpoints/MatterFan.h>
#include <MatterEndpoints/MatterTemperatureSensor.h>
#include <MatterEndpoints/MatterHumiditySensor.h>
#include <MatterEndpoints/MatterContactSensor.h>
#include <MatterEndpoints/MatterPressureSensor.h>
#include <MatterEndpoints/MatterOccupancySensor.h>
#include <MatterEndpoints/MatterOnOffPlugin.h>
#include <MatterEndpoints/MatterThermostat.h>

using namespace esp_matter;

class ArduinoMatter {
public:
  static inline String getManualPairingCode() {
    // return the pairing code for manual pairing
    return String("34970112332");
  }
  static inline String getOnboardingQRCodeUrl() {
    // return the URL for the QR code for onboarding
    return String("https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:Y.K9042C00KA0648G00");
  }
  static void begin();
  static bool isDeviceCommissioned();
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  static bool isWiFiConnected();
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  static bool isThreadConnected();
#endif
  static bool isDeviceConnected();
  static void decommission();

  // list of Matter EndPoints Friend Classes
  friend class MatterGenericSwitch;
  friend class MatterOnOffLight;
  friend class MatterDimmableLight;
  friend class MatterColorTemperatureLight;
  friend class MatterColorLight;
  friend class MatterEnhancedColorLight;
  friend class MatterFan;
  friend class MatterTemperatureSensor;
  friend class MatterHumiditySensor;
  friend class MatterContactSensor;
  friend class MatterPressureSensor;
  friend class MatterOccupancySensor;
  friend class MatterOnOffPlugin;
  friend class MatterThermostat;

protected:
  static void _init();
};

extern ArduinoMatter Matter;

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
