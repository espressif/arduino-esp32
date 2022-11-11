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
  | SCK  |  18   |     23   |
  | FS   |  19   |     25   |
  | SD 1 |  21   |     27   | I2S DOUT -> I2S_1 DIN
  | SD 2 |  22   |     26   | I2S DIN  <- I2S_1 DOUT

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

//#define INTERNAL_CONNECTIONS // uncomment to use without external connections

#if SOC_I2S_NUM > 1

#include <I2S.h>
#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"

#include "soc/i2s_periph.h"
#include "soc/gpio_sig_map.h"

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

#define DATA_MASTER_TO_SLAVE PIN_I2S_SD_OUT
#define DATA_SLAVE_TO_MASTER PIN_I2S_SD_IN

#if CONFIG_IDF_TARGET_ESP32
  #define I2S0_DATA_OUT_IDX   I2S0O_DATA_OUT23_IDX
  #define I2S0_DATA_IN_IDX   I2S0I_DATA_IN15_IDX
  #define I2S1_DATA_OUT_IDX   I2S1O_DATA_OUT23_IDX
  #define I2S1_DATA_IN_IDX   I2S1I_DATA_IN15_IDX
#elif CONFIG_IDF_TARGET_ESP32S3
  #define I2S0_DATA_OUT_IDX   I2S0O_SD_OUT_IDX
  #define I2S0_DATA_IN_IDX   I2S0I_SD_IN_IDX
  #define I2S1_DATA_OUT_IDX   I2S1O_SD_OUT_IDX
  #define I2S1_DATA_IN_IDX   I2S1I_SD_IN_IDX
#endif

void setup() {
  Serial.begin(115200);
  while(!Serial);

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
  buffer = (uint8_t*) malloc(I2S.getDMABufferByteSize());
  if(buffer == NULL){
    Serial.println("Failed to allocate buffer!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }

  if(!I2S_1.setDuplex()){
    Serial.println("ERROR - could not set duplex for I2S_1");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  if (!I2S_1.begin(I2S_PHILIPS_MODE, bps)) { // start in slave mode
    Serial.println("Failed to initialize I2S_1!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }
  buffer1 = (uint8_t*) malloc(I2S_1.getDMABufferByteSize());
  if(buffer1 == NULL){
    Serial.println("Failed to allocate buffer1!");
    while(true){
      vTaskDelay(10); // Cannot continue
    }
  }

#ifdef INTERNAL_CONNECTIONS
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SCK],    PIN_FUNC_GPIO);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_FS],     PIN_FUNC_GPIO);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SD_OUT], PIN_FUNC_GPIO);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SD_IN],  PIN_FUNC_GPIO);

  gpio_set_direction(PIN_I2S_SCK,    GPIO_MODE_INPUT_OUTPUT);
  gpio_set_direction(PIN_I2S_FS,     GPIO_MODE_INPUT_OUTPUT);
  gpio_set_direction(PIN_I2S_SD_OUT, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_direction(PIN_I2S_SD_IN,  GPIO_MODE_INPUT_OUTPUT);

  esp_rom_gpio_connect_out_signal(PIN_I2S_SCK, I2S0I_BCK_OUT_IDX, 0, 0);
  esp_rom_gpio_connect_in_signal(PIN_I2S1_SCK, I2S1O_BCK_IN_IDX, 0);

  esp_rom_gpio_connect_out_signal(PIN_I2S_FS, I2S0I_WS_OUT_IDX, 0, 0);
  esp_rom_gpio_connect_in_signal(PIN_I2S1_FS, I2S1O_WS_IN_IDX, 0);

  esp_rom_gpio_connect_out_signal(DATA_MASTER_TO_SLAVE, I2S0_DATA_OUT_IDX, 0, 0);
  esp_rom_gpio_connect_in_signal(DATA_MASTER_TO_SLAVE, I2S1_DATA_IN_IDX, 0);

  esp_rom_gpio_connect_out_signal(DATA_SLAVE_TO_MASTER, I2S1_DATA_OUT_IDX, 0, 0);
  esp_rom_gpio_connect_in_signal(DATA_SLAVE_TO_MASTER, I2S0_DATA_IN_IDX, 0);
#endif
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
  I2S_1.write(write_sample1); // Right channel
  I2S_1.write(write_sample1); // Left channel

  // increment the counter for the next sample
  count++;

  int read_sample = I2S.read();
  int read_sample1 = I2S_1.read();

  Serial.printf("%d %d\n", read_sample, read_sample1);
}

#else
  #error "This example cannot run on current SoC - the example requires two I2S modules found in ESP32 and ESP32-S3"
#endif
