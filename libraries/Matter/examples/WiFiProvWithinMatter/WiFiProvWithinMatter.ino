/*
  Please read README.md file in this folder, or on the web:
  https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiProv/examples/WiFiProv

  Note: This sketch takes up a lot of space for the app and may not be able to flash with default setting on some chips.
  If you see Error like this: "Sketch too big"
  In Arduino IDE go to: Tools > Partition scheme > chose anything that has more than 1.4MB APP
   - for example "No OTA (2MB APP/2MB SPIFFS)"

  This example demonstrates that it is possible to provision WiFi using BLE or Software AP using
  the ESP BLE Prov APP or ESP SoftAP Provisioning APP from Android Play or/and iOS APP Store

  Once the WiFi is provisioned, Matter will start its process as usual.

  This same Example could be used for any other WiFi Provisioning method.
*/

// Matter Manager
#include <Matter.h>
#include <WiFiProv.h>
#include <WiFi.h>

#if !CONFIG_BLUEDROID_ENABLED
#define USE_SOFT_AP  // ESP32-S2 has no BLE, therefore, it shall use SoftAP Provisioning
#endif
//#define USE_SOFT_AP // Uncomment if you want to enforce using the Soft AP method instead of BLE

const char *pop = "abcd1234";           // Proof of possession - otherwise called a PIN - string provided by the device, entered by the user in the phone app
const char *service_name = "PROV_123";  // Name of your device (the Espressif apps expects by default device name starting with "Prov_")
const char *service_key = NULL;         // Password used for SofAP method (NULL = no password needed)
bool reset_provisioned = true;          // When true the library will automatically delete previously provisioned data.

// List of Matter Endpoints for this Node
// Single On/Off Light Endpoint - at least one per node
MatterOnOffLight OnOffLight;

// Light GPIO that can be controlled by Matter APP
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#endif

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.

// Button control - decommision the Matter Node
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Matter Protocol Endpoint (On/OFF Light) Callback
bool matterCB(bool state) {
  digitalWrite(ledPin, state ? HIGH : LOW);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  pinMode(buttonPin, INPUT_PULLUP);

  Serial.begin(115200);
  // Initialize the LED GPIO
  pinMode(ledPin, OUTPUT);

  WiFi.begin();  // no SSID/PWD - get it from the Provisioning APP or from NVS (last successful connection)

  // BLE Provisioning using the ESP SoftAP Prov works fine for any BLE SoC, including ESP32, ESP32S3 and ESP32C3.
#if CONFIG_BLUEDROID_ENABLED && !defined(USE_SOFT_AP)
  Serial.println("Begin Provisioning using BLE");
  // Sample uuid that user can pass during provisioning using BLE
  uint8_t uuid[16] = {0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02};
  WiFiProv.beginProvision(
    NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BLE, NETWORK_PROV_SECURITY_1, pop, service_name, service_key, uuid, reset_provisioned
  );
  Serial.println("You may use this BLE QRCode:");
  WiFiProv.printQR(service_name, pop, "ble");
#else
  Serial.println("Begin Provisioning using Soft AP");
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_SOFTAP, NETWORK_PROV_SCHEME_HANDLER_NONE, NETWORK_PROV_SECURITY_1, pop, service_name, service_key);
  Serial.println("You may use this WiFi QRCode:");
  WiFiProv.printQR(service_name, pop, "softap");
#endif

  // Wait for WiFi connection
  uint32_t counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    // resets the device after 10 minutes
    if (counter > 2 * 60 * 10) {
      Serial.println("\r\n================================================");
      Serial.println("Already 10 minutes past. The device will reboot.");
      Serial.println("================================================\r\n");
      Serial.flush();  // wait until the Serial has sent the whole message.
      ESP.restart();
    }
    // WiFi searching feedback
    Serial.print(".");
    delay(500);
    // adds a new line every 30 seconds
    counter++;
    if (!(counter % 60)) {
      Serial.println();
    }
  }

  // WiFi shall be connected by now
  Serial.println();

  // Initialize at least one Matter EndPoint
  OnOffLight.begin();

  // Associate a callback to the Matter Controller
  OnOffLight.onChange(matterCB);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  while (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    Serial.println();
    // waits 30 seconds for Matter Commissioning, keeping it blocked until done
    delay(30000);
  }
}

void loop() {
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  if (digitalRead(buttonPin) == HIGH && button_state) {
    button_state = false;  // released
  }

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissining again, reboot takes a second or so
  }

  delay(500);
}
