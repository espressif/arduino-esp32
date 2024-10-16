#pragma once

#include <Arduino.h>
#include <esp_matter.h>

using namespace esp_matter;

class ArduinoMatter {
  public:
    static void begin();
    static inline String getManualPairingCode()  {
      // return the pairing code for manual pairing
      return String("34970112332");
    }
    static inline String getOnboardingQRCodeUrl() {
      // return the URL for the QR code for onboarding
      return String("https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:Y.K9042C00KA0648G00");
    }
    static void start();
    static bool isDeviceCommissioned();
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
    static bool isWiFiConnected();
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    static bool isThreadConnected();
#endif
    static bool isDeviceConnected();
    static void decommission();
};

extern ArduinoMatter Matter;