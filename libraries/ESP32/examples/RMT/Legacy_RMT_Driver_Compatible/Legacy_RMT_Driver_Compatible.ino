/*
 * This example demonstrates how to use the file build_opt.h to disable the new RMT Driver
 * Note that this file shall be added the Arduino project 
 * 
 * If the content of this file changes, it is necessary to delete the compiled Arduino IDE cache
 * It can be done by changing, for instance, the "Core Debug Level" option, forcing the system to rebuild the Arduino Core
 * 
 */

#ifndef ESP32_ARDUINO_NEW_RMT_DRV_OFF

// add the file "build_opt.h" to your Arduino project folder with "-DESP32_ARDUINO_NEW_RMT_DRV_OFF" to use the RMT Legacy driver
#warning "ESP32_ARDUINO_NEW_RMT_DRV_OFF is not defined, using new RMT driver"

#define RMT_PIN 4  // Valid GPIO for ESP32, S2, S3, C3, C6 and H2
bool installed = false;

void setup() {
  Serial.begin(115200);
  Serial.println("This sketch uses the new RMT driver.");
  installed = rmtInit(RMT_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
}

void loop() {
  String msg = "RMT New driver is installed: ";
  msg += (char*)(installed ? "Yes." : "No.");
  Serial.println(msg);
  delay(5000);
}

#else

// add the file "build_opt.h" to your Arduino project folder with "-DESP32_ARDUINO_NEW_RMT_DRV_OFF" to use the RMT Legacy driver
// neoPixelWrite() is a function that writes to the RGB LED and it won't be available here
#include "driver/rmt.h"

bool installed = false;

void setup() {
  Serial.begin(115200);
  Serial.println("This sketch is using the RMT Legacy driver.");
  installed = rmt_driver_install(RMT_CHANNEL_0, 0, 0) == ESP_OK;
}

void loop() {
  String msg = "RMT Legacy driver is installed: ";
  msg += (char*)(installed ? "Yes." : "No.");
  Serial.println(msg);
  delay(5000);
}

#endif // ESP32_ARDUINO_NEW_RMT_DRV_OFF