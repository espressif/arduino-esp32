/*
 This example is only for ESP
 This example demonstrates simultaneous usage of microphone and speaker using single I2S module.
 The application transfers data from input to output

 Circuit:
 * ESP32
   * GND connected GND
   * VIN connected 5V
   * SCK 5
   * FS 25
   * DIN 35
   * DOUT 26
 * I2S microphone
 * I2S decoder + headphones / speaker

 created 8 October 2021
 by Tomas Pilny
 */

#include <I2S.h>
const long sampleRate = 16000;
const int bitsPerSample = 32;
uint8_t *buffer;

void setup() {
  Serial.begin(115200);
  //I2S.setAllPins(5, 25, 35, 26); // you can change default pins; order of pins = (CLK, WS, IN, OUT)
  if(!I2S.setDuplex()){
    Serial.println("ERROR - could not set duplex");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  if (!I2S.begin(I2S_PHILIPS_MODE, sampleRate, bitsPerSample)) {
    Serial.println("Failed to initialize I2S!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  buffer = (uint8_t*) malloc(I2S.getBufferSize() * (bitsPerSample / 8));
  if(buffer == NULL){
    Serial.println("Failed to allocate buffer!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  Serial.println("Setup done");
}

void loop() {
  //I2S.write(I2S.read()); // primitive implementation sample-by-sample

  // Buffer based implementation
  I2S.read(buffer, I2S.getBufferSize() * (bitsPerSample / 8));
  I2S.write(buffer, I2S.getBufferSize() * (bitsPerSample / 8));

  //optimistic_yield(1000); // yield if last yield occurred before <parameter> CPU clock cycles ago
}
