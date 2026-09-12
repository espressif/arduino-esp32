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

// On-network commissioning over Ethernet. Side-by-side with MatterOnNetworkWiFi:
// same On/Off Light; the delta is PHY macros, ETH.begin() (EMAC or SPI), IPv6, waitForNetwork().
// Do not start Arduino ESPmDNS — CHIP owns the mDNS responder.
// Do not use the Arduino BLE library (BLE.h / BLEDevice) in this sketch.

// Defaults used when the variant does not define ETH_PHY_TYPE (SPI W5500).
#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 1
#define ETH_PHY_CS 15
#define ETH_PHY_IRQ 4
#define ETH_PHY_RST 5
#define ETH_SPI_SCK 14
#define ETH_SPI_MISO 12
#define ETH_SPI_MOSI 13
#endif

#include <Arduino.h>
#include <ETH.h>
#include <Matter.h>
#include <SPI.h>

MatterOnOffLight OnOffLight;

#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;
#endif

const uint8_t buttonPin = BOOT_PIN;
uint32_t button_time_stamp = 0;
bool button_state = false;
const uint32_t decommissioningTimeout = 5000;

bool onOffLightCallback(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  return true;
}

static void halt(const char *reason) {
  Serial.println(reason);
  Serial.println("Halting. loop() will not run.");
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, HIGH);
  Serial.printf("\r\n===============\r\nMatter Over ETH\r\nLED Pin(%d)\r\n===============\r\n", ledPin);
  delay(500);
  digitalWrite(ledPin, LOW);

  // Compile-time: CONFIG_ETH_ENABLED. Does not mean a cable is plugged in.
  if (!Matter.isNetworkSupported(MATTER_NETWORK_ETHERNET)) {
    halt("Ethernet is not enabled in this build.");
  }
  Serial.println("Ethernet network is supported.");

  // Before any accessory begin(). One-arg Ethernet turns CHIPoBLE off.
  // Do not also call setBLECommissioningEnabled(false) — this already does that.
  if (!Matter.selectNetwork(MATTER_NETWORK_ETHERNET)) {
    halt("selectNetwork(Ethernet) failed.");
  }
  Serial.println("Ethernet network selected.");

  // Internal EMAC (original ESP32) when the variant defines MDC/MDIO.
  // Otherwise SPI PHY — defaults below are W5500; override ETH_PHY_* / ETH_SPI_* for your board.
#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(ETH_PHY_MDC) && defined(ETH_PHY_MDIO)
  Serial.println("ETH type: internal EMAC");
  if (!ETH.begin()) {
#else
  Serial.println("ETH type: SPI");
  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI)) {
#endif
    halt("ETH.begin() failed. Set ETH_PHY_* for your board.");
  }

  ETH.enableIPv6();
  Serial.println("Waiting for Ethernet IPv6...");
  if (!Matter.waitForNetwork(30000)) {
    halt("No Ethernet IPv6 address within 30 s.");
  }
  Serial.printf("Ethernet IPv6 ready, active network=%d\r\n", static_cast<int>(Matter.getActiveNetwork()));

  OnOffLight.begin();
  OnOffLight.onChange(onOffLightCallback);
  Matter.begin();
  Serial.printf("BLE commissioning enabled: %s\r\n", Matter.isBLECommissioningEnabled() ? "YES" : "NO");

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Commission it on the Ethernet network with the pairing code or QR code.");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
}

void loop() {
  if (digitalRead(buttonPin) == LOW && !button_state) {
    button_time_stamp = millis();
    button_state = true;
  }
  if (digitalRead(buttonPin) == HIGH && button_state) {
    button_state = false;
  }
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();
  }
  delay(500);
}
