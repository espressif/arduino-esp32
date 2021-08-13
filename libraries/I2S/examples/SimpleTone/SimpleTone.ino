/*
 This example generates a square wave based tone at a specified frequency
 and sample rate. Then outputs the data using the I2S interface to a
 MAX08357 I2S Amp Breakout board.

 Circuit:
 * Arduino/Genuino Zero, MKR family and Nano 33 IoT
 * MAX08357:
   * GND connected GND
   * VIN connected 5V
   * LRC connected to pin 0 (Zero) or 3 (MKR), A2 (Nano) or 25 (ESP32)
   * BCLK connected to pin 1 (Zero) or 2 (MKR), A3 (Nano) or 5 (ESP32)
   * DIN connected to pin 9 (Zero) or A6 (MKR), 4 (Nano) or 26 (ESP32)

 created 17 November 2016
 by Sandeep Mistry
 */

#include <I2S.h>
const int frequency = 440; // frequency of square wave in Hz
const int amplitude = 500; // amplitude of square wave
const int sampleRate = 8000; // sample rate in Hz
const int bps = 16;

const int halfWavelength = (sampleRate / frequency); // half wavelength of square wave

short sample = amplitude; // current sample value
int count = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("I2S simple tone");

  // start I2S at the sample rate with 16-bits per sample
  if (!I2S.begin(I2S_PHILIPS_MODE, sampleRate, bps)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }
  I2S.setDataOutPin(26);
}

void loop() {
    if (count % halfWavelength == 0 ) {
      // invert the sample every half wavelength count multiple to generate square wave
      sample = -1 * sample;
    }

    // write the same sample twice, once for left and once for the right channel
    I2S.write(sample);
    I2S.write(sample);

    // increment the counter for the next sample
    count++;
}
