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

// Reading and Writing shall use as rmt_symbols_size this unit
// ESP32 has 8 MEM BLOCKS in total shared with Reading and/or Writing
// ESP32-S2 has 4 MEM BLOCKS in total shared with Reading and/or Writing
// ESP32-S3 has 4 MEM BLOCKS for Reading and another 4 MEM BLOCKS for Writing
// ESP32-C3 has 2 MEM BLOCKS for Reading and another 2 MEM BLOCKS for Writing
#define RMT_SYMBOLS_PER_CHANNEL_BLOCK SOC_RMT_MEM_WORDS_PER_CHANNEL

typedef enum {
  RMT_MEM_NUM_BLOCKS_1 = 1,
  RMT_MEM_NUM_BLOCKS_2 = 2,
#if SOC_RMT_TX_CANDIDATES_PER_GROUP > 2
  RMT_MEM_NUM_BLOCKS_3 = 3,
  RMT_MEM_NUM_BLOCKS_4 = 4,
#if SOC_RMT_TX_CANDIDATES_PER_GROUP > 4
  RMT_MEM_NUM_BLOCKS_5 = 5,
  RMT_MEM_NUM_BLOCKS_6 = 6,
  RMT_MEM_NUM_BLOCKS_7 = 7,
  RMT_MEM_NUM_BLOCKS_8 = 8,
#endif
#endif
} rmt_reserve_memsize_t;

// Each RMT Symbols has 4 bytes
// Total number of bytes per RMT_MEM_BLOCK is RMT_SYMBOLS_PER_CHANNEL_BLOCK * 4 bytes
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
     Sending data in one-go mode or continual mode
     Blocking and non-blocking mode 
*/
bool rmtWrite(int pin, rmt_data_t* data, size_t num_rmt_symbols, bool blocking, bool loop);

/**
     Initiates async receive, event flag indicates data received

*/
bool rmtReadAsync(int pin, rmt_data_t* data, size_t *num_rmt_symbols, void* eventFlag, bool waitForData, uint32_t timeout_ms);

/**
   Setting threshold for Rx completed
*/
bool rmtSetRxThreshold(int pin, uint16_t value);

/**
   Parameters changed in Arduino Core 3: low and high (ticks) are now expressed in Carrier Freq in Hz and
   duty cycle in percentage float 0.0 to 1.0 - example: 38.5KHz 33% High => 38500, 0.33
   Setting carrier
   
*/
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