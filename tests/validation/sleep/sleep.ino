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

// External wakeup
#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex

#if CONFIG_IDF_TARGET_ESP32H2
#define WAKEUP_GPIO              GPIO_NUM_7     // Only RTC IO are allowed
#define TARGET_FREQ              32
#else
#define WAKEUP_GPIO              GPIO_NUM_4     // Only RTC IO are allowed
#define TARGET_FREQ              40
#endif

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

void setup_rtc_io() {
#if SOC_RTCIO_WAKE_SUPPORTED && SOC_PM_SUPPORT_EXT0_WAKEUP
  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 1);
  rtc_gpio_pullup_en(WAKEUP_GPIO);
  rtc_gpio_pulldown_dis(WAKEUP_GPIO);
#endif
}

void setup_rtc_cntl() {
#if SOC_RTCIO_WAKE_SUPPORTED && SOC_PM_SUPPORT_EXT1_WAKEUP
  esp_sleep_enable_ext1_wakeup_io(BUTTON_PIN_BITMASK(WAKEUP_GPIO), ESP_EXT1_WAKEUP_ANY_HIGH);
  rtc_gpio_pulldown_dis(WAKEUP_GPIO);
  rtc_gpio_pullup_en(WAKEUP_GPIO);
#endif
}

void setup_gpio() {
  esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
  gpio_pullup_dis(WAKEUP_GPIO);
  gpio_pulldown_en(WAKEUP_GPIO);
  gpio_wakeup_enable(WAKEUP_GPIO, GPIO_INTR_HIGH_LEVEL);
  esp_sleep_enable_gpio_wakeup();
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
    } else if (command == "rtc_io_deep") {
      // Test external wakeup from deep sleep using RTC IO
      setup_rtc_io();
      esp_deep_sleep_start();
      Serial.println("FAIL");
      while(1);
    } else if (command == "rtc_cntl_deep") {
      // Test external wakeup from deep sleep using RTC controller
      setup_rtc_cntl();
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
    } else if (command == "rtc_io_light") {
      // Test external wakeup from light sleep using RTC IO
      setup_rtc_io();
    } else if (command == "rtc_cntl_light") {
      // Test external wakeup from light sleep using RTC controller
      setup_rtc_cntl();
    } else if (command == "gpio_light") {
      // Test external wakeup from light sleep using GPIO
      setup_gpio();
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
    gpio_hold_dis(WAKEUP_GPIO);
    setCpuFrequencyMhz(orig_freq);
  }
}
