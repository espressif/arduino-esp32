/* Wi-Fi FTM Responder Arduino Example

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/
#include "WiFi.h"
// Change the SSID and PASSWORD here if needed
const char* WIFI_FTM_SSID = "WiFi_FTM_Responder";
const char* WIFI_FTM_PASS = "ftm_responder";

void setup() {
  Serial.begin(115200);
  Serial.println("Starting SoftAP with FTM Responder support");
  // Enable AP with FTM support (last argument is 'true')
  WiFi.softAP(WIFI_FTM_SSID, WIFI_FTM_PASS, 1, 0, 4, true);
}

void loop() {
  delay(1000);
}
