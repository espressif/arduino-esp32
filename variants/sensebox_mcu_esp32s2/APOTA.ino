// APOTA is an Arduino fallback sketch that is written to OTA1_Partition.
// APOTA opens an access point which waits to receive a .bin file on /sketch.
// After successful upload, the file is written to OTA0_Partition, and the microcontroller reboots to the newly uploaded sketch.

#include <Arduino.h>
#define DISPLAY_ENABLED
// Prints per-upload timing (total, Update.write, display) to Serial.
#define OTA_TIMING_DEBUG

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
#if __has_include("esp_mac.h")
#include "esp_mac.h"  // esp_read_mac() moved here in ESP-IDF 5 (Arduino core 3.x)
#endif

String ssid;
uint8_t mac[6];

// Create an instance of the server
WebServer server(80);
bool displayEnabled = false;
const uint8_t DISPLAY_ADDRESS = 0x3D;

// Redraw the progress bar only every PROGRESS_STEP percent. Redrawing it for
// every ~1.4 KB upload chunk sends the whole frame buffer over I2C each time
// and slows the upload down considerably.
const int PROGRESS_STEP = 5;
int lastProgress = -1;
// Content-Length of the request, only used for the progress display. It
// includes the multipart overhead, so it is not passed to Update.begin().
size_t uploadExpectedSize = 0;
// Set only once Update.end() succeeded, so a request without a file or with
// an interrupted body is never answered with "OK"
bool updateSucceeded = false;

// Optional MD5 of the .bin file, sent by the app as "x-MD5" header. When
// present, Update.end() rejects a corrupted upload instead of flashing it.
const char *MD5_HEADER = "x-MD5";

#ifdef OTA_TIMING_DEBUG
unsigned long uploadStartMs = 0;
unsigned long writeTimeMs = 0;
unsigned long displayTimeMs = 0;
unsigned long chunkCount = 0;
#endif

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
    // A press after the double-press interval starts a new sequence,
    // otherwise a late second press would count towards the next one
    if (currentTime - lastPressTime > doublePressInterval) {
      pressCount = 0;
    }
    pressCount++;  // Count the button press

    if (pressCount == 2) {
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
  // display.begin() only fails when the frame buffer cannot be allocated, it
  // does not notice a missing display. Probe the I2C address instead, so no
  // time is wasted sending frames to a display that is not connected.
  Wire.begin();
  Wire.beginTransmission(DISPLAY_ADDRESS);
  bool displayFound = Wire.endTransmission() == 0;
  Serial.printf("Display %s\n", displayFound ? "found" : "not found, display output disabled");

  displayEnabled = displayFound && display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS);
  if (displayEnabled) {
    display.display();
    delay(100);
    display.clearDisplay();
  }
}

void displayStatusBar(int progress) {
  if (!displayEnabled) {
    return;
  }

  display.clearDisplay();
  display.setCursor(16, 8);
  display.println("Sketch is being");
  display.setCursor(32, 22);
  display.println("uploaded!");

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
  if (!displayEnabled) {
    return;
  }

  display.clearDisplay();

  // Draw WiFi symbol
  display.drawBitmap(0, 12, epd_bitmap_wifi, 44, 44, WHITE);

  // Display SSID text
  display.setCursor(52, 13);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.println("Connect");
  display.setCursor(60, 27);
  display.println("with:");

  // Display SSID
  display.setCursor(40, 43);
  display.setTextSize(1);  // Larger text for SSID
  display.print(ssid);

  display.display();
}

void displaySuccessScreen() {
  if (!displayEnabled) {
    return;
  }

  display.clearDisplay();

  // Draw WiFi symbol
  display.drawBitmap(0, 12, epd_bitmap_checkmark, 44, 44, WHITE);

  // Display SSID text
  display.setCursor(48, 22);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.println("Successfully");
  display.setCursor(48, 36);
  display.println("uploaded!");

  display.display();
}

void wipeDisplay() {
  if (!displayEnabled) {
    return;
  }

  display.clearDisplay();
  display.println("");
  display.display();
}

void setupWiFi() {
  // WiFi.macAddress() returns 00:00:00:00:00:00 on Arduino core 3.x while WiFi
  // is not started yet. Read the station MAC directly instead, so the SSID
  // stays the same as with older cores.
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
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

  // Disable WiFi power saving, it adds latency to every received packet
  WiFi.setSleep(false);
}

void setupOTA() {
  // Handle updating process
  server.on(
    "/sketch", HTTP_POST,
    []() {
      // Respond first so the app does not wait for the success screen
      bool failed = !updateSucceeded;
      server.sendHeader("Connection", "close");
      if (failed) {
        String error = Update.hasError() ? Update.errorString() : "Update incomplete";
        Serial.printf("Update failed: %s\n", error.c_str());
        server.send(500, "text/plain", String("FAIL: ") + error);
      } else {
        server.send(200, "text/plain", "OK");
      }
      // send() only queues the response. Give the TCP stack time to deliver it
      // and close the connection cleanly before ESP.restart() tears down the
      // access point, otherwise the app waits for a response that never comes.
      delay(500);
      server.client().stop();

      if (failed) {
        // Keep the access point open so the upload can simply be retried
        Update.abort();
        displayWelcomeScreen();
        return;
      }

#ifdef DISPLAY_ENABLED
      if (displayEnabled) {
        displaySuccessScreen();
        delay(3000);
        wipeDisplay();
      }
#endif
      ESP.restart();
    },
    []() {
      HTTPUpload &upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        lastProgress = -1;
        updateSucceeded = false;
#ifdef OTA_TIMING_DEBUG
        uploadStartMs = millis();
        writeTimeMs = 0;
        displayTimeMs = 0;
        chunkCount = 0;
#endif
        uploadExpectedSize = server.clientContentLength();
        Serial.printf("Receiving Update: %s, Request size: %u\n", upload.filename.c_str(), uploadExpectedSize);

        // A previous upload may have been interrupted without cleanup
        if (Update.isRunning()) {
          Update.abort();
        }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  //start with max available size
          Update.printError(Serial);
        } else if (server.hasHeader(MD5_HEADER)) {
          String md5 = server.header(MD5_HEADER);
          if (md5.length() == 32) {
            Update.setMD5(md5.c_str());
            Serial.printf("Expecting MD5: %s\n", md5.c_str());
          } else {
            Serial.printf("Ignoring invalid MD5 header: %s\n", md5.c_str());
          }
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
#ifdef OTA_TIMING_DEBUG
        chunkCount++;
        unsigned long t0 = millis();
#endif
        /* flashing firmware to ESP*/
        size_t written = Update.write(upload.buf, upload.currentSize);
#ifdef OTA_TIMING_DEBUG
        writeTimeMs += millis() - t0;
#endif
        if (written != upload.currentSize) {
          Update.printError(Serial);
        } else {
          int progress = uploadExpectedSize > 0 ? min(100, (int)((Update.progress() * 100) / uploadExpectedSize)) : 0;
          if (progress / PROGRESS_STEP != lastProgress / PROGRESS_STEP) {
            lastProgress = progress;
#ifdef DISPLAY_ENABLED
#ifdef OTA_TIMING_DEBUG
            unsigned long t1 = millis();
#endif
            displayStatusBar(progress);  // Update progress on status bar
#ifdef OTA_TIMING_DEBUG
            displayTimeMs += millis() - t1;
#endif
#endif
          }
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        // end() also verifies the MD5 if one was set
        updateSucceeded = Update.end(true);  //true to set the size to the current progress
        if (!updateSucceeded) {
          Update.printError(Serial);
        }
#ifdef OTA_TIMING_DEBUG
        unsigned long totalMs = millis() - uploadStartMs;
        Serial.printf(
          "OTA timing: %u bytes in %lu ms (%.1f KB/s), %lu chunks\n", upload.totalSize, totalMs, totalMs > 0 ? upload.totalSize / 1.024 / totalMs : 0.0,
          chunkCount
        );
        Serial.printf(
          "OTA timing: Update.write %lu ms, display %lu ms, rest (network/parser) %lu ms\n", writeTimeMs, displayTimeMs, totalMs - writeTimeMs - displayTimeMs
        );
#endif
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        // Connection dropped mid-upload. The response handler is not called
        // in this case, so clean up here to allow a new attempt.
        Serial.println("Upload aborted");
        Update.abort();
        displayWelcomeScreen();
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
#ifdef DISPLAY_ENABLED
  // Draw once here instead of in every loop() pass: each redraw blocks
  // handleClient() for the duration of an I2C frame transfer
  displayWelcomeScreen();
#endif
  // Set the ESP32 as an access point
  setupOTA();
  // WebServer only stores request headers that are registered up front
  const char *headerKeys[] = {MD5_HEADER};
  server.collectHeaders(headerKeys, 1);
  server.begin();
}

void loop() {
  // Handle client requests
  server.handleClient();

  if (doublePressDetected) {
    Serial.println("Double press detected!");
    // Restarts on success, only returns if OTA0 was not found
    setBootPartitionToOTA0();
    esp_restart();
  }
}
