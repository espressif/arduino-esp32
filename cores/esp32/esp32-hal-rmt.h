// Copyright 2023 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MAIN_ESP32_HAL_RMT_H_
#define MAIN_ESP32_HAL_RMT_H_

// Rx Done Event
#define RMT_FLAG_RX_DONE (1)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  RMT_RX_MODE = 0,  // false
  RMT_TX_MODE = 1,  // true
} rmt_ch_dir_t;

typedef enum {
  RMT_MEM_64 =  1,
  RMT_MEM_128 = 2,
  RMT_MEM_192 = 3,
  RMT_MEM_256 = 4,
  RMT_MEM_320 = 5,
  RMT_MEM_384 = 6,
  RMT_MEM_448 = 7,
  RMT_MEM_512 = 8,
} rmt_reserve_memsize_t;

typedef union {
  struct {
    uint32_t duration0 : 15;
    uint32_t level0 : 1;
    uint32_t duration1 : 15;
    uint32_t level1 : 1;
  };
  uint32_t val;
} rmt_data_t;


/**
     Initialize the object
    
    New Parameters in Arduino Core 3: RMT tick is set in the rmtInit() function by the 
    frequency of the RMT channel. Example: 100ns tick => 10MHz, thus frequency will be 10,000,000 Hz
*/
bool rmtInit(int pin, rmt_ch_dir_t channel_direction, rmt_reserve_memsize_t memsize, uint32_t frequency_Hz);

/**
     BREAKING CHANGE in Arduino Core 3: rmtSetTick() was removed, in favor od rmtInit()

     Sets the clock/divider of timebase the nearest tick to the supplied value in nanoseconds
     return the real actual tick value in ns
*/
//float rmtSetTick(int pin, float tick);

/**
     Sending data in one-go mode or continual mode
      (more data being send while updating buffers in interrupts)
     Non-Blocking mode - returns right after executing
*/
bool rmtWrite(int pin, rmt_data_t* data, size_t size_byte);

/**
     Sending data in one-go mode or continual mode
      (more data being send while updating buffers in interrupts)
     Blocking mode - only returns when data has been sent
*/
bool rmtWriteBlocking(int pin, rmt_data_t* data, size_t size_byte);

/**
     Loop data up to the reserved memsize continuously

*/
bool rmtLoop(int pin, rmt_data_t* data, size_t size_byte);

/**
     Initiates async receive, event flag indicates data received

*/
bool rmtReadAsync(int pin, rmt_data_t* data, size_t size, void* eventFlag, bool waitForData, uint32_t timeout);

/***
   Ends async receive started with rmtRead(); but does not
   rmtDeInit().
*/
bool rmtEnd(int pin);

/*  Additional interface */

/**
     Start reception

*/
bool rmtBeginReceive(int pin);

/**
     Checks if reception completes

*/
bool rmtReceiveCompleted(int pin);

/**
     Reads the data for particular channel

*/
bool rmtReadData(int pin, uint32_t* data, size_t size);

/**
   Setting threshold for Rx completed
*/
bool rmtSetRxThreshold(int pin, uint16_t value);

/**
   Parameters changed in Arduino Core 3: low and high (ticks) are now expressed in Carrier Freq in Hz and
   duty cycle in percentage float 0.0 to 1.0 - example: 38.5KHz 33% High => 38500, 0.33
   Setting carrier
   
*/
//bool rmtSetCarrier(int pin, bool carrier_en, bool carrier_level, uint32_t low, uint32_t high);
bool rmtSetCarrier(int pin, bool carrier_en, bool carrier_level, uint32_t frequency_Hz, float duty_percent);
/**
   Setting input filter
*/
bool rmtSetFilter(int pin, bool filter_en, uint8_t filter_level);

/**
   Deinitialize the driver
*/
bool rmtDeinit(int pin);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_RMT_H_ */