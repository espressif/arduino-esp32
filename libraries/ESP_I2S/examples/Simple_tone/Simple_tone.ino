/*
 This example generates a square wave based tone at a specified frequency
 and sample rate. Then outputs the data using the I2S interface to a
 MAX08357 I2S Amp Breakout board.
 I2S Circuit:
 * Arduino/Genuino Zero, MKR family and Nano 33 IoT
 * MAX08357:
   * GND connected GND
   * VIN connected 5V
   * LRC connected to pin 0 (Zero) or 3 (MKR), A2 (Nano) or 25 (ESP32)
   * BCLK connected to pin 1 (Zero) or 2 (MKR), A3 (Nano) or 5 (ESP32)
   * DIN connected to pin 9 (Zero) or A6 (MKR), 4 (Nano) or 26 (ESP32)
 DAC Circuit:
 * ESP32 or ESP32-S2
 * Audio amplifier
   - Note:
     - ESP32 has DAC on GPIO pins 25 and 26.
     - ESP32-S2 has DAC on GPIO pins 17 and 18.
  - Connect speaker(s) or headphones.
 created 17 November 2016
 by Sandeep Mistry
 For ESP extended
 Tomas Pilny
 2nd September 2021
 Lucas Saavedra Vaz (lucasssvaz)
 22nd December 2023
 */

#include <ESP_I2S.h>

const int frequency = 440;    // frequency of square wave in Hz
const int amplitude = 500;    // amplitude of square wave
const int sampleRate = 8000;  // sample rate in Hz

i2s_data_bit_width_t bps = I2S_DATA_BIT_WIDTH_16BIT;
i2s_mode_t mode = I2S_MODE_STD;
i2s_slot_mode_t slot = I2S_SLOT_MODE_STEREO;

const int halfWavelength = (sampleRate / frequency);  // half wavelength of square wave

int32_t sample = amplitude;  // current sample value
int count = 0;

I2SClass i2s;

void setup() {
  Serial.begin(115200);
  Serial.println("I2S simple tone");

  // start I2S at the sample rate with 16-bits per sample
  if (!i2s.begin(mode, sampleRate, bps, slot)) {
    Serial.println("Failed to initialize I2S!");
    while (1)
      ;  // do nothing
  }
}

void loop() {
  if (count % halfWavelength == 0) {
    // invert the sample every half wavelength count multiple to generate square wave
    sample = -1 * sample;
  }

  i2s.write(sample);  // Right channel
  i2s.write(sample);  // Left channel

  // increment the counter for the next sample
  count++;
}
