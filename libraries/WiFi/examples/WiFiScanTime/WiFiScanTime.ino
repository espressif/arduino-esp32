/*
 *  This sketch demonstrates how to scan WiFi networks with custom scanning time.
 *  The API is based on the Arduino WiFi Shield library, but has significant changes as newer WiFi functions are supported.
 *  E.g. the return value of `encryptionType()` different because more modern encryption is supported.
 */

/*
 * WiFi scan timing parameters explained:
 *
 * min=0, max=0: scan dwells on each channel for 120 ms.
 * min>0, max=0: scan dwells on each channel for 120 ms.
 * min=0, max>0: scan dwells on each channel for max ms.
 * min>0, max>0: the minimum time the scan dwells on each channel is min ms. If no AP is found during this time frame, the scan switches to the next channel. Otherwise, the scan dwells on the channel for max ms.
 */

#include "WiFi.h"

void wifiScan(uint16_t min_time, uint16_t max_time) {
  Serial.println("Scan start");

  // Set the minimum time per channel for active scanning.
  WiFi.setScanActiveMinTime(min_time);

  // Capture the start time of the scan.
  uint32_t start = millis();

  // WiFi.scanNetworks will return the number of networks found.
  // Scan networks with those options: Synchrone mode, show hidden networks, active scan, max scan time per channel.
  int n = WiFi.scanNetworks(false, true, false, max_time);
  Serial.printf("Scan done, elapsed time: %lu ms\n", millis() - start);
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.printf("%2d", i + 1);
      Serial.print(" | ");
      Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
      Serial.print(" | ");
      Serial.printf("%4ld", WiFi.RSSI(i));
      Serial.print(" | ");
      Serial.printf("%2ld", WiFi.channel(i));
      Serial.print(" | ");
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN:            Serial.print("open"); break;
        case WIFI_AUTH_WEP:             Serial.print("WEP"); break;
        case WIFI_AUTH_WPA_PSK:         Serial.print("WPA"); break;
        case WIFI_AUTH_WPA2_PSK:        Serial.print("WPA2"); break;
        case WIFI_AUTH_WPA_WPA2_PSK:    Serial.print("WPA+WPA2"); break;
        case WIFI_AUTH_WPA2_ENTERPRISE: Serial.print("WPA2-EAP"); break;
        case WIFI_AUTH_WPA3_PSK:        Serial.print("WPA3"); break;
        case WIFI_AUTH_WPA2_WPA3_PSK:   Serial.print("WPA2+WPA3"); break;
        case WIFI_AUTH_WAPI_PSK:        Serial.print("WAPI"); break;
        default:                        Serial.print("unknown");
      }
      Serial.println();
      delay(10);
    }
  }
  Serial.println("");

  // Delete the scan result to free memory for code below.
  WiFi.scanDelete();

  // Wait a bit before scanning again
  delay(2000);
}

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Scan WiFi networks with a minimum time of 100 ms per channel and a maximum time of 300 ms per channel (default values).
  wifiScan(100, 300);

  // Scan WiFi networks with a minimum time of 100 ms per channel and a maximum time of 1500 ms per channel.
  wifiScan(100, 1500);

  // Scan WiFi networks with a minimum time of 0 ms per channel and a maximum time of 1500 ms per channel.
  wifiScan(0, 1500);
}

void loop() {
  // Nothing to do here
}
