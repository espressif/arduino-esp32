// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
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

#ifdef __cplusplus
extern "C" {
#endif

// notification flags
#define RMT_FLAG_TX_DONE     (1)
#define RMT_FLAG_RX_DONE     (2)
#define RMT_FLAG_ERROR       (4)
#define RMT_FLAGS_ALL        (RMT_FLAG_TX_DONE | RMT_FLAG_RX_DONE | RMT_FLAG_ERROR)

#define RMT_TX_MODE      true
#define RMT_RX_MODE      false

struct rmt_obj_s;

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

typedef struct rmt_obj_s rmt_obj_t;

typedef void (*rmt_rx_data_cb_t)(uint32_t *data, size_t len, void *arg);

typedef struct {
    union {
        struct {
            uint32_t duration0 :15;
            uint32_t level0 :1;
            uint32_t duration1 :15;
            uint32_t level1 :1;
        };
        uint32_t val;
    };
} rmt_data_t;


/**
*    Prints object information
*
*/
void _rmtDumpStatus(rmt_obj_t* rmt);

/**
*    Initialize the object
*
*/
rmt_obj_t* rmtInit(int pin, bool tx_not_rx, rmt_reserve_memsize_t memsize);

/**
*    Sets the clock/divider of timebase the nearest tick to the supplied value in nanoseconds
*    return the real actual tick value in ns
*/
float rmtSetTick(rmt_obj_t* rmt, float tick);

/**
*    Sending data in one-go mode or continual mode
*     (more data being send while updating buffers in interrupts)
*    Non-Blocking mode - returns right after executing
*/
bool rmtWrite(rmt_obj_t* rmt, rmt_data_t* data, size_t size);

/**
*    Sending data in one-go mode or continual mode
*     (more data being send while updating buffers in interrupts)
*    Blocking mode - only returns when data has been sent
*/
bool rmtWriteBlocking(rmt_obj_t* rmt, rmt_data_t* data, size_t size);

/**
*    Loop data up to the reserved memsize continuously
*
*/
bool rmtLoop(rmt_obj_t* rmt, rmt_data_t* data, size_t size);

/**
*    Initiates async receive, event flag indicates data received
*
*/
bool rmtReadAsync(rmt_obj_t* rmt, rmt_data_t* data, size_t size, void* eventFlag, bool waitForData, uint32_t timeout);

/**
*    Initiates async receive with automatic buffering
*    and callback with data from ISR
*
*/
bool rmtRead(rmt_obj_t* rmt, rmt_rx_data_cb_t cb, void * arg);

/***
 * Ends async receive started with rmtRead(); but does not
 * rmtDeInit().
 */
bool rmtEnd(rmt_obj_t* rmt);

/*  Additional interface */

/**
*    Start reception
*
*/
bool rmtBeginReceive(rmt_obj_t* rmt);

/**
*    Checks if reception completes
*
*/
bool rmtReceiveCompleted(rmt_obj_t* rmt);

/**
*    Reads the data for particular channel
*
*/
bool rmtReadData(rmt_obj_t* rmt, uint32_t* data, size_t size);

/**
 * Setting threshold for Rx completed
 */
bool rmtSetRxThreshold(rmt_obj_t* rmt, uint32_t value);

/**
 * Setting carrier
 */
bool rmtSetCarrier(rmt_obj_t* rmt, bool carrier_en, bool carrier_level, uint32_t low, uint32_t high);

/**
 * Setting input filter
 */
bool rmtSetFilter(rmt_obj_t* rmt, bool filter_en, uint32_t filter_level);

/**
 * Deinitialize the driver
 */
bool rmtDeinit(rmt_obj_t *rmt);

// TODO:
//  * uninstall interrupt when all channels are deinit
//  * send only-conti mode with circular-buffer
//  * put sanity checks to some macro or inlines
//  * doxy comments
//  * error reporting

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_RMT_H_ */
