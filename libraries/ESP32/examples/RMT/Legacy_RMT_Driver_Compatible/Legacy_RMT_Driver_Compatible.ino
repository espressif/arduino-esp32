#ifndef NO_NEW_RMT_DRV
// add the file "build_opt.h" to your Arduino project folder with "-DNO_NEW_RMT_DRV" to use the RMT Legacy driver
#warning "NO_NEW_RMT_DRV is not defined, using new RMT driver"

#define RMT_PIN 4  // Valid GPIO for ESP32, S2, S3, C3, C6 and H2
bool installed = false;

void setup() {
  Serial.begin(115200);
  Serial.println("This sketch uses the new RMT driver.");
  installed = rmtInit(RMT_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
}

void loop() {
  Serial.println("RMT New driver is installed: " + installed ? String("Yes.") : String("No."));
  delay(5000);
}

#else

// add the file "build_opt.h" to your Arduino project folder with "-DNO_NEW_RMT_DRV" to use the RMT Legacy driver
// neoPixelWrite() is a function that writes to the RGB LED and it won't be available here
#include "driver/rmt.h"

bool installed = false;

void setup() {
  Serial.begin(115200);
  Serial.println("This sketch is using the RMT Legacy driver.");
  installed = rmt_driver_install(RMT_CHANNEL_0, 0, 0) == ESP_OK;
}

void loop() {
  Serial.println("RMT Legacy driver is installed: " + installed ? String("Yes.") : String("No."));
  delay(5000);
}


#endif