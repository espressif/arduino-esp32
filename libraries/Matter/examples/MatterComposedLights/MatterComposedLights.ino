// Matter Manager
#include <Matter.h>
#include <WiFi.h>

// List of Matter Endpoints for this Node
// There will be 3 On/Off Light Endpoints in the same Node
#include <MatterOnOffLight.h>
MatterOnOffLight OnOffLight1;
MatterOnOffLight OnOffLight2;
MatterOnOffLight OnOffLight3;

// Matter Protocol Endpoint Callback for each Light Accessory
bool setLightOnOff1(bool state) {
  Serial.printf("CB-Light1 changed state to: %s\r\n", state ? "ON" : "OFF");
  return true;
}

bool setLightOnOff2(bool state) {
  Serial.printf("CB-Light2 changed state to: %s\r\n", state ? "ON" : "OFF");
  return true;
}

bool setLightOnOff3(bool state) {
  Serial.printf("CB-Light3 changed state to: %s\r\n", state ? "ON" : "OFF");
  return true;
}

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

  // Initialize all 3 Matter EndPoints
  OnOffLight1.begin();
  OnOffLight2.begin();
  OnOffLight3.begin();
  OnOffLight1.onChangeOnOff(setLightOnOff1);
  OnOffLight2.onChangeOnOff(setLightOnOff2);
  OnOffLight3.onChangeOnOff(setLightOnOff3);

  // Matter begining - Last step, after all EndPoints are initialized
  Matter.begin();
}

void loop() {
  // Check Matter Light Commissioning state
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Light Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) { // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
  }

  //displays the Light state every 3 seconds
  Serial.println("======================");
  Serial.printf("Matter Light #1 is %s\r\n", OnOffLight1.getOnOff() ? "ON" : "OFF");
  Serial.printf("Matter Light #2 is %s\r\n", OnOffLight2.getOnOff() ? "ON" : "OFF");
  Serial.printf("Matter Light #3 is %s\r\n", OnOffLight3.getOnOff() ? "ON" : "OFF");
  delay(3000);
}