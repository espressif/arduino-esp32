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

/**
 * @brief This example demonstrates simple Zigbee Gateway functionality.
 *
 * The example demonstrates how to use Zigbee library on ESP32s to create a Zigbee Gateway, updating the time from NTP server.
 * The Gateway is able to communicate with Zigbee end devices and send/receive data to/from them.
 * The Gateway is also able to communicate with the cloud or other devices over Wi-Fi / BLE.
 *
 * Proper Zigbee mode must be selected in Tools->Zigbee mode->Zigbee ZCZR (coordinator/router)
 * and also the correct partition scheme must be selected in Tools->Partition Scheme->Zigbee ZCZR
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"

/* Zigbee gateway configuration */
#define GATEWAY_ENDPOINT_NUMBER 1
#define GATEWAY_RCP_UART_PORT   UART_NUM_1  // UART 0 is used for Serial communication
#define GATEWAY_RCP_RX_PIN      4
#define GATEWAY_RCP_TX_PIN      5

ZigbeeGateway zbGateway = ZigbeeGateway(GATEWAY_ENDPOINT_NUMBER);

/* Wi-Fi credentials */
const char *ssid = "your-ssid";
const char *password = "your-password";

/* NTP server configuration */
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char *time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

/* Time structure */
struct tm timeinfo;

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  // Initialize Wi-Fi and connect to AP
  WiFi.begin(ssid, password);
  esp_sntp_servermode_dhcp(1);  // (optional)

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Initialize Zigbee and Begin Zigbee stack
  // Optional: set Zigbee device name and model
  zbGateway.setManufacturerAndModel("Espressif", "ZigbeeGateway");
  zbGateway.addTimeCluster(timeinfo, gmtOffset_sec);

  // Add endpoint to Zigbee Core
  Serial.println("Adding Zigbee Gateway endpoint");
  Zigbee.addEndpoint(&zbGateway);

  // Optional: Open network for 180 seconds after boot
  Zigbee.setRebootOpenNetwork(180);

  // Set custom radio configuration for RCP communication
  esp_zb_radio_config_t radio_config = ZIGBEE_DEFAULT_UART_RCP_RADIO_CONFIG();
  radio_config.radio_uart_config.port = GATEWAY_RCP_UART_PORT;
  radio_config.radio_uart_config.rx_pin = (gpio_num_t)GATEWAY_RCP_RX_PIN;
  radio_config.radio_uart_config.tx_pin = (gpio_num_t)GATEWAY_RCP_TX_PIN;

  Zigbee.setRadioConfig(radio_config);

  // When all EPs are registered, start Zigbee with ZIGBEE_COORDINATOR or ZIGBEE_ROUTER mode
  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }

  // set notification call-back function
  sntp_set_time_sync_notification_cb(timeavailable);
  sntp_set_sync_interval(30000);  // sync every 30 seconds

  // config time zone and NTP servers
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
}

void loop() {
  // Nothing to do here in this example
}

void printLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  zbGateway.setTime(timeinfo);
  Serial.println("Time updated in Zigbee Gateway");
}

// Callback function (gets called when time adjusts via NTP)
void timeavailable(struct timeval *t) {
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}
