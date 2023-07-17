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
const int frequency = 1000; // frequency of square wave in Hz
const int sampleRate = 8000; // sample rate in Hz
//const int sampleRate = 44100; // sample rate in Hz
const int bps = 16;
const int amplitude = (1<<(bps-1))-1; // amplitude of square wave

const int halfWavelength = (sampleRate / frequency); // half wavelength of square wave

int32_t sample = amplitude; // current sample value
int count = 0;

void *data;
const size_t data_elements = halfWavelength * 2 * CHANNEL_NUMBER;
const size_t data_len = (bps / 8) * data_elements;

i2s_mode_t mode = I2S_PHILIPS_MODE; // I2S decoder is needed
//i2s_mode_t mode = ADC_DAC_MODE; // bps must be 16; Audio amplifier is needed

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

  // Generate data for buffer write
  data = malloc(data_len);
  if(data == NULL){
    Serial.println("Failed to create data buffer!");
    while (1); // do nothing
  }

  int high;
  int low;
  switch(bps){
    case 8:
        high = (mode == I2S_PHILIPS_MODE) ? 0x7F : 0xFF;
        low = (mode == I2S_PHILIPS_MODE) ? 0x80 : 0x00;
        break;
      case 16:
        high = (mode == I2S_PHILIPS_MODE) ? 0x7FFF : 0xFFFF;
        low = (mode == I2S_PHILIPS_MODE) ? 0x8000 : 0x00;
        break;
      case 24:
        // 24 bps is stored in 32 bit data type on lower 3 Bytes (MSB is ignored)
        high = (mode == I2S_PHILIPS_MODE) ? 0x007FFFFF : 0x00FFFFFF;
        low = (mode == I2S_PHILIPS_MODE) ? 0x00800000 : 0x00;
        break;
      case 32:
        high = (mode == I2S_PHILIPS_MODE) ? 0x7FFFFFFF : 0xFFFFFFFF;
        low = (mode == I2S_PHILIPS_MODE) ? 0x80000000 : 0x00;
        break;
  }

  Serial.printf("Create data buffer for bps %d with values %d=0x%d for high and %d=0x%x for low\n", bps, high, high, low, low);
  for(int i = 0; i < data_elements; ++i){
    switch(bps){
      case 8:
        //((int8_t*)data)[i] = i <= data_elements/2 ? 0x7F : 0x80;
        //((int8_t*)data)[i] = i <= data_elements/2 ? 0xFF : 0x0;
        ((int8_t*)data)[i] = (int8_t)(i <= data_elements/2 ? high : low);
        break;
      case 16:
        //((int16_t*)data)[i] = i <= data_elements/2 ? 0x7FFF : 0x8000;
        //((int16_t*)data)[i] = i <= data_elements/2 ? 0xFFFF : 0x0;
        ((int16_t*)data)[i] = (int16_t)(i <= data_elements/2 ? high : low);
        break;
      case 24:
        // 24 bps is stored in 32 bit data type on lower 3 Bytes (MSB is ignored)
        //((int32_t*)data)[i] = i <= data_elements/2 ? 0x007FFFFF : 0x00800000;
        //((int32_t*)data)[i] = i <= data_elements/2 ? 0x00FFFFFF : 0x0;
        ((int32_t*)data)[i] = (int32_t)(i <= data_elements/2 ? high : low);
        break;
      case 32:
        //((int32_t*)data)[i] = i <= data_elements/2 ? 0x7FFFFFFF : 0x80000000;
        //((int32_t*)data)[i] = i <= data_elements/2 ? 0xFFFFFFFF : 0x0;
        ((int32_t*)data)[i] = (int32_t)(i <= data_elements/2 ? high : low);
        break;
    }
  }

  I2S.setDMABufferSampleSize(data_elements);
  Serial.printf("Set buffer sample size to %d\n", data_elements);

  // start I2S at the sample rate with 16-bits per sample
  if (!I2S.begin(mode, sampleRate, bps)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }
  //I2S.write(data, data_len);
  //I2S.write(data, data_len);
}

void loop() {
//size_t bytes_written;
//i2s_write(I2S_NUM_0, data, data_len, &bytes_written, portMAX_DELAY);

  int avail = I2S.availableForWrite();
  if(avail >= data_len){
    Serial.printf("avail for write = %d; data_len=%d\n", avail, data_len);
    I2S.write(data, data_len);
    //int written = I2S.write(data, data_len);
    //Serial.printf("written = %d\n", written);
  }else{
    //Serial.printf("data_len=%d cannot fit inside buffer of size%d => flush\n",data_len, avail);
    //I2S.flush();
    //TODO
    delay(1);
  }


  /*
    if (count % halfWavelength == 0 ) {
      // invert the sample every half wavelength count multiple to generate square wave
      sample = -1 * sample;
      Serial.printf("sample %d\n", sample);
    }

    I2S.write(sample); // Right channel
    I2S.write(sample); // Left channel

    // increment the counter for the next sample
    count++;
*/
}
