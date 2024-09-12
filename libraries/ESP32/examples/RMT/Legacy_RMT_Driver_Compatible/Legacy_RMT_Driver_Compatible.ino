/*
 * This example demonstrates how to use the file build_opt.h to disable the new RMT Driver
 * Note that this file shall be added the Arduino project
 *
 * If the content of this file changes, it is necessary to delete the compiled Arduino IDE cache
 * It can be done by changing, for instance, the "Core Debug Level" option, forcing the system to rebuild the Arduino Core
 *
 */

#ifndef ESP32_ARDUINO_NO_RGB_BUILTIN

// add the file "build_opt.h" to your Arduino project folder with "-DESP32_ARDUINO_NO_RGB_BUILTIN" to use the RMT Legacy driver
#error "ESP32_ARDUINO_NO_RGB_BUILTIN is not defined, this example is intended to demonstrate the RMT Legacy driver."
#error "Please add the file 'build_opt.h' with '-DESP32_ARDUINO_NO_RGB_BUILTIN' to your Arduino project folder."
#error "Another way to disable the RGB_BUILTIN is to define it in the platformio.ini file, for instance: '-D ESP32_ARDUINO_NO_RGB_BUILTIN'"

#else

// add the file "build_opt.h" to your Arduino project folder with "-DESP32_ARDUINO_NO_RGB_BUILTIN" to use the RMT Legacy driver
// rgbLedWrite() is a function that writes to the RGB LED and it won't be available here
#include "driver/rmt.h"

bool installed = false;

void setup() {
  Serial.begin(115200);
  Serial.println("This sketch is using the RMT Legacy driver.");
  installed = rmt_driver_install(RMT_CHANNEL_0, 0, 0) == ESP_OK;
}

void loop() {
  String msg = "RMT Legacy driver is installed: ";
  msg += (char *)(installed ? "Yes." : "No.");
  Serial.println(msg);
  delay(5000);
}

#endif  // ESP32_ARDUINO_NO_RGB_BUILTIN
