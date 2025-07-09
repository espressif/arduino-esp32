#define DISPLAY_ENABLED

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiAP.h>
#include <Update.h>
#include <Wire.h>
#ifdef DISPLAY_ENABLED
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel rgb_led_1 = Adafruit_NeoPixel(1, 1, NEO_GRB + NEO_KHZ800);

#endif
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

String ssid;
uint8_t mac[6];

// Create an instance of the server
WebServer server(80);
bool displayEnabled;

const int BUTTON_PIN = 0;                       // GPIO for the button
volatile unsigned long lastPressTime = 0;       // Time of last button press
volatile bool doublePressDetected = false;      // Flag for double press
const unsigned long doublePressInterval = 500;  // Max. time (in ms) between two presses for double press
volatile int pressCount = 0;                    // Counts the button presses

const unsigned char epd_bitmap_wifi[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x01, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x07, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x1f, 0xe0, 0xff, 0x00, 0x00,
  0x00, 0x3f, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x7c, 0x00, 0x03, 0xe0, 0x00, 0x00, 0xf0, 0x00, 0x01, 0xf0, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x78, 0x00,
  0x03, 0xc0, 0x00, 0x00, 0x38, 0x00, 0x07, 0x80, 0x00, 0x00, 0x1c, 0x00, 0x0f, 0x00, 0x06, 0x00, 0x0e, 0x00, 0x0e, 0x00, 0x7f, 0xe0, 0x0e, 0x00,
  0x0c, 0x01, 0xff, 0xf0, 0x06, 0x00, 0x00, 0x07, 0xff, 0xfc, 0x02, 0x00, 0x00, 0x0f, 0x80, 0x3e, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x0f, 0x00, 0x00,
  0x00, 0x1c, 0x00, 0x07, 0x80, 0x00, 0x00, 0x38, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x70, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x70, 0x00, 0x00, 0xc0, 0x00,
  0x00, 0x20, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xe0, 0x00, 0x00,
  0x00, 0x01, 0xe0, 0xf0, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x78, 0x00, 0x00, 0x00, 0x03, 0x80, 0x38, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'checkmark', 44x44px
const unsigned char epd_bitmap_checkmark[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x0e, 0x00, 0xf0, 0x00, 0x00,
  0x00, 0x0f, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x0f, 0x83, 0xc0, 0x00, 0x00, 0x00, 0x07, 0xc7, 0x80, 0x00, 0x00, 0x00, 0x03, 0xef, 0x00, 0x00, 0x00,
  0x00, 0x01, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void IRAM_ATTR handleButtonPress() {
  unsigned long currentTime = millis();  // Get current time

  // Debounce: If the current press is too close to the last one, ignore it
  if (currentTime - lastPressTime > 50) {
    pressCount++;  // Count the button press

    // Check if this is the second press within the double-press interval
    if (pressCount == 2 && (currentTime - lastPressTime <= doublePressInterval)) {
      doublePressDetected = true;  // Double press detected
      pressCount = 0;              // Reset counter
    }

    lastPressTime = currentTime;  // Update the time of the last press
  }
}

// Function to switch the boot partition to OTA1
void setBootPartitionToOTA0() {
  const esp_partition_t *ota0_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);

  if (ota0_partition) {
    // Set OTA1 as new boot partition
    esp_ota_set_boot_partition(ota0_partition);
    Serial.println("Boot partition changed to OTA0. Restarting...");

    // Restart to boot from the new partition
    esp_restart();
  } else {
    Serial.println("OTA1 partition not found!");
  }
}

void setupDisplay() {
  displayEnabled = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  if (displayEnabled) {
    display.display();
    delay(100);
    display.clearDisplay();
  }
}

void displayStatusBar(int progress) {
  display.clearDisplay();
  display.setCursor(24, 8);
  display.println("Sketch wird");
  display.setCursor(22, 22);
  display.println("hochgeladen!");

  display.fillRect(0, SCREEN_HEIGHT - 24, SCREEN_WIDTH - 4, 8, BLACK);  // Clear status bar area
  display.drawRect(0, SCREEN_HEIGHT - 24, SCREEN_WIDTH - 4, 8, WHITE);  // Draw border
  int filledWidth = (progress * SCREEN_WIDTH - 4) / 100;                // Calculate progress width
  display.fillRect(1, SCREEN_HEIGHT - 23, filledWidth - 4, 6, WHITE);   // Fill progress bar

  display.setCursor((SCREEN_WIDTH / 2) - 12, SCREEN_HEIGHT - 10);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.print(progress);
  display.println(" %");
  display.display();
}

void displayWelcomeScreen() {
  display.clearDisplay();

  // Draw WiFi symbol
  display.drawBitmap(0, 12, epd_bitmap_wifi, 44, 44, WHITE);

  // Display SSID text
  display.setCursor(40, 13);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.println("Verbinde dich");  // "Connect"
  display.setCursor(60, 27);
  display.println("mit:");  // "with"

  // Display SSID
  display.setCursor(40, 43);
  display.setTextSize(1);  // Larger text for SSID
  display.print(ssid);

  display.display();
}

void displaySuccessScreen() {
  display.clearDisplay();

  // Draw WiFi symbol
  display.drawBitmap(0, 12, epd_bitmap_checkmark, 44, 44, WHITE);

  // Display SSID text
  display.setCursor(48, 22);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.println("Erfolgreich");  // "Successfully"
  display.setCursor(48, 36);
  display.println("hochgeladen!");  // "uploaded!"

  display.display();
}

void wipeDisplay() {
  display.clearDisplay();
  display.println("");
  display.display();
}

void setupWiFi() {
  WiFi.macAddress(mac);
  char macLastFour[5];
  snprintf(macLastFour, sizeof(macLastFour), "%02X%02X", mac[4], mac[5]);
  ssid = "senseBox:" + String(macLastFour);

  // Define the IP address, gateway, and subnet mask
  IPAddress local_IP(192, 168, 1, 1);  // The new IP address
  IPAddress gateway(192, 168, 1, 1);   // Gateway address (can be the same as the AP's IP)
  IPAddress subnet(255, 255, 255, 0);  // Subnet mask

  // Set the IP address, gateway, and subnet mask of the access point
  WiFi.softAPConfig(local_IP, gateway, subnet);

  // Start the access point
  WiFi.softAP(ssid.c_str());
}

void setupOTA() {
  // Handle updating process
  server.on(
    "/sketch", HTTP_POST,
    []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload &upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        size_t fsize = UPDATE_SIZE_UNKNOWN;
        if (server.clientContentLength() > 0) {
          fsize = server.clientContentLength();
        }
        Serial.printf("Receiving Update: %s, Size: %d\n", upload.filename.c_str(), fsize);

        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(fsize)) {  //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        } else {
          int progress = (Update.progress() * 100) / Update.size();
          displayStatusBar(progress);  // Update progress on status bar
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  //true to set the size to the current progress
          displaySuccessScreen();
          delay(3000);
          wipeDisplay();
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    }
  );
}

void setup() {
  // Start Serial communication
  Serial.begin(115200);
  rgb_led_1.begin();
  rgb_led_1.setBrightness(30);
  rgb_led_1.setPixelColor(0, rgb_led_1.Color(51, 51, 255));
  rgb_led_1.show();

  // Configure button pin as input
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Interrupt for the button
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

#ifdef DISPLAY_ENABLED
  setupDisplay();
#endif
  setupWiFi();
  // Set the ESP32 as an access point
  setupOTA();
  server.begin();
}

void loop() {
  // Handle client requests
  server.handleClient();

#ifdef DISPLAY_ENABLED
  displayWelcomeScreen();
#endif

  if (doublePressDetected) {
    Serial.println("Doppeldruck erkannt!");  // "Double press detected!"
    setBootPartitionToOTA0();
#ifdef DISPLAY_ENABLED
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.println("");
    display.display();
    delay(50);
#endif
    // Restart to boot from the new partition
    esp_restart();
  }
}
