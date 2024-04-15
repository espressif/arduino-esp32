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

#include "soc/soc_caps.h"
#if SOC_RMT_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum {
    RMT_RX_MODE = 0,  // false
    RMT_TX_MODE = 1,  // true
  } rmt_ch_dir_t;

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

// Reading and Writing shall use as rmt_symbols_size this unit
// ESP32 has 8 MEM BLOCKS in total shared with Reading and/or Writing
// ESP32-S2 has 4 MEM BLOCKS in total shared with Reading and/or Writing
// ESP32-S3 has 4 MEM BLOCKS for Reading and another 4 MEM BLOCKS for Writing
// ESP32-C3 has 2 MEM BLOCKS for Reading and another 2 MEM BLOCKS for Writing
#define RMT_SYMBOLS_PER_CHANNEL_BLOCK SOC_RMT_MEM_WORDS_PER_CHANNEL

// Used to tell rmtRead() to wait for ever until reading data from the RMT channel
#define RMT_WAIT_FOR_EVER ((uint32_t)portMAX_DELAY)

// Helper macro to calculate the number of RTM symbols in a array or type
#define RMT_SYMBOLS_OF(x) (sizeof(x) / sizeof(rmt_data_t))

  /**
    Initialize the object

    New Parameters in Arduino Core 3: RMT tick is set in the rmtInit() function by the
    frequency of the RMT channel. Example: 100ns tick => 10MHz, thus frequency will be 10,000,000 Hz
    Returns <true> on execution success, <false> otherwise
*/
  bool rmtInit(int pin, rmt_ch_dir_t channel_direction, rmt_reserve_memsize_t memsize, uint32_t frequency_Hz);

  /**
     Sets the End of Transmission level to be set for the <pin> when the RMT transmission ends.
     This function affects how rmtWrite(), rmtWriteAsync() or rmtWriteLooping() will set the pin after writing the data.
     The default EOT level is LOW, in case this function isn't used before RMT Writing.
     This level can be set for each RMT pin and can be changed between writings to the same pin.

     <EOT_Level> shall be Zero (LOW) or non-zero (HIGH) value.
     It only affects the transmission process, therefore, it doesn't affect any IDLE LEVEL before starting the RMT transmission.
     The pre-transmission idle level can be set manually calling, for instance, digitalWrite(pin, Level).

     Returns <true> when EOT has been correctly set for <pin>, <false> otherwise.
*/
  bool rmtSetEOT(int pin, uint8_t EOT_Level);

  /**
     Sending data in Blocking Mode.
     <rmt_symbol> is a 32 bits structure as defined by rmt_data_t type.
     It is possible to use the macro RMT_SYMBOLS_OF(data), if data is an array of <rmt_data_t>.

     Blocking mode - only returns after sending all data or by timeout.
     If the writing operation takes longer than <timeout_ms> in milliseconds, it will end its
     execution returning <false>.
     Timeout can be set as undefined time by passing <RMT_WAIT_FOR_EVER> as <timeout_ms> parameter.
     When the operation is timed out, rmtTransmitCompleted() will return <false> until the transmission
     is finished, when rmtTransmitCompleted() will return <true>.

     Returns <true> when there is no error in the write operation, <false> otherwise, including when it
     exits by timeout.
*/
  bool rmtWrite(int pin, rmt_data_t *data, size_t num_rmt_symbols, uint32_t timeout_ms);

  /**
     Sending data in Async Mode.
     <rmt_symbol> is a 32 bits structure as defined by rmt_data_t type.
     It is possible to use the macro RMT_SYMBOLS_OF(data), if <data> is an array of <rmt_data_t>

     If more than one rmtWriteAsync() is executed in sequence, it will wait for the first transmission
     to finish, resulting in a return <false> that indicates that the rmtWriteAsync() call has failed.
     In such case, this channel will have to finish the previous transmission before starting a new one.

     Non-Blocking mode - returns right after execution.
     Returns <true> on execution success, <false> otherwise.

     <bool rmtTransmitCompleted(int pin)> will return <true> when all data is sent.
*/
  bool rmtWriteAsync(int pin, rmt_data_t *data, size_t num_rmt_symbols);

  /**
     Writing data up to the reserved memsize, looping continuously
     <rmt_symbol> is a 32 bits structure as defined by rmt_data_t type.
     It is possible to use the macro RMT_SYMBOLS_OF(data), if data is an array of rmt_data_t

     If *data or size_byte are NULL | Zero, it will disable the writing loop and stop transmission

     Non-Blocking mode - returns right after execution
     Returns <true> on execution success, <false> otherwise

     <bool rmtTransmitCompleted(int pin)> will return always <true> while it is looping.
*/
  bool rmtWriteLooping(int pin, rmt_data_t *data, size_t num_rmt_symbols);

  /**
     Checks if transmission is completed and the rmtChannel ready for transmitting new data.
     To be ready for a new transmission, means that the previous transmission is completed.
     Returns <true> when all data has been sent, <false> otherwise.
     The data transmission information is reset when a new rmtWrite/Async function is called.
     If rmtWrite() times out or rmtWriteAsync() is called, this function will return <false> until
     all data is sent out.
     rmtTranmitCompleted() will always return <true> when rmtWriteLooping() is called,
     because it has no effect in such case.
*/
  bool rmtTransmitCompleted(int pin);

  /**
     Initiates blocking receive. Read data will be stored in a user provided buffer <*data>
     It will read up to <num_rmt_symbols> RMT Symbols and the value of this variable will
     change to the effective number of symbols read.
     <rmt_symbol> is a 32 bits structure as defined by rmt_data_t type.

     If the reading operation takes longer than <timeout_ms> in milliseconds, it will end its
     execution and the function will return <false>. In a time out scenario, <num_rmt_symbols> won't
     change and rmtReceiveCompleted() can be used latter to check if there is data available.
     Timeout can be set as undefined time by passing RMT_WAIT_FOR_EVER as <timeout_ms> parameter

     Returns <true> when there is no error in the read operation, <false> otherwise, including when it
     exits by timeout.
     Returns, by value, the number of RMT Symbols read in <num_rmt_symbols> and the user buffer <data>
     when the read operation has success within the defined <timeout_ms>. If the function times out, it
     will read RMT data latter asynchronously, affecting <*data> and <*num_rmt_symbols>. After timeout,
     the application can check if data is already available using <rmtReceiveCompleted(int pin)>
*/
  bool rmtRead(int pin, rmt_data_t *data, size_t *num_rmt_symbols, uint32_t timeout_ms);

  /**
     Initiates async (non-blocking) receive. It will return immediately after execution.
     Read data will be stored in a user provided buffer <*data>.
     It will read up to <num_rmt_symbols> RMT Symbols and the value of this variable will
     change to the effective number of symbols read, whenever the read is completed.
     <rmt_symbol> is a 32 bits structure as defined by <rmt_data_t> type.

     Returns <true> when there is no error in the read operation, <false> otherwise.
     Returns asynchronously, by value, the number of RMT Symbols read, and also, it will copy
     the RMT received data to the user buffer <data> when the read operation happens.
     The application can check if data is already available using <rmtReceiveCompleted(int pin)>
*/
  bool rmtReadAsync(int pin, rmt_data_t *data, size_t *num_rmt_symbols);

  /**
     Checks if a data reception is completed and the rmtChannel has new data for processing.
     Returns <true> when data has been received, <false> otherwise.
     The data reception information is reset when a new rmtRead/Async function is called.
*/
  bool rmtReceiveCompleted(int pin);

  /**
   Function used to set a threshold (in ticks) used to consider that a data reception has ended.
   In receive mode, when no edge is detected on the input signal for longer than idle_thres_ticks
   time, the receiving process is finished and the Data is made available by
   the rmtRead/Async functions. Note that this time (in RMT channel frequency cycles) will also
   define how many low/high bits are read at the end of the received data.
   The function returns <true> if it is correctly executed, <false> otherwise.
*/
  bool rmtSetRxMaxThreshold(int pin, uint16_t idle_thres_ticks);

  /**
   Parameters changed in Arduino Core 3: low and high (ticks) are now expressed in Carrier Freq in Hz and
   duty cycle in percentage float 0.0 to 1.0 - example: 38.5KHz 33% High => 38500, 0.33

   Function to set a RX demodulation carrier or TX modulation carrier
    <carrier_en> is used to enable/disable the use of demodulation/modulation for RX/TX
    <carrier_level> true means that the polarity level for the (de)modulation is positive
    <frequency_Hz> is the carrier frequency used
    <duty_percent> is a float deom 0 to 1 (0.5 means a square wave) of the carrier frequency
   The function returns <true> if it is correctly executed, <false> otherwise.
*/
  bool rmtSetCarrier(int pin, bool carrier_en, bool carrier_level, uint32_t frequency_Hz, float duty_percent);

  /**
   Function used to filter input noise in the RX channel.
   In receiving mode, channel will ignore any input pulse which width (high or low)
   is smaller than <filter_pulse_ticks>
   If <filter_pulse_ns> is Zero, it will to disable the filter.
   The function returns <true> if it is correctly executed, <false> otherwise.
*/
  bool rmtSetRxMinThreshold(int pin, uint8_t filter_pulse_ticks);

  /**
   Deinitializes the driver and releases all allocated memory
   It also disables RMT for this gpio
*/
  bool rmtDeinit(int pin);

#ifdef __cplusplus
}
#endif

#endif /* SOC_RMT_SUPPORTED */
#endif /* MAIN_ESP32_HAL_RMT_H_ */
