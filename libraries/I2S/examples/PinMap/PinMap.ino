// This is actually some personal stuff, not actually example ...
// probably to search usable pins - hence the name


/*
 This example is only for ESP32 and ESP32-S3 which have two separate I2S modules
 This example demonstrates simultaneous usage of both I2S module.
 The application generates square wave for both modules and transfers to each other.
 You can plot the waves with Arduino plotter

 To prepare the example you will need to connect the output pins of the modules as if they were standalone devices.
 The pin-out differ for the SoCs - refer to the table of used SoC.

  ESP32
  | Pin  |  I2S  |   I2S_1  |
  | -----|-------|- --------|
  | SCK  |  18   |     22   |
  | FS   |  19   |     23   |
  | SD 1 |  21   |     35   | I2S DOUT -> I2S_1 DIN
  | SD 2 |  34   |     27   | I2S DIN  <- I2S_1 DOUT

  ESP32-S3
  | Pin  |  I2S  |   I2S_1  |
  | -----|-------|- --------|
  | SCK  |  18   |     36   |
  | FS   |  19   |     37   |
  | SD 1 |   4   |     40   | I2S DOUT -> I2S_1 DIN
  | SD 2 |   5   |     39   | I2S DIN  <- I2S_1 DOUT

 created 7 Nov 2022
 by Tomas Pilny
 */

#include <I2S.h>
#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"

#include "soc/i2s_periph.h"
#include "soc/gpio_sig_map.h"

//uint8_t ban_pins[] = {1,3,6,7,8,9,10,11,24,28,29,30,31}; // ESP32 WROVER-B
uint8_t ban_pins[] = {1,3,6,7,8,9,10,11,20,24,28,29,30,31}; // ESP32 WROVER-B
uint8_t data[] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};

void setup() {
  Serial.begin(115200);
  while(!Serial);

  if (!I2S.begin(I2S_PHILIPS_MODE, 8000, 8)) {
    Serial.println("Failed to initialize I2S!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  int ret;
  bool skip;
  I2S.setDataPin(21);
  int ban_size = sizeof(ban_pins) / sizeof(ban_pins[0]);
  Serial.println("Initial setup done");
  for(int pin = 22; pin < 34; ++pin){
    skip = false;
    Serial.printf("* * * * * * * * Try to set SCK pin to %d * * * * * * * *\n", pin);
    for(int i = 0; i < ban_size; ++i){
      if(pin == ban_pins[i]){
        Serial.printf("pin %d is on ban list - skip\n", pin);
        skip = true;
        break; // out of inner loop
      }
    }
    if(skip){ continue; }
    if(pin == I2S.getFsPin()) I2S.setFsPin(pin-1);
    if(pin == I2S.getDataPin()) I2S.setDataPin(pin-1);
    ret = I2S.setSckPin(pin);
    Serial.printf("Set pin returned %s\n", ret ? "OK" : "FAILED");
    Serial.printf("I2S: SCK=%d, FS=%d, SD=%d\n", I2S.getSckPin(), I2S.getFsPin(), I2S.getDataPin());
    uint8_t byte;
    while(!Serial.available()){
      I2S.write_blocking(data, sizeof(data) / sizeof(data[0]));
      //delay(10);
    }
    byte = Serial.read();
    Serial.printf("Serial received byte %d = char %c\n", byte, byte);
  }
}

void loop() {
  delay(1000);
}
