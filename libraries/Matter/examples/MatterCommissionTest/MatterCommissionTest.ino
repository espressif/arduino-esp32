#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL
#include <Arduino.h>
#include <WiFi.h>

// Matter Manager
#include <Matter.h>

// List of Matter Endpoints for this Node
// On/Off Light Endpoint
#include <MatterOnOffLight.h>
MatterOnOffLight OnOffLight;

// WiFi is manually set and started

const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  // enable IPv6
  WiFi.enableIPv6(true);
  // Manually connect to WiFi
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

  // Initialize Matter Node
  Matter.begin();
  // Initialize Matter EndPoint
  OnOffLight.begin();
  // Matter start
  Matter.start();
}

void loop() {
  // Check Matter Commissioning state
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Light Commissioning.
    while (!Matter.isDeviceCommissioned()) {
      delay(5000);
      Serial.println("Matter Fabric not commissioned yet. Waiting for commissioning.");
    }
  }
  Serial.println("Matter Node is commissioned and connected to Wi-Fi.");
  Serial.println("====> Decommissioning in 30 seconds. <====");
  delay(30000);
  Matter.decommission();
  Serial.println("Matter Node is decommissioned. Commsssioning widget shall start over.");
}

#else
void setup() {}
void loop() {}
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */