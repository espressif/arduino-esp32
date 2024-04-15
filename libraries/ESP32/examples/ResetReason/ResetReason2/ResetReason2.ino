//----------------------------------------------------------------------------------
//  ResetReason2.ino
//
//  Prints last reset reason of ESP32
//  This uses the mechanism in IDF that persists crash reasons across a reset.
//  See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#software-reset
//
//  This example uses esp_reset_reason() instead of rtc_get_reset_reason(0).
//  This example forces a WDT timeout in 10s.
//
//  Note enableLoopWDT() cannot be used here because it also takes care of
//  resetting WDT.  See cores/esp32/main.cpp::loopTask().
//
//  Author: David McCurley
//  Public Domain License
//----------------------------------------------------------------------------------
//  2023.04.21 r1.0 Initial release
//----------------------------------------------------------------------------------

#include "esp_task_wdt.h"

//Converts reason type to a C string.
//Type is located in /tools/sdk/esp32/include/esp_system/include/esp_system.h
const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN: return "Unknown";
    case ESP_RST_POWERON: return "PowerOn";    //Power on or RST pin toggled
    case ESP_RST_EXT: return "ExtPin";         //External pin - not applicable for ESP32
    case ESP_RST_SW: return "Reboot";          //esp_restart()
    case ESP_RST_PANIC: return "Crash";        //Exception/panic
    case ESP_RST_INT_WDT: return "WDT_Int";    //Interrupt watchdog (software or hardware)
    case ESP_RST_TASK_WDT: return "WDT_Task";  //Task watchdog
    case ESP_RST_WDT: return "WDT_Other";      //Other watchdog
    case ESP_RST_DEEPSLEEP: return "Sleep";    //Reset after exiting deep sleep mode
    case ESP_RST_BROWNOUT: return "BrownOut";  //Brownout reset (software or hardware)
    case ESP_RST_SDIO: return "SDIO";          //Reset over SDIO
    default: return "";
  }
}

void PrintResetReason(void) {
  esp_reset_reason_t r = esp_reset_reason();
  if (r == ESP_RST_POWERON) {
    delay(6000);  //Wait for serial monitor to connect
  }
  Serial.printf("\r\nReset reason %i - %s\r\n", r, resetReasonName(r));
}

void setup() {
  Serial.begin(115200);
  PrintResetReason();

  //Start WDT monitor
  if (esp_task_wdt_add(NULL) != ESP_OK) {
    log_e("Failed to add current task to WDT");
  }
}

//Enable esp_task_wdt_reset() below to prevent a WDT reset
void loop() {
  Serial.printf("Alive %lums\r\n", millis());
  //esp_task_wdt_reset();
  delay(1000);  //1s delay
}
