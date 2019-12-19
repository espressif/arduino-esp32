#include "esp_camera.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        // Time ESP32 will go to sleep (in seconds) */

// esp32 number denoting position
const int number = 6;

// The length of the complete cycle.
const int cycleLength = 1800;
// How long the camera remains on in each cycle
const int intervalOn = 60;

// How long the camera sleeps for each cycle
int intervalOff = cycleLength - intervalOn;
// how long after the start of the next cycle it will take to turn on given id Number
int stagger = (number - 1) * intervalOn;
// When the next cycle starts
int start;
// Time device turned on epoch time
int awoke;
// When the 32 will turn on next
int sleepAlarm;

const char* ssid = "***";
const char* password = "***";

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP);

int currentTime = 0; // What the current time is
int wokeUpAt = 0; // When the 32 last woke up

void startCameraServer();

IPAddress ip;

void OTA() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println("connection strong");
  } else {
    Serial.println("connection lost");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.println(WiFi.status());
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
      delay(2000);
    }
    Serial.println("reconnected!");
  }
  if (ip != WiFi.localIP()){
    ip = WiFi.localIP();
    Serial.println("new ip address");
    Serial.print(ip);
  }
  delay(500);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  //pinMode(sleepPin, INPUT);
  //esp_sleep_enable_ext0_wakeup(sleepGpioPin, 1);
  Serial.println("Morning!");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //config.pixel_format = PIXFORMAT_RAW;
  //init with high specs to pre-allocate larger buffers
  Serial.printf("psramFound(): ",psramFound());
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    Serial.printf("framesize UXGA");
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    Serial.printf("framesize SVGA");
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_XGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  timeClient.begin();
  timeClient.update();
  awoke = timeClient.getEpochTime();
  // Gets start of next cycle
  if(awoke % cycleLength != 0) {
    // start of next cycle equals time awoken + length of cycle minus time elapsed since start of last cycle
    start = awoke + cycleLength - (awoke % cycleLength);
  } else {
    // In case the system woke up exactly on it's previous starting time (Should only be possible for camera 1)
    start = awoke + cycleLength;
  }
  sleepAlarm = start + stagger;
}

void loop() {
  OTA();
  timeClient.update();
//  currentTime = timeClient.getEpochTime();
//  Serial.println(currentTime);
//  if(currentTime > awoke + intervalOn) {
//    Serial.println("Good Night!");
//    esp_sleep_enable_timer_wakeup((sleepAlarm - currentTime) * uS_TO_S_FACTOR);
//    esp_deep_sleep_start();
//  }
  delay(500);
}
