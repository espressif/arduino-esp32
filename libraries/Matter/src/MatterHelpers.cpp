// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>

void matterWaitUntilReady(uint32_t timeoutMs) {
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Commission it using the pairing code or QR code.");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  } else {
    Serial.println("Matter Node is commissioned. Waiting for the controller (CASE).");
  }

  const uint32_t start = millis();
  uint32_t lastPrint = 0;
  while (true) {
    const bool commissioned = Matter.isDeviceCommissioned();
    const bool connected = Matter.isDeviceConnected();
    const bool online = Matter.isOnline();
    const uint32_t now = millis();
    if (lastPrint == 0 || (now - lastPrint) >= 10000) {
      lastPrint = now;
      const char *net = "none";
      switch (Matter.getActiveNetwork()) {
        case MATTER_NETWORK_WIFI:     net = "wifi"; break;
        case MATTER_NETWORK_THREAD:   net = "thread"; break;
        case MATTER_NETWORK_ETHERNET: net = "eth"; break;
        default:                      break;
      }
      Serial.printf(
        "[ready] net=%s commissioned=%s connected=%s controller=%s\r\n", net, commissioned ? "Y" : "N", connected ? "Y" : "N", online ? "Y" : "N"
      );
    }
    if (online) {
      Serial.println("Controller CASE session is up.");
      return;
    }
    if (timeoutMs > 0 && (now - start) >= timeoutMs) {
      break;
    }
    delay(200);
  }

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("No Matter fabric after timeout. Restarting.");
    delay(500);
    ESP.restart();
  }
  Serial.println("Commissioned but no CASE session yet. Continuing.");
}

void matterRestartIfNoFabric() {
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter fabric removed. Restarting.");
    delay(500);
    ESP.restart();
  }
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
