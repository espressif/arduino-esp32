/*
 * ESP-Hosted OTA Update Example
 *
 * This example demonstrates how to update the ESP-Hosted co-processor firmware
 * over-the-air (OTA). The ESP-Hosted solution allows an ESP32 to act as a WiFi
 * co-processor for other microcontrollers.
 *
 * Prerequisites:
 * - ESP32 with ESP-Hosted firmware configured as WiFi co-processor
 * - Network connectivity to download firmware updates
 * - Valid WiFi credentials
 */

#include "WiFi.h"           // WiFi library for network connectivity
#include "ESP_HostedOTA.h"  // ESP-Hosted OTA update functionality

// WiFi network credentials - CHANGE THESE TO YOUR NETWORK SETTINGS
const char *ssid = "your-ssid";          // Replace with your WiFi network name
const char *password = "your-password";  // Replace with your WiFi password

void setup() {
  // Step 1: Initialize serial communication for debugging output
  Serial.begin(115200);

  // Step 2: Initialize the ESP-Hosted WiFi station mode
  // This prepares the ESP-Hosted co-processor for WiFi operations
  WiFi.STA.begin();

  // Step 3: Display connection attempt information
  Serial.println();
  Serial.println("******************************************************");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Step 4: Attempt to connect to the specified WiFi network
  WiFi.STA.connect(ssid, password);

  // Step 5: Wait for WiFi connection to be established
  // Display progress dots while connecting
  while (WiFi.STA.status() != WL_CONNECTED) {
    delay(500);         // Wait 500ms between connection attempts
    Serial.print(".");  // Show connection progress
  }
  Serial.println();

  // Step 6: Display successful connection information
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.STA.localIP());

  // Step 7: Attempt to update the ESP-Hosted co-processor firmware
  // This function will:
  // - Check if ESP-Hosted is initialized
  // - Verify if an update is available
  // - Download and install the firmware update if needed
  if (updateEspHostedSlave()) {
    // Step 8: Restart the host ESP32 after successful update
    // This is currently required to properly activate the new firmware
    // on the ESP-Hosted co-processor
    ESP.restart();
  }
}

void loop() {
  delay(1000);
}
