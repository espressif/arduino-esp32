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

//#define INTERNAL_CONNECTIONS // uncomment to use without external connections

//#if SOC_I2S_NUM > 1

#include <I2S.h>
#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"

#include "soc/i2s_periph.h"
#include "soc/gpio_sig_map.h"

const long sampleRate = 8000;
const int bps = 8;
uint8_t *buffer;
uint8_t *buffer1;

const int signal_frequency = 1000; // frequency of square wave in Hz
const int signal_frequency1 = 2000; // frequency of square wave in Hz
const int amplitude = (1<<(bps-1))-1; // amplitude of square wave
const int halfWavelength = (sampleRate / signal_frequency); // half wavelength of square wave
const int halfWavelength1 = (sampleRate / signal_frequency1); // half wavelength of square wave
int32_t write_sample = amplitude; // current sample value
//int32_t write_sample = 165; // current sample value
int32_t write_sample1 = amplitude; // current sample value
int count = 0;
int count1 = 0;

void *write_buffer0;
size_t wr_buf_size0 = (bps/8)*halfWavelength*2;

void *write_buffer1;
size_t wr_buf_size1 = (bps/8)*halfWavelength1*2;

//#define TASK_PRIORITY 0 // only reads
//#define TASK_PRIORITY 1 // sometimes working ok; sometimes only writes
#define TASK_PRIORITY 2 // only writes

/*
static void I2S_write_task(void *args){
  while(true){
    if(I2S.availableForWrite() >= wr_buf_size0){
      Serial.printf("ok write because: I2S.availableForWrite() = %d\n", I2S.availableForWrite());
      I2S.write(write_buffer0, wr_buf_size0);
    }else{
      //Serial.printf("no write because: I2S.availableForWrite() = %d\n", I2S.availableForWrite());
    }
    delay(1);
  }
}

static void I2S_1_write_task(void *args){
  while(true){
    if(I2S_1.availableForWrite() >= wr_buf_size1){
      Serial.printf("ok write because: I2S_1.availableForWrite() = %d\n", I2S_1.availableForWrite());
      I2S_1.write(write_buffer1, wr_buf_size1);
    }else{
      Serial.printf("no write because: I2S_1.availableForWrite() = %d\n", I2S_1.availableForWrite());
    }
    delay(1);
  }
}
*/

void setup() {
  Serial.begin(115200, SERIAL_8N1, 16, 17);
  while(!Serial);

  Serial.printf("Try to init\nI2S: MCLK=%d, SCK=%d, FS=%d, SD=%d, OUT=%d, IN=%d\nI2S1: MCLK=%d, SCK=%d, FS=%d, SD=%d, OUT=%d, IN=%d\n",
  I2S.getMclkPin(), I2S.getSckPin(), I2S.getFsPin(), I2S.getDataPin(), I2S.getDataOutPin(), I2S.getDataInPin(),
  I2S_1.getMclkPin(), I2S_1.getSckPin(), I2S_1.getFsPin(), I2S_1.getDataPin(), I2S_1.getDataOutPin(), I2S_1.getDataInPin());

  I2S.setDMABufferFrameSize(32);
  //I2S.setDMABufferFrameSize(halfWavelength > 8 ? halfWavelength : 8);
  I2S_1.setDMABufferFrameSize(32);
  //I2S_1.setDMABufferFrameSize(halfWavelength1 > 8 ? halfWavelength1 : 8);

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

  Serial.printf("Initialized successfully\nI2S: MCLK=%d, SCK=%d, FS=%d, SD=%d, OUT=%d, IN=%d\nI2S1: MCLK=%d, SCK=%d, FS=%d, SD=%d, OUT=%d, IN=%d\n",
  I2S.getMclkPin(), I2S.getSckPin(), I2S.getFsPin(), I2S.getDataPin(), I2S.getDataOutPin(), I2S.getDataInPin(),
  I2S_1.getMclkPin(), I2S_1.getSckPin(), I2S_1.getFsPin(), I2S_1.getDataPin(), I2S_1.getDataOutPin(), I2S_1.getDataInPin());
  /*
  Serial.printf("Setup done; create tasks with priority %d\n", (int) TASK_PRIORITY);
  delay(1000);

  xTaskCreate(I2S_write_task, "I2S_write_task", 4096, NULL, (UBaseType_t) TASK_PRIORITY, NULL);
  xTaskCreate(I2S_1_write_task, "I2S_1_write_task", 4096, NULL, (UBaseType_t) TASK_PRIORITY, NULL);
  xTaskCreate(read_task, "read_task", 4096, NULL, (UBaseType_t) TASK_PRIORITY, NULL);
  Serial.println("Tasks created");
  */
  Serial.printf("write value (amplitude) = %d\n", write_sample);

  // create buffers
  write_buffer0 = malloc(wr_buf_size0);
  if(write_buffer0 == NULL){
    Serial.printf("ERROR creating write_buffer0; halt.");
    while(true);
  }
  write_buffer1 = malloc(wr_buf_size1);
  if(write_buffer1 == NULL){
    Serial.printf("ERROR creating write_buffer0; halt.");
    while(true);
  }

  for(int i = 0; i < halfWavelength*2; ++i){
    if(i % halfWavelength == 0 ) {
      write_sample = -1 * write_sample;
    }
    switch(bps){
      case 8:
        ((int8_t*)write_buffer0)[i] = write_sample;
        break;
      case 16:
        ((int16_t*)write_buffer0)[i] = write_sample;
        break;
      case 24:
      case 32:
        ((int32_t*)write_buffer0)[i] = write_sample;
        break;
    }
  }

  for(int i = 0; i < halfWavelength1*2; ++i){
    if(i % halfWavelength1 == 0 ) {
      write_sample1 = -1 * write_sample1;
    }
    switch(bps){
      case 8:
        ((int8_t*)write_buffer1)[i] = write_sample1;
        break;
      case 16:
        ((int16_t*)write_buffer1)[i] = write_sample1;
        break;
      case 24:
      case 32:
        ((int32_t*)write_buffer1)[i] = write_sample1;
        break;
    }
  }

/*
  xTaskCreate(I2S_write_task, "I2S_write_task", 4096, NULL, (UBaseType_t) TASK_PRIORITY, NULL);
  xTaskCreate(I2S_1_write_task, "I2S_1_write_task", 4096, NULL, (UBaseType_t) TASK_PRIORITY, NULL);
  Serial.println("Tasks created");
*/

#ifdef INTERNAL_CONNECTIONS
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_MCLK],   PIN_FUNC_GPIO);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SCK],    PIN_FUNC_GPIO);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_FS],     PIN_FUNC_GPIO);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SD_OUT], PIN_FUNC_GPIO);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SD_IN],  PIN_FUNC_GPIO);

  gpio_set_direction(PIN_I2S_MCLK,    GPIO_MODE_INPUT_OUTPUT);
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
}

int read_sample;
int read_sample1;

void loop() {
  if(I2S.availableForWrite() >= wr_buf_size0){
    //Serial.printf("ok write because: I2S.availableForWrite() = %d and the buffer is %d Bytes (wr_buf_size0)\n", I2S.availableForWrite(), wr_buf_size0);
    I2S.write(write_buffer0, wr_buf_size0);
  }
  if(I2S_1.availableForWrite() >= wr_buf_size1){
    //Serial.printf("ok write because: I2S_1.availableForWrite() = %d and the buffer is %d Bytes (wr_buf_size1)\n", I2S_1.availableForWrite(), wr_buf_size1);
    //Serial.printf("ok write because: I2S_1.availableForWrite() = %d\n", I2S_1.availableForWrite());
    I2S_1.write(write_buffer1, wr_buf_size1);
  }

  if(I2S.available()){
    read_sample = I2S.read();
    Serial.printf("Slave to master: 0x%X=%hhd\n", read_sample, (char)read_sample);
    //Serial.printf("1->0: 0x%X=%hhd\n", read_sample, (char)read_sample);
  }

  if(I2S_1.available()){
    read_sample1 = I2S_1.read();
    //Serial.printf("0->1: 0x%X=%hhd\n", read_sample1, (char)read_sample1);
    Serial.printf("Master to slave: 0x%X=%hhd\n", read_sample1, (char)read_sample1);
  }

//  if(read_sample && read_sample1){
/*
  if(read_sample || read_sample1){
    Serial.printf("1->0: 0x%X=%hhd; 0->1: 0x%X=%hhd\n", read_sample, (char)read_sample, read_sample1, (char)read_sample1);
  }
*/
}
/*
extern "C" void app_main(){
  setup();
  while(true){loop();}
}
*/
//#else
//  #error "This example cannot run on current SoC - the example requires two I2S modules found in ESP32 and ESP32-S3"
//#endif



