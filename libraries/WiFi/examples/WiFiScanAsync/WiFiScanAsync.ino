/*
    This sketch demonstrates how to scan WiFi networks in Async Mode.
    The API is based on the Arduino WiFi Shield library, but has significant changes as newer WiFi functions are supported.
    E.g. the return value of `encryptionType()` different because more modern encryption is supported.
*/
#include "WiFi.h"

void startWiFiScan() {
  Serial.println("Scan start");
  // WiFi.scanNetworks will return immediately in Async Mode.
  WiFi.scanNetworks(true);  // 'true' turns Async Mode ON
}

void printScannedNetworks(uint16_t networksFound) {
  if (networksFound == 0) {
    Serial.println("no networks found");
  } else {
    Serial.println("\nScan done");
    Serial.print(networksFound);
    Serial.println(" networks found");
    Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
    for (int i = 0; i < networksFound; ++i) {
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
        case WIFI_AUTH_OPEN:
          Serial.print("open");
          break;
        case WIFI_AUTH_WEP:
          Serial.print("WEP");
          break;
        case WIFI_AUTH_WPA_PSK:
          Serial.print("WPA");
          break;
        case WIFI_AUTH_WPA2_PSK:
          Serial.print("WPA2");
          break;
        case WIFI_AUTH_WPA_WPA2_PSK:
          Serial.print("WPA+WPA2");
          break;
        case WIFI_AUTH_WPA2_ENTERPRISE:
          Serial.print("WPA2-EAP");
          break;
        case WIFI_AUTH_WPA3_PSK:
          Serial.print("WPA3");
          break;
        case WIFI_AUTH_WPA2_WPA3_PSK:
          Serial.print("WPA2+WPA3");
          break;
        case WIFI_AUTH_WAPI_PSK:
          Serial.print("WAPI");
          break;
        default:
          Serial.print("unknown");
      }
      Serial.println();
      delay(10);
    }
    Serial.println("");
    // Delete the scan result to free memory for code below.
    WiFi.scanDelete();
  }
}

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("Setup done");
  startWiFiScan();
}

void loop() {
  // check WiFi Scan Async process
  int16_t WiFiScanStatus = WiFi.scanComplete();
  if (WiFiScanStatus < 0) {  // it is busy scanning or got an error
    if (WiFiScanStatus == WIFI_SCAN_FAILED) {
      Serial.println("WiFi Scan has failed. Starting again.");
      startWiFiScan();
    }
    // other option is status WIFI_SCAN_RUNNING - just wait.
  } else {  // Found Zero or more Wireless Networks
    printScannedNetworks(WiFiScanStatus);
    startWiFiScan();  // start over...
  }

  // Loop can do something else...
  delay(250);
  Serial.println("Loop running...");
}
