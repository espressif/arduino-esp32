/*
 * This sketch tests the deep sleep functionality of the ESP32
 */

#include <Arduino.h>

// Timer
#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5          /* Time ESP32 will go to sleep (in seconds) */

// Touchpad
#if CONFIG_IDF_TARGET_ESP32
#define THRESHOLD 1000   /* Greater the value, more the sensitivity */
#else
#define THRESHOLD 0      /* Lower the value, more the sensitivity */
#endif

// External wakeup
#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex
#define WAKEUP_GPIO              GPIO_NUM_33     // Only RTC IO are allowed - ESP32 Pin example

RTC_DATA_ATTR int bootCount = 0;

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  Serial.print("Wakeup reason: ");

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("rtc_io"); break;
    case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("rtc_cntl"); break;
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println("ulp"); break;
    default:                        Serial.println("power_up"); break;
  }
}

void setup_timer() {
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

void setup_touchpad() {
  #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
  touchSleepWakeUpEnable(T1, THRESHOLD);
  #else
  esp_sleep_enable_timer_wakeup(10);
  #endif
}

void setup_rtc_io() {
  #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 1);
  rtc_gpio_pullup_en(WAKEUP_GPIO);
  rtc_gpio_pulldown_dis(WAKEUP_GPIO);
  #else
  esp_sleep_enable_timer_wakeup(10);
  #endif
}

void setup_rtc_cntl() {
  #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
  esp_sleep_enable_ext1_wakeup_io(BUTTON_PIN_BITMASK(WAKEUP_GPIO), ESP_EXT1_WAKEUP_ANY_HIGH);
  rtc_gpio_pulldown_dis(WAKEUP_GPIO);
  rtc_gpio_pullup_en(WAKEUP_GPIO);
  #else
  esp_sleep_enable_timer_wakeup(10);
  #endif
}



void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();
  Serial.flush();

  if (bootCount == 1) {
    // Timer
    setup_timer();
  } else if (bootCount == 2) {
    // Touchpad
    setup_touchpad();
  } else if (bootCount == 3) {
    // rtc_io
    setup_rtc_io();
  } else if (bootCount == 4) {
    // rtc_cntl
    setup_rtc_cntl();
  }
  else {
    return;
  }

  esp_deep_sleep_start();
  Serial.println("This will never be printed");

}

void loop() {
  //This is not going to be called
}
