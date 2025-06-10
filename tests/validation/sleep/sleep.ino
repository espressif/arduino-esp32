/*
 * This sketch tests the deep sleep functionality of the ESP32
 */

#include <Arduino.h>
#include "driver/rtc_io.h"
#include "driver/uart.h"

// Timer
#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  1          /* Time ESP32 will go to sleep (in seconds) */

// Touchpad
#if CONFIG_IDF_TARGET_ESP32
#define THRESHOLD 1000   /* Greater the value, more the sensitivity */
#else
#define THRESHOLD 0      /* Lower the value, more the sensitivity */
#endif

#define TARGET_FREQ CONFIG_XTAL_FREQ

RTC_DATA_ATTR int boot_count = 0;
uint32_t orig_freq = 0;

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
    case ESP_SLEEP_WAKEUP_GPIO:     Serial.println("gpio"); break;
    case ESP_SLEEP_WAKEUP_UART:     Serial.println("uart"); break;
    default:                        Serial.println("power_up"); break;
  }
}

void setup_timer() {
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

void setup_touchpad() {
#if SOC_TOUCH_SENSOR_SUPPORTED
  touchSleepWakeUpEnable(T1, THRESHOLD);
#endif
}

void setup_uart() {
  uart_set_wakeup_threshold(UART_NUM_0, 9); // wake up with "aaa" string (9 positive edges)
  esp_sleep_enable_uart_wakeup(UART_NUM_0);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  orig_freq = getCpuFrequencyMhz();

  //Increment boot number and print it every reboot
  boot_count++;
  Serial.println("Boot number: " + String(boot_count));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();
  Serial.flush();
}

void loop() {
  // Disable all configured wakeup sources
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  if(Serial.available() > 0) {
    String command = Serial.readString();
    command.trim();

    if (command == "timer_deep") {
      // Test timer wakeup from deep sleep
      setup_timer();
      esp_deep_sleep_start();
      Serial.println("FAIL");
      while(1);
    } else if (command == "touchpad_deep") {
      // Test touchpad wakeup from deep sleep
      setup_touchpad();
      esp_deep_sleep_start();
      Serial.println("FAIL");
      while(1);
    } else if (command == "timer_light") {
      // Test timer wakeup from light sleep
      setup_timer();
    } else if (command == "timer_freq_light") {
      // Test timer wakeup from light sleep while changing CPU frequency
      setCpuFrequencyMhz(TARGET_FREQ);
      setup_timer();
    } else if (command == "touchpad_light") {
      // Test touchpad wakeup from light sleep
      setup_touchpad();
    } else if (command == "uart_light") {
      // Test external wakeup from light sleep using UART
      setup_uart();
    } else {
      Serial.println("FAIL");
      while(1);
    }

    esp_light_sleep_start();
    Serial.println("Woke up from light sleep");
    print_wakeup_reason();
    Serial.flush();
    setCpuFrequencyMhz(orig_freq);
  }
}
