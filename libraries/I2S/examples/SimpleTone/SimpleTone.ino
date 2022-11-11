/*
 This example generates a square wave based tone at a specified frequency
 and sample rate. Then outputs the data using the I2S interface to a
 MAX08357 I2S Amp Breakout board.

 I2S Circuit:
 * Arduino/Genuino Zero, MKR family, Nano 33 IoT or any ESP32
 * MAX08357 or PCM510xA:
  | Pin  | Zero  |  MKR  | Nano  | ESP32 | ESP32-S2, ESP32-C3, ESP32-S3 |
  | -----|-------|-------|-------|-------|------------------------------|
  | GND  |  GND  |  GND  |  GND  |  GND  |            GND               |
  | 5V   |  5V   |  5V   |  5V   |  5V   |            5V                |
  | SCK  |   1   |   2   |  A3   |  18   |            18                |
  | FS   |   0   |   3   |  A2   |  19   |            19                |
  | SD   |   9   |  A6   |   4   |  21   |             4                |
 * note: those chips supports only 16/24/32 bits per sample, i.e. 8 bps will be refused = no audio output

 DAC Circuit:
 * ESP32
 * Audio amplifier
   - Note:
     - ESP32 has DAC on GPIO pins 25 and 26.
  - Connect speaker(s) or headphones.

 created 17 November 2016
 by Sandeep Mistry
 For ESP extended
 Tomas Pilny
 2nd September 2021
 */

#include <I2S.h>
const int frequency = 440; // frequency of square wave in Hz
const int sampleRate = 8000; // sample rate in Hz
const int bps = 16;
const int amplitude = (1<<(bps-1))-1; // amplitude of square wave

const int halfWavelength = (sampleRate / frequency); // half wavelength of square wave

int32_t sample = amplitude; // current sample value
int count = 0;

i2s_mode_t mode = I2S_PHILIPS_MODE; // I2S decoder is needed
//i2s_mode_t mode = ADC_DAC_MODE; // Audio amplifier is needed

// Mono channel input
// This is ESP specific implementation -
//   samples will be automatically copied to both channels inside I2S driver
//   If you want to have true mono output use I2S_PHILIPS_MODE and interlay
//   second channel with 0-value samples.
//   The order of channels is RIGH followed by LEFT
//i2s_mode_t mode = I2S_RIGHT_JUSTIFIED_MODE; // I2S decoder is needed

void setup() {
  Serial.begin(115200);
  Serial.println("I2S simple tone");

  // start I2S at the sample rate with 16-bits per sample
  if (!I2S.begin(mode, sampleRate, bps)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }
}

void loop() {
    if (count % halfWavelength == 0 ) {
      // invert the sample every half wavelength count multiple to generate square wave
      sample = -1 * sample;
    }

    if(mode == I2S_PHILIPS_MODE || mode == ADC_DAC_MODE){ // write the same sample twice, once for Right and once for Left channel
      I2S.write(sample); // Right channel
      I2S.write(sample); // Left channel
    }else if(mode == I2S_RIGHT_JUSTIFIED_MODE || mode == I2S_LEFT_JUSTIFIED_MODE){
      // write the same only once - it will be automatically copied to the other channel
      I2S.write(sample);
    }

    // increment the counter for the next sample
    count++;
}
