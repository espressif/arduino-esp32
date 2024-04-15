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

#include "soc/soc_caps.h"

#if SOC_RMT_SUPPORTED
#include "esp32-hal.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "hal/rmt_ll.h"

#include "esp32-hal-rmt.h"
#include "esp32-hal-periman.h"
#include "esp_idf_version.h"

// Arduino Task Handle indicates if the Arduino Task has been started already
extern TaskHandle_t loopTaskHandle;

// RMT Events
#define RMT_FLAG_RX_DONE (1)
#define RMT_FLAG_TX_DONE (2)

/**
   Internal macros
*/

#if CONFIG_DISABLE_HAL_LOCKS
#define RMT_MUTEX_LOCK(busptr)
#define RMT_MUTEX_UNLOCK(busptr)
#else
#define RMT_MUTEX_LOCK(busptr) \
  do { \
  } while (xSemaphoreTake(busptr->g_rmt_objlocks, portMAX_DELAY) != pdPASS)
#define RMT_MUTEX_UNLOCK(busptr) xSemaphoreGive(busptr->g_rmt_objlocks)
#endif /* CONFIG_DISABLE_HAL_LOCKS */


/**
   Typedefs for internal structures, enums
*/

struct rmt_obj_s {
  // general RMT information
  rmt_channel_handle_t rmt_channel_h;       // IDF RMT channel handler
  rmt_encoder_handle_t rmt_copy_encoder_h;  // RMT simple copy encoder handle

  uint32_t signal_range_min_ns;  // RX Filter data - Low Pass pulse width
  uint32_t signal_range_max_ns;  // RX idle time that defines end of reading

  EventGroupHandle_t rmt_events;  // read/write done event RMT callback handle
  bool rmt_ch_is_looping;         // Is this RMT TX Channel in LOOPING MODE?
  size_t *num_symbols_read;       // Pointer to the number of RMT symbol read by IDF RMT RX Done
  uint32_t frequency_Hz;          // RMT Frequency
  uint8_t rmt_EOT_Level;          // RMT End of Transmission Level - default is LOW

#if !CONFIG_DISABLE_HAL_LOCKS
  SemaphoreHandle_t g_rmt_objlocks;  // Channel Semaphore Lock
#endif                               /* CONFIG_DISABLE_HAL_LOCKS */
};

typedef struct rmt_obj_s *rmt_bus_handle_t;

/**
   Internal variables used in RMT API
*/
static SemaphoreHandle_t g_rmt_block_lock = NULL;

/**
   Internal method (private) declarations
*/

// This is called from an IDF ISR code, therefore this code is part of an ISR
static bool _rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *data, void *args) {
  BaseType_t high_task_wakeup = pdFALSE;
  rmt_bus_handle_t bus = (rmt_bus_handle_t)args;
  // sets the returning number of RMT symbols (32 bits) effectively read
  *bus->num_symbols_read = data->num_symbols;
  // set RX event group and signal the received RMT symbols of that channel
  xEventGroupSetBitsFromISR(bus->rmt_events, RMT_FLAG_RX_DONE, &high_task_wakeup);
  // A "need to yield" is returned in order to execute portYIELD_FROM_ISR() in the main IDF RX ISR
  return high_task_wakeup == pdTRUE;
}

// This is called from an IDF ISR code, therefore this code is part of an ISR
static bool _rmt_tx_done_callback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *data, void *args) {
  BaseType_t high_task_wakeup = pdFALSE;
  rmt_bus_handle_t bus = (rmt_bus_handle_t)args;
  // set RX event group and signal the received RMT symbols of that channel
  xEventGroupSetBitsFromISR(bus->rmt_events, RMT_FLAG_TX_DONE, &high_task_wakeup);
  // A "need to yield" is returned in order to execute portYIELD_FROM_ISR() in the main IDF RX ISR
  return high_task_wakeup == pdTRUE;
}

// This function must be called only after checking the pin and its bus with _rmtGetBus()
static bool _rmtCheckDirection(uint8_t gpio_num, rmt_ch_dir_t rmt_dir, const char *labelFunc) {
  // gets bus RMT direction from the Peripheral Manager information
  rmt_ch_dir_t bus_rmt_dir = perimanGetPinBusType(gpio_num) == ESP32_BUS_TYPE_RMT_TX ? RMT_TX_MODE : RMT_RX_MODE;

  if (bus_rmt_dir == rmt_dir) {  // matches expected RX/TX channel
    return true;
  }

  // print error message
  if (rmt_dir == RMT_RX_MODE) {
    log_w("==>%s():Channel set as TX instead of RX.", labelFunc);
  } else {
    log_w("==>%s():Channel set as RX instead of TX.", labelFunc);
  }
  return false;  // mismatched
}

static rmt_bus_handle_t _rmtGetBus(int pin, const char *labelFunc) {
  // Is pin RX or TX? Let's find it out
  peripheral_bus_type_t rmt_bus_type = perimanGetPinBusType(pin);
  if (rmt_bus_type != ESP32_BUS_TYPE_RMT_TX && rmt_bus_type != ESP32_BUS_TYPE_RMT_RX) {
    log_e("==>%s():GPIO %u is not attached to an RMT channel.", labelFunc, pin);
    return NULL;
  }

  return (rmt_bus_handle_t)perimanGetPinBus(pin, rmt_bus_type);
}

// Peripheral Manager detach callback
static bool _rmtDetachBus(void *busptr) {
  // sanity check - it should never happen
  assert(busptr && "_rmtDetachBus bus NULL pointer.");

  bool retCode = true;
  rmt_bus_handle_t bus = (rmt_bus_handle_t)busptr;
  log_v("Detaching RMT GPIO Bus");

  // lock it
  while (xSemaphoreTake(g_rmt_block_lock, portMAX_DELAY) != pdPASS) {}

  // free Event Group
  if (bus->rmt_events != NULL) {
    vEventGroupDelete(bus->rmt_events);
    bus->rmt_events = NULL;
  }
  // deallocate the channel encoder
  if (bus->rmt_copy_encoder_h != NULL) {
    if (ESP_OK != rmt_del_encoder(bus->rmt_copy_encoder_h)) {
      log_w("RMT Encoder Deletion has failed.");
      retCode = false;
    }
  }
  // disable and deallocate RMT channel
  if (bus->rmt_channel_h != NULL) {
    // force stopping rmt TX/RX processing and unlock Power Management (APB Freq)
    rmt_disable(bus->rmt_channel_h);
    if (ESP_OK != rmt_del_channel(bus->rmt_channel_h)) {
      log_w("RMT Channel Deletion has failed.");
      retCode = false;
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  // deallocate channel semaphore
  if (bus->g_rmt_objlocks != NULL) {
    vSemaphoreDelete(bus->g_rmt_objlocks);
  }
#endif
  // free the allocated bus data structure
  free(bus);

  // release the mutex
  xSemaphoreGive(g_rmt_block_lock);
  return retCode;
}

/**
   Public method definitions
*/

bool rmtSetEOT(int pin, uint8_t EOT_Level) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_TX_MODE, __FUNCTION__)) {
    return false;
  }

  bus->rmt_EOT_Level = EOT_Level > 0 ? 1 : 0;
  return true;
}

bool rmtSetCarrier(int pin, bool carrier_en, bool carrier_level, uint32_t frequency_Hz, float duty_percent) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }

  if (duty_percent > 1) {
    log_w("GPIO %d - RMT Carrier must be a float percentage from 0 to 1. Setting to 50%.", pin);
    duty_percent = 0.5;
  }
  rmt_carrier_config_t carrier_cfg = { 0 };
  carrier_cfg.duty_cycle = duty_percent;                     // duty cycle
  carrier_cfg.frequency_hz = carrier_en ? frequency_Hz : 0;  // carrier frequency in Hz
  carrier_cfg.flags.polarity_active_low = carrier_level;     // carrier modulation polarity level

  bool retCode = true;
  RMT_MUTEX_LOCK(bus);
  // modulate carrier to TX channel
  if (ESP_OK != rmt_apply_carrier(bus->rmt_channel_h, &carrier_cfg)) {
    log_w("GPIO %d - Error applying RMT carrier.", pin);
    retCode = false;
  }
  RMT_MUTEX_UNLOCK(bus);

  return retCode;
}

bool rmtSetRxMinThreshold(int pin, uint8_t filter_pulse_ticks) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }

  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  uint32_t filter_pulse_ns = (1000000000 / bus->frequency_Hz) * filter_pulse_ticks;
  // RMT_LL_MAX_FILTER_VALUE is 255 for ESP32, S2, S3, C3, C6 and H2;
  // filter_pulse_ticks is 8 bits, thus it will not exceed 255
#if 0  // for the future, in case some other SoC has different limit
  if (filter_pulse_ticks > RMT_LL_MAX_FILTER_VALUE) {
    log_e("filter_pulse_ticks is too big. Max = %d", RMT_LL_MAX_FILTER_VALUE);
    return false;
  }
#endif

  RMT_MUTEX_LOCK(bus);
  bus->signal_range_min_ns = filter_pulse_ns;  // set zero to disable it
  RMT_MUTEX_UNLOCK(bus);
  return true;
}

bool rmtSetRxMaxThreshold(int pin, uint16_t idle_thres_ticks) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }

  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  uint32_t idle_thres_ns = (1000000000 / bus->frequency_Hz) * idle_thres_ticks;
  // RMT_LL_MAX_IDLE_VALUE is 65535 for ESP32,S2 and 32767 for S3, C3, C6 and H2
#if RMT_LL_MAX_IDLE_VALUE < 65535  // idle_thres_ticks is 16 bits anyway - save some bytes
  if (idle_thres_ticks > RMT_LL_MAX_IDLE_VALUE) {
    log_e("idle_thres_ticks is too big. Max = %ld", RMT_LL_MAX_IDLE_VALUE);
    return false;
  }
#endif

  RMT_MUTEX_LOCK(bus);
  bus->signal_range_max_ns = idle_thres_ns;
  RMT_MUTEX_UNLOCK(bus);
  return true;
}

bool rmtDeinit(int pin) {
  log_v("Deiniting RMT GPIO %d", pin);
  if (_rmtGetBus(pin, __FUNCTION__) != NULL) {
    // release all allocated data
    return perimanClearPinBus(pin);
  }
  log_e("GPIO %d - No RMT channel associated.", pin);
  return false;
}

static bool _rmtWrite(int pin, rmt_data_t *data, size_t num_rmt_symbols, bool blocking, bool loop, uint32_t timeout_ms) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_TX_MODE, __FUNCTION__)) {
    return false;
  }
  bool loopCancel = false;  // user wants to cancel the writing loop mode
  if (data == NULL || num_rmt_symbols == 0) {
    if (!loop) {
      log_w("GPIO %d - RMT Write Data NULL pointer or size is zero.", pin);
      return false;
    } else {
      loopCancel = true;
    }
  }

  log_v("GPIO: %d - Request: %d RMT Symbols - %s - Timeout: %d", pin, num_rmt_symbols, blocking ? "Blocking" : "Non-Blocking", timeout_ms);
  log_v("GPIO: %d - Currently in Loop Mode: [%s] | Asked to Loop: %s, LoopCancel: %s", pin, bus->rmt_ch_is_looping ? "YES" : "NO", loop ? "YES" : "NO", loopCancel ? "YES" : "NO");

  if ((xEventGroupGetBits(bus->rmt_events) & RMT_FLAG_TX_DONE) == 0) {
    log_v("GPIO %d - RMT Write still pending to be completed.", pin);
    return false;
  }

  rmt_transmit_config_t transmit_cfg = { 0 };  // loop mode disabled
  bool retCode = true;

  RMT_MUTEX_LOCK(bus);
  // wants to start in writing or looping over a previous looping --> resets the channel
  if (bus->rmt_ch_is_looping == true) {
    // must force stopping a previous loop transmission first
    rmt_disable(bus->rmt_channel_h);
    // enable it again for looping or writing
    rmt_enable(bus->rmt_channel_h);
    bus->rmt_ch_is_looping = false;  // not looping anymore
  }
  // sets the End of Transmission level to HIGH if the user has requested so
  if (bus->rmt_EOT_Level) {
    transmit_cfg.flags.eot_level = 1;  // EOT is HIGH
  }
  if (loopCancel) {
    // just resets and releases the channel, maybe, already done above, then exits
    bus->rmt_ch_is_looping = false;
  } else {  // new writing | looping request
    // looping | Writing over a previous looping state is valid
    if (loop) {
      transmit_cfg.loop_count = -1;  // enable infinite loop mode
      // keeps RMT_FLAG_TX_DONE set - it never changes
    } else {
      // looping mode never sets this flag (IDF 5.1) in the callback
      xEventGroupClearBits(bus->rmt_events, RMT_FLAG_TX_DONE);
    }
    // transmits just once or looping data
    if (ESP_OK != rmt_transmit(bus->rmt_channel_h, bus->rmt_copy_encoder_h, (const void *)data, num_rmt_symbols * sizeof(rmt_data_t), &transmit_cfg)) {
      retCode = false;
      log_w("GPIO %d - RMT Transmission failed.", pin);
    } else {  // transmit OK
      if (loop) {
        bus->rmt_ch_is_looping = true;  // for ever... until a channel canceling or new writing
      } else {
        if (blocking) {
          // wait for transmission confirmation | timeout
          retCode = (xEventGroupWaitBits(bus->rmt_events, RMT_FLAG_TX_DONE, pdFALSE /* do not clear on exit */,
                                         pdFALSE /* wait for all bits */, timeout_ms)
                     & RMT_FLAG_TX_DONE)
                    != 0;
        }
      }
    }
  }
  RMT_MUTEX_UNLOCK(bus);
  return retCode;
}

static bool _rmtRead(int pin, rmt_data_t *data, size_t *num_rmt_symbols, bool waitForData, uint32_t timeout_ms) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }
  if (data == NULL || num_rmt_symbols == NULL) {
    log_w("GPIO %d - RMT Read Data and/or Size NULL pointer.", pin);
    return false;
  }
  log_v("GPIO: %d - Request: %d RMT Symbols - %s - Timeout: %d", pin, *num_rmt_symbols, waitForData ? "Blocking" : "Non-Blocking", timeout_ms);
  bool retCode = true;
  RMT_MUTEX_LOCK(bus);

  // request reading RMT Channel Data
  rmt_receive_config_t receive_config;
  receive_config.signal_range_min_ns = bus->signal_range_min_ns;
  receive_config.signal_range_max_ns = bus->signal_range_max_ns;

  xEventGroupClearBits(bus->rmt_events, RMT_FLAG_RX_DONE);
  bus->num_symbols_read = num_rmt_symbols;

  rmt_receive(bus->rmt_channel_h, data, *num_rmt_symbols * sizeof(rmt_data_t), &receive_config);
  // wait for data if requested
  if (waitForData) {
    retCode = (xEventGroupWaitBits(bus->rmt_events, RMT_FLAG_RX_DONE, pdFALSE /* do not clear on exit */,
                                   pdFALSE /* wait for all bits */, timeout_ms)
               & RMT_FLAG_RX_DONE)
              != 0;
  }

  RMT_MUTEX_UNLOCK(bus);
  return retCode;
}


bool rmtWrite(int pin, rmt_data_t *data, size_t num_rmt_symbols, uint32_t timeout_ms) {
  return _rmtWrite(pin, data, num_rmt_symbols, true /*blocks*/, false /*looping*/, timeout_ms);
}

bool rmtWriteAsync(int pin, rmt_data_t *data, size_t num_rmt_symbols) {
  return _rmtWrite(pin, data, num_rmt_symbols, false /*blocks*/, false /*looping*/, 0 /*N/A*/);
}

bool rmtWriteLooping(int pin, rmt_data_t *data, size_t num_rmt_symbols) {
  return _rmtWrite(pin, data, num_rmt_symbols, false /*blocks*/, true /*looping*/, 0 /*N/A*/);
}

bool rmtTransmitCompleted(int pin) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_TX_MODE, __FUNCTION__)) {
    return false;
  }

  bool retCode = true;
  RMT_MUTEX_LOCK(bus);
  retCode = (xEventGroupGetBits(bus->rmt_events) & RMT_FLAG_TX_DONE) != 0;
  RMT_MUTEX_UNLOCK(bus);
  return retCode;
}

bool rmtRead(int pin, rmt_data_t *data, size_t *num_rmt_symbols, uint32_t timeout_ms) {
  return _rmtRead(pin, data, num_rmt_symbols, true /* blocking */, timeout_ms);
}

bool rmtReadAsync(int pin, rmt_data_t *data, size_t *num_rmt_symbols) {
  return _rmtRead(pin, data, num_rmt_symbols, false /* non-blocking */, 0 /* N/A */);
}

bool rmtReceiveCompleted(int pin) {
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  bool retCode = true;
  RMT_MUTEX_LOCK(bus);
  retCode = (xEventGroupGetBits(bus->rmt_events) & RMT_FLAG_RX_DONE) != 0;
  RMT_MUTEX_UNLOCK(bus);
  return retCode;
}

bool rmtInit(int pin, rmt_ch_dir_t channel_direction, rmt_reserve_memsize_t mem_size, uint32_t frequency_Hz) {
  log_v("GPIO %d - %s - MemSize[%d] - Freq=%dHz", pin, channel_direction == RMT_RX_MODE ? "RX MODE" : "TX MODE", mem_size * RMT_SYMBOLS_PER_CHANNEL_BLOCK, frequency_Hz);

  // create common block mutex for protecting allocs from multiple threads allocating RMT channels
  if (!g_rmt_block_lock) {
    g_rmt_block_lock = xSemaphoreCreateMutex();
    if (g_rmt_block_lock == NULL) {
      log_e("GPIO %d - Failed creating RMT Mutex.", pin);
      return false;
    }
  }

  // set Peripheral Manager deInit Callback
  perimanSetBusDeinit(ESP32_BUS_TYPE_RMT_TX, _rmtDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_RMT_RX, _rmtDetachBus);

  // check is pin is valid and in the right direction
  if ((channel_direction == RMT_TX_MODE && !GPIO_IS_VALID_OUTPUT_GPIO(pin)) || (!GPIO_IS_VALID_GPIO(pin))) {
    log_e("GPIO %d is not valid or can't be used for output in TX mode.", pin);
    return false;
  }

  // validate the RMT ticks by the requested frequency
  // Based on 80Mhz using a divider of 8 bits (calculated as 1..256)
  if (frequency_Hz > 80000000 || frequency_Hz < 312500) {
    log_e("GPIO %d - Bad RMT frequency resolution. Must be between 312.5KHz to 80MHz.", pin);
    return false;
  }

  // Try to detach any (Tx|Rx|Whatever) previous bus or just keep it as not attached
  if (!perimanClearPinBus(pin)) {
    log_w("GPIO %d - Can't detach previous peripheral.", pin);
    return false;
  }

  // lock it
  while (xSemaphoreTake(g_rmt_block_lock, portMAX_DELAY) != pdPASS) {}

  // allocate the rmt bus object and sets all fields to NULL
  rmt_bus_handle_t bus = (rmt_bus_handle_t)heap_caps_calloc(1, sizeof(struct rmt_obj_s), MALLOC_CAP_DEFAULT);
  if (bus == NULL) {
    log_e("GPIO %d - Bus Memory allocation fault.", pin);
    goto Err;
  }

  // store the RMT Freq to check Filter and Idle valid values in the RMT API
  bus->frequency_Hz = frequency_Hz;
  // pulses with width smaller than min_ns will be ignored (as a glitch)
  //bus->signal_range_min_ns = 0; // disabled  --> not necessary CALLOC set all to ZERO.
  // RMT stops reading if the input stays idle for longer than max_ns
  bus->signal_range_max_ns = (1000000000 / frequency_Hz) * RMT_LL_MAX_IDLE_VALUE;  // maximum possible
  // creates the event group to control read_done and write_done
  bus->rmt_events = xEventGroupCreate();
  if (bus->rmt_events == NULL) {
    log_e("GPIO %d - RMT Group Event allocation fault.", pin);
    goto Err;
  }

  // Starting with Receive|Transmit DONE bits set, for allowing a new request from user
  xEventGroupSetBits(bus->rmt_events, RMT_FLAG_RX_DONE | RMT_FLAG_TX_DONE);

  // channel particular configuration
  if (channel_direction == RMT_TX_MODE) {
    // TX Channel
    rmt_tx_channel_config_t tx_cfg;
    tx_cfg.gpio_num = pin;
    // CLK_APB for ESP32|S2|S3|C3 -- CLK_PLL_F80M for C6 -- CLK_XTAL for H2
    tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_cfg.resolution_hz = frequency_Hz;
    tx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL * mem_size;
    tx_cfg.trans_queue_depth = 10;  // maximum allowed
    tx_cfg.flags.invert_out = 0;
    tx_cfg.flags.with_dma = 0;
    tx_cfg.flags.io_loop_back = 0;
    tx_cfg.flags.io_od_mode = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2)
    tx_cfg.intr_priority = 0;
#endif

    if (rmt_new_tx_channel(&tx_cfg, &bus->rmt_channel_h) != ESP_OK) {
      log_e("GPIO %d - RMT TX Initialization error.", pin);
      goto Err;
    }

    // set TX Callback
    rmt_tx_event_callbacks_t cbs = { .on_trans_done = _rmt_tx_done_callback };
    if (ESP_OK != rmt_tx_register_event_callbacks(bus->rmt_channel_h, &cbs, bus)) {
      log_e("GPIO %d RMT - Error registering TX Callback.", pin);
      goto Err;
    }

  } else {
    // RX Channel
    rmt_rx_channel_config_t rx_cfg;
    rx_cfg.gpio_num = pin;
    // CLK_APB for ESP32|S2|S3|C3 -- CLK_PLL_F80M for C6 -- CLK_XTAL for H2
    rx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_cfg.resolution_hz = frequency_Hz;
    rx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL * mem_size;
    rx_cfg.flags.invert_in = 0;
    rx_cfg.flags.with_dma = 0;
    rx_cfg.flags.io_loop_back = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2)
    rx_cfg.intr_priority = 0;
#endif
    // try to allocate the RMT Channel
    if (ESP_OK != rmt_new_rx_channel(&rx_cfg, &bus->rmt_channel_h)) {
      log_e("GPIO %d RMT - RX Initialization error.", pin);
      goto Err;
    }

    // set RX Callback
    rmt_rx_event_callbacks_t cbs = { .on_recv_done = _rmt_rx_done_callback };
    if (ESP_OK != rmt_rx_register_event_callbacks(bus->rmt_channel_h, &cbs, bus)) {
      log_e("GPIO %d RMT - Error registering RX Callback.", pin);
      goto Err;
    }
  }

  // allocate memory for the RMT Copy encoder
  rmt_copy_encoder_config_t copy_encoder_config = {};
  if (rmt_new_copy_encoder(&copy_encoder_config, &bus->rmt_copy_encoder_h) != ESP_OK) {
    log_e("GPIO %d - RMT Encoder Memory Allocation error.", pin);
    goto Err;
  }

  // create each channel Mutex for multi thread operations
#if !CONFIG_DISABLE_HAL_LOCKS
  bus->g_rmt_objlocks = xSemaphoreCreateMutex();
  if (bus->g_rmt_objlocks == NULL) {
    log_e("GPIO %d - Failed creating RMT Channel Mutex.", pin);
    goto Err;
  }
#endif

  rmt_enable(bus->rmt_channel_h);  // starts/enables the channel

  // Finally, allocate Peripheral Manager RMT bus and associate it to its GPIO
  peripheral_bus_type_t pinBusType =
    channel_direction == RMT_TX_MODE ? ESP32_BUS_TYPE_RMT_TX : ESP32_BUS_TYPE_RMT_RX;
  if (!perimanSetPinBus(pin, pinBusType, (void *)bus, -1, -1)) {
    log_e("Can't allocate the GPIO %d in the Peripheral Manager.", pin);
    goto Err;
  }

  // this delay is necessary when CPU frequency changes, but internal RMT setup is "old/wrong"
  // The use case is related to the RMT_CPUFreq_Test example. The very first RMT Write
  // goes in the wrong pace (frequency). The delay allows other IDF tasks to run to fix it.
  if (loopTaskHandle != NULL) {
    // it can only run when Arduino task has been already started.
    delay(1);
  }  // prevent panic when rmtInit() is executed within an C++ object constructor
  // release the mutex
  xSemaphoreGive(g_rmt_block_lock);
  return true;

Err:
  // release LOCK and the RMT object
  xSemaphoreGive(g_rmt_block_lock);
  _rmtDetachBus((void *)bus);
  return false;
}

#endif /* SOC_RMT_SUPPORTED */
