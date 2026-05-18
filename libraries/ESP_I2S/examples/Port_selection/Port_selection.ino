/*
  This example selects a specific I2S controller before starting the driver.

  It selects I2S_NUM_0 explicitly. Use another SoC-specific controller such as
  I2S_NUM_1 when the target supports it.
*/

#include <Arduino.h>
#include <ESP_I2S.h>

#define I2S_LRC  25
#define I2S_BCLK 5
#define I2S_DIN  26

const i2s_port_t selectedPort = I2S_NUM_0;

const int frequency = 440;
const int amplitude = 500;
const int sampleRate = 8000;
const unsigned int halfWavelength = sampleRate / frequency / 2;

I2SClass i2s;
int32_t sample = amplitude;
unsigned int count = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("I2S port selection");

  if (!i2s.setPort(selectedPort)) {
    Serial.println("Failed to select I2S port!");
    while (1);
  }

  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DIN);
  if (!i2s.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("Failed to initialize I2S!");
    while (1);
  }

  Serial.printf("Allocated I2S port: %d\n", (int)i2s.getPort());
}

void loop() {
  if (count % halfWavelength == 0) {
    sample = -sample;
  }

  i2s.write(sample);
  i2s.write(sample >> 8);
  i2s.write(sample);
  i2s.write(sample >> 8);

  count++;
}
