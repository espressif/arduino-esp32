// Matter Manager
#include <Matter.h>
#include <WiFi.h>

// List of Matter Endpoints for this Node
// On/Off Light Endpoint
#include <MatterOnOffLight.h>
MatterOnOffLight OnOffLight;

// set your board LED pin here
#ifdef LED_BUILTIN
const uint8_t ledPin = LED_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#warning "Do not forget to set the LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = 0; // Set your pin here. Using BOOT Button. C6/C3 use GPIO9.

// Matter Protocol Endpoint Callback
bool setLightOnOff(bool state) {
  Serial.printf("User Callback :: New Light State = %s\r\n", state ? "ON" : "OFF");
  if (state) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  // This callback must return the success state to Matter core
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

  // Initialize Matter EndPoint
  OnOffLight.begin(true);
  OnOffLight.onChangeOnOff(setLightOnOff);

  // Matter begining - Last step, after all EndPoints are initialized
  Matter.begin();
}

uint32_t lastMillis = millis();
const uint32_t debouceTime = 70; // button debounce time
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
    // Initializa the USER BUTTON (Boot button) GPIO that will act as a toggle switch
    pinMode(buttonPin, INPUT_PULLUP);
    // Initialize the LED (light) GPIO and Matter End Point
    pinMode(ledPin, OUTPUT);
    Serial.printf("Initial state: %s\r\n", OnOffLight.getOnOff() ? "ON" : "OFF");
    setLightOnOff(OnOffLight.getOnOff()); // configure the Light based on initial state
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
    delay(10000);
  }

  // Onboard User Button is used as a Light toggle switch
  if (digitalRead(buttonPin) == 0 && millis() - lastMillis > debouceTime) {
    Serial.println("User button pressed. Toggling Light!");
    lastMillis = millis();
    OnOffLight.toggle();  // Matter Controller also can see the change
  }
}