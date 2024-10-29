// Matter Manager
#include <Matter.h>
#include <WiFi.h>
#include <Preferences.h>

// List of Matter Endpoints for this Node
// Dimmable Light Endpoint
#include <MatterEndpoints/MatterDimmableLight.h>
MatterDimmableLight DimmableLight;

// it will keep last OnOff & Brightness state stored, using Preferences
Preferences lastStatePref;

// set your board RGB LED pin here
#ifdef RGB_BUILTIN
const uint8_t ledPin = RGB_BUILTIN;
#else
const uint8_t ledPin = 2;  // Set your pin here if your board has not defined LED_BUILTIN
#warning "Do not forget to set the RGB LED pin"
#endif

// set your board USER BUTTON pin here
const uint8_t buttonPin = 0;  // Set your pin here. Using BOOT Button. C6/C3 use GPIO9.

// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

// Set the RGB LED Light based on the current state of the Dimmable Light
void setRGBLight(bool state, uint8_t brightness) {
  Serial.printf("Setting Light to State: %s and Brightness: %d\r\n", DimmableLight ? "ON" : "OFF", brightness);
  if (state) {
    rgbLedWrite(ledPin, brightness, brightness, brightness);
  } else {
    digitalWrite(ledPin, LOW);
  }
}

// Matter Protocol Endpoint On-Off Change Callback
bool setLightOnOff(bool state) {
  Serial.printf("User Callback :: New Light State = %s\r\n", state ? "ON" : "OFF");
  setRGBLight(state, DimmableLight.getBrightness());
  // store last OnOff state for when the Light is restarted / power goes off
  lastStatePref.putBool("lastOnOffState", state);
  // This callback must return the success state to Matter core
  return true;
}

// Matter Protocol Endpoint Brightness Change Callback
bool setLightBrightness(uint8_t brightness) {
  Serial.printf("User Callback :: New Light Brigthness = %.2f%% [Val = %d]\r\n", ((float) brightness * 100) / MatterDimmableLight::MAX_BRIGHTNESS, brightness);
  setRGBLight(DimmableLight.getOnOff(), brightness);
  // store last Brightness for when the Light is restarted / power goes off
  lastStatePref.putUChar("lastBrightness", brightness);
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the LED (light) GPIO and Matter End Point
  pinMode(ledPin, OUTPUT);

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
  lastStatePref.begin("matterLight", false);
  bool lastOnOffState = lastStatePref.getBool("lastOnOffState", true);
  uint8_t lastBrightness = lastStatePref.getUChar("lastBrightness", 15); // default brightness = 12%
  DimmableLight.begin(lastOnOffState, lastBrightness);
  DimmableLight.onChangeOnOff(setLightOnOff);
  DimmableLight.onChangeBrightness(setLightBrightness);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
    Serial.printf("Initial state: %s | brightness: %d\r\n", DimmableLight ? "ON" : "OFF", DimmableLight.getBrightness());
    setLightOnOff(DimmableLight.getOnOff());            // configure the Light based on initial state
    setLightBrightness(DimmableLight.getBrightness());  // configure the Light based on initial brightness
  }
}
// Button control
uint32_t button_time_stamp = 0;                 // debouncing control
bool button_state = false;                      // false = released | true = pressed
const uint32_t debouceTime = 250;               // button debouncing time (ms)
const uint32_t decommissioningTimeout = 10000;  // keep the button pressed for 10s to decommission the light

void loop() {
  // Check Matter Light Commissioning state, which may change during execution of loop()
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
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    Serial.printf("Initial state: %s | brightness: %d\r\n", DimmableLight ? "ON" : "OFF", DimmableLight.getBrightness());
    setLightOnOff(DimmableLight.getOnOff());            // configure the Light based on initial state
    setLightBrightness(DimmableLight.getBrightness());  // configure the Light based on initial brightness
    Serial.println("Matter Node is commissioned and connected to Wi-Fi. Ready for use.");
  }

  // A button is also used to control the light
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used as a Light toggle switch or to decommission it
  uint32_t time_diff = millis() - button_time_stamp;
  if (button_state && time_diff > debouceTime && digitalRead(buttonPin) == HIGH) {
    button_state = false;  // released
    // Toggle button is released - toggle the light
    Serial.println("User button released. Toggling Light!");
    DimmableLight.toggle();  // Matter Controller also can see the change

    // Factory reset is triggered if the button is pressed longer than 10 seconds
    if (time_diff > decommissioningTimeout) {
      Serial.println("Decommissioning the Light Matter Accessory. It shall be commissioned again.");
      DimmableLight = false;  // turn the light off
      Matter.decommission();
    }
  }
}
