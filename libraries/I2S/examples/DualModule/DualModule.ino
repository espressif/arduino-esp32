/*
 This example is only for ESP32 and ESP32-S3 which have two separate I2S modules
 This example demonstrates simultaneous usage of both I2S module.
 The application generates square wave for both modules and transfers to each other.
 You can plot the waves with Arduino plotter

    I2S
  | Pin  | ESP32 | ESP32-S3 |
  | -----|-------|- --------|
  | GND  |  GND  |    GND   |
  | VIN  |  5V   |     5V   |
  | SCK  |  19   |     19   |
  | FS   |  21   |     21   |
  | DIN  |  23   |      5   |
  | DOUT |  22   |      4   |

    I2S1
  | Pin  | ESP32 | ESP32-S3 |
  | -----|-------|- --------|
  | GND  |  GND  |    GND   |
  | VIN  |  5V   |     5V   |
  | SCK  |  18   |     36   |
  | FS   |  22   |     37   |
  | DIN  |  26   |     40   |
  | DOUT |  25   |     39   |

 created 18 October 2022
 by Tomas Pilny
 */

#include <I2S.h>
const long sampleRate = 16000;
const int bps = 32;
uint8_t *buffer;
uint8_t *buffer1;


const int signal_frequency = 1000; // frequency of square wave in Hz
const int signal_frequency1 = 2000; // frequency of square wave in Hz
const int amplitude = (1<<(bps-1))-1; // amplitude of square wave
const int halfWavelength = (sampleRate / signal_frequency); // half wavelength of square wave
const int halfWavelength1 = (sampleRate / signal_frequency1); // half wavelength of square wave
int32_t write_sample = amplitude; // current sample value
int32_t write_sample1 = amplitude; // current sample value
int count = 0;


void setup() {
  Serial.begin(115200);
  while(!Serial);
  I2S.setAllPins(PIN_I2S1_SCK, PIN_I2S1_FS, PIN_I2S1_SD, PIN_I2S1_SD_OUT, PIN_I2S1_SD_IN); // set all pins to match I2S1 to connect them internally
  if(!I2S.setDuplex()){
    Serial.println("ERROR - could not set duplex for I2S");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  if (!I2S.begin(I2S_PHILIPS_MODE, sampleRate, bps)) {
    Serial.println("Failed to initialize I2S!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  buffer = (uint8_t*) malloc(I2S.getBufferSize() * (bps / 8));
  if(buffer == NULL){
    Serial.println("Failed to allocate buffer!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }

  if(!I2S1.setDuplex()){
    Serial.println("ERROR - could not set duplex for I2S1");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  if (!I2S1.begin(I2S_PHILIPS_MODE, bps)) { // start in slave mode  --- CRASHING!
    Serial.println("Failed to initialize I2S1!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  buffer1 = (uint8_t*) malloc(I2S1.getBufferSize() * (bps / 8));
  if(buffer1 == NULL){
    Serial.println("Failed to allocate buffer1!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  Serial.println("Setup done");
}

void loop() {
  // invert the sample every half wavelength count multiple to generate square wave
  if (count % halfWavelength == 0 ) {
    write_sample = -1 * write_sample;
  }
  I2S.write(write_sample); // Right channel
  I2S.write(write_sample); // Left channel
  if (count % halfWavelength1 == 0 ) {
    write_sample1 = -1 * write_sample1;
  }
  I2S1.write(write_sample1); // Right channel
  I2S1.write(write_sample1); // Left channel

  // increment the counter for the next sample
  count++;


  int read_sample = I2S.read();
  int read_sample1 = I2S1.read();

  Serial.printf("%d %d", read_sample, read_sample1);
  Serial.printf("%d", read_sample1);
}