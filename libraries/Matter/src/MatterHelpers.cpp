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
#include "soc/soc_caps.h"
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <stdio.h>
#include <string.h>
#if !CONFIG_ENABLE_CHIPOBLE && (SOC_WIFI_SUPPORTED || CONFIG_ESP_HOSTED_ENABLED)
#include <WiFi.h>

void matterConnectWiFi(const char *ssid, const char *password) {
  if (ssid == nullptr) {
    ssid = "";
  }
  if (password == nullptr) {
    password = "";
  }
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
#if CONFIG_LWIP_IPV6
  WiFi.enableIPv6(true);
#endif
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Wi-Fi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
}
#endif /* !CONFIG_ENABLE_CHIPOBLE && Wi-Fi */

// CONFIG_IDF_TARGET is "esp32c6" / "esp32" / "esp32s3". Product uses "ESP32-C6".
static void matterExampleSocName(char *out, size_t outSize) {
  if (out == nullptr || outSize < 6) {
    return;
  }
  const char *target = CONFIG_IDF_TARGET;
  size_t i = 0;
  const char prefix[] = "ESP32";
  for (; prefix[i] != '\0' && (i + 1) < outSize; i++) {
    out[i] = prefix[i];
  }
  const size_t tlen = strlen(target);
  if (tlen > 5 && (i + 1) < outSize) {
    out[i++] = '-';
    for (size_t j = 5; target[j] != '\0' && (i + 1) < outSize; j++) {
      char c = target[j];
      if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
      }
      out[i++] = c;
    }
  }
  out[i] = '\0';
}

bool matterSetExampleIdentity(const char *endpointName) {
  if (endpointName == nullptr || endpointName[0] == '\0') {
    endpointName = "Matter";
  }
  Matter.setVendorName("Espressif");

  char soc[16] = {};
  matterExampleSocName(soc, sizeof(soc));

  // Product name string for Matter attributes (VendorName and ProductName)
  char product[33] = {};
  snprintf(product, sizeof(product), "%s %s", soc, endpointName);

  // Set ProductName for Matter Mobile App
  Matter.setProductName(product);

  // Set DeviceName for NodeLabel (used by Alexa and other controllers)
  return Matter.setDeviceName(product);
}

static void printReadyStatus(bool commissioned, bool connected, bool online) {
  const char *net = "none";
  switch (Matter.getActiveNetwork()) {
    case MATTER_NETWORK_WIFI:     net = "wifi"; break;
    case MATTER_NETWORK_THREAD:   net = "thread"; break;
    case MATTER_NETWORK_ETHERNET: net = "eth"; break;
    default:                      break;
  }
  Serial.printf("[ready] net=%s commissioned=%s connected=%s controller=%s\r\n", net, commissioned ? "Y" : "N", connected ? "Y" : "N", online ? "Y" : "N");
}

void matterWaitUntilReady(uint32_t timeoutMs) {
  if (!Matter.isStackStarted()) {
    Serial.println("Matter.begin() failed. Pairing codes are not available.");
    Serial.println("Halting. loop() will not run.");
    while (true) {
      delay(1000);
    }
  }
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
    bool printed = false;
    if (lastPrint == 0 || (now - lastPrint) >= 10000) {
      lastPrint = now;
      printReadyStatus(commissioned, connected, online);
      printed = true;
    }
    if (online) {
      if (!printed) {
        printReadyStatus(commissioned, connected, online);
      }
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
  if (!Matter.isStackStarted()) {
    return;
  }
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter fabric removed. Restarting.");
    delay(500);
    ESP.restart();
  }
}

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
