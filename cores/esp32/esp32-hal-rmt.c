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

#include "esp32-hal.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

#include "esp32-hal-rmt.h"
#include "esp32-hal-periman.h"


/**
   Internal macros
*/

#if CONFIG_DISABLE_HAL_LOCKS
# define RMT_MUTEX_LOCK(busptr)
# define RMT_MUTEX_UNLOCK(busptr)
#else
# define RMT_MUTEX_LOCK(busptr)    do {} while (xSemaphoreTake(busptr->g_rmt_objlocks, portMAX_DELAY) != pdPASS)
# define RMT_MUTEX_UNLOCK(busptr)  xSemaphoreGive(busptr->g_rmt_objlocks)
#endif /* CONFIG_DISABLE_HAL_LOCKS */


/**
   Typedefs for internal stuctures, enums
*/
struct rmt_obj_s {

  // general rmt information
  rmt_channel_handle_t rmt_channel_h;        // NULL value means channel not alocated
  rmt_encoder_handle_t rmt_copy_encoder_h;   // RMT simple copy encoder handle

  uint8_t num_buffers;                       // Number of allocated buffers needed for reading
  uint32_t signal_range_min_ns;              // RX Filter data - Low Pass pulse width
  uint32_t signal_range_max_ns;              // RX idle time that defines end of reading

  EventGroupHandle_t rmt_rx_events;          // reading event user handle
  TaskHandle_t rmt_rxTaskHandle;             // Rx task created to wait for received RMT data
  QueueHandle_t rmt_rx_queue;                // Rx callback data queue
  bool rmt_rx_completed;                     // rmtReceiveCompleted() rmtReadAsync() async reading
  bool rmt_async_rx_enabled;                 // rmtEnd() rmtBeginReceive() rmtRead() rmtReadAsync() control
  rmt_data_t* rmt_data_rx_ptr;               // Rx Task - data user pointer
  size_t rmt_data_rx_size;                   // Rx Task - data length

  xSemaphoreHandle g_rmt_objlocks;           // Channel Semaphore Lock
};
typedef struct rmt_obj_s *rmt_bus_handle_t;

/**
   Internal variables used in RMT API
*/
static xSemaphoreHandle g_rmt_block_lock = NULL;

/**
   Internal method (private) declarations
*/

static bool _rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *data, void *args)
{
  BaseType_t high_task_wakeup = pdFALSE;
  QueueHandle_t rmt_rx_queue = (QueueHandle_t)args;
  // send the received RMT symbols to the _rmtRxTask of that channel
  xQueueSendFromISR(rmt_rx_queue, data, &high_task_wakeup);
  return high_task_wakeup == pdTRUE;
}

static void _rmtRxTask(void *args) {
  rmt_bus_handle_t bus = (rmt_bus_handle_t) args;

  // sanity check - it should never happen
  assert(bus && "_rmtRxTask bus NULL pointer.");

  bus->rmt_rx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
  if (bus->rmt_rx_queue == NULL) {
    log_e("Error creating RX Queue.");
    goto err;
  }

  rmt_rx_event_callbacks_t cbs = { .on_recv_done = _rmt_rx_done_callback };
  if (ESP_OK != rmt_rx_register_event_callbacks(bus->rmt_channel_h, &cbs, bus->rmt_rx_queue)) {
    log_e("Error registering RX Callback.");
    vQueueDelete(bus->rmt_rx_queue);
    goto err;
  }

  for (;;) {
    // request reading RMT Channel Data
    rmt_receive_config_t receive_config;
    receive_config.signal_range_min_ns = bus->signal_range_min_ns;
    receive_config.signal_range_max_ns = bus->signal_range_max_ns;
    // buffer to save the received RMT symbols from the callback, in STACK for easier memory deallocation
    size_t num_rmt_symbols = bus->num_buffers * SOC_RMT_MEM_WORDS_PER_CHANNEL;  // max is 8*64*4 = 2KB
    rmt_data_t rmt_symbols[num_rmt_symbols]; // number of symbols requested by user
    // must make sure that the RX Channel is enabled!
    while (!bus->rmt_async_rx_enabled) {
      delay(1); // let other lower priority tasks to run, such as Arduino Task...
    }
    rmt_receive(bus->rmt_channel_h, rmt_symbols, num_rmt_symbols * sizeof(rmt_data_t), &receive_config);

    // wait for ever for RX done signal
    rmt_rx_done_event_data_t rx_data;
    if (xQueueReceive(bus->rmt_rx_queue, &rx_data, portMAX_DELAY) == pdPASS) {
      log_d("RMT has read %d bytes.", rx_data.num_symbols * sizeof(rmt_data_t));
      // Async Read -- copy data to caller
      uint32_t data_size = bus->rmt_data_rx_size;
      rmt_data_t *p = (rmt_data_t *)bus->rmt_data_rx_ptr;
      if (p != NULL && data_size > 0) {
        if (rx_data.num_symbols < data_size) data_size = rx_data.num_symbols;
        for (uint32_t i = 0; i < data_size; i++) {
          p[i].val = rx_data.received_symbols[i].val;
        }
      }
      // set RX event group
      if (bus->rmt_rx_events) {
        xEventGroupSetBits(bus->rmt_rx_events, RMT_FLAG_RX_DONE);
      }
      // used in rmtReceiveCompleted()
      bus->rmt_rx_completed = true;
    } // xQueueReceive
  } // for(;;)

err:
  vTaskDelete(NULL);
}

static bool _rmtCreateRxTask(rmt_bus_handle_t bus)
{
  if (bus == NULL) {
    return false;
  }
  if (bus->rmt_rxTaskHandle != NULL) {   // Task already created
    return true;
  }

  xTaskCreate(_rmtRxTask, "rmt_rx_task", 1024 * 6, bus, configMAX_PRIORITIES - 1, &bus->rmt_rxTaskHandle);

  if (bus->rmt_rxTaskHandle == NULL) {
    log_e("RMT RX Task create failed");
    return false;
  }
  return true;
}

// This function must be called only after checking the pin and its bus with _rmtGetBus()
static bool _rmtCheckDirection(uint8_t gpio_num, rmt_ch_dir_t rmt_dir, const char* labelFunc)
{
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
  return false;       // mismatched
}

static rmt_bus_handle_t _rmtGetBus(int pin, const char* labelFunc)
{
  // Is pin RX or TX? Let's find it out
  peripheral_bus_type_t rmt_bus_type = perimanGetPinBusType(pin);
  if (rmt_bus_type != ESP32_BUS_TYPE_RMT_TX && rmt_bus_type != ESP32_BUS_TYPE_RMT_RX) {
    log_e("==>%s():GPIO %u is not attached to an RMT channel.", labelFunc, pin);
    return NULL;
  }

  return (rmt_bus_handle_t)perimanGetPinBus(pin, rmt_bus_type);
}

// Peripheral Manager detach callback
static bool _rmtDetachBus(void *busptr)
{
  // sanity check - it should never happen
  assert(busptr && "_rmtDetachBus bus NULL pointer.");

  bool retCode = true;
  rmt_bus_handle_t bus = (rmt_bus_handle_t) busptr;

  // deleted RX channel tasks
  if (bus->rmt_rxTaskHandle != NULL) {
    vTaskDelete(bus->rmt_rxTaskHandle);
  }
  // delete RX Queue
  if (bus->rmt_rx_queue != NULL) {
    vQueueDelete(bus->rmt_rx_queue);
  }

  // deallocate the channel encoder
  if (bus->rmt_copy_encoder_h != NULL) {
    if (ESP_OK != rmt_del_encoder(bus->rmt_copy_encoder_h)) {
      log_w("RMT Encoder Deletion has failed.");
      retCode = false;
    }
  }
  // disable and deallocate RMT channal
  if (bus->rmt_channel_h != NULL) {
    // force stopping rmt TX/RX processing
    rmt_disable(bus->rmt_channel_h);
    if (ESP_OK != rmt_del_channel(bus->rmt_channel_h)) {
      log_w("RMT Channel Deletion has failed.");
      retCode = false;
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  // deallocate locker
  if (bus->g_rmt_objlocks != NULL) {
    vSemaphoreDelete(bus->g_rmt_objlocks);
  }
#endif
  // free the allocated bus data structure
  free(bus);
  return retCode;
}

/**
   Public method definitions
*/

// RX demodulation carrier or TX modulatin carrier
bool rmtSetCarrier(int pin, bool carrier_en, bool carrier_level, uint32_t frequency_Hz, float duty_percent)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }

  if (duty_percent > 1) {
    log_w("GPIO %d - RMT Carrier must be a float percentage from 0 to 1. Setting to 50%.", pin);
    duty_percent = 0.5;
  }
  rmt_carrier_config_t carrier_cfg = {0};
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

//In receive mode, channel will ignore input pulse when the pulse width is smaller than <filter_pulse_ns>
bool rmtSetFilter(int pin, bool filter_en, uint8_t filter_pulse_ns)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }

  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  RMT_MUTEX_LOCK(bus);
  bus->signal_range_min_ns = filter_en ? filter_pulse_ns : 0; // set as zero to disable it
  RMT_MUTEX_UNLOCK(bus);
  return true;
}

//In receive mode, when no edge is detected on the input signal for
//longer than idle_thres channel clock cycles, the receive process is finished.
bool rmtSetRxThreshold(int pin, uint16_t value)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }

  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  RMT_MUTEX_LOCK(bus);
  bus->signal_range_max_ns = value; // set as zero to disable it
  RMT_MUTEX_UNLOCK(bus);
  return true;
}

bool rmtDeinit(int pin)
{
  if (_rmtGetBus(pin, __FUNCTION__) != NULL) {
    // release all allocated data
    return perimanSetPinBus(pin, ESP32_BUS_TYPE_INIT, NULL);
  }
  log_e("GPIO %d - No RMT channel associated.", pin);
  return false;
}

bool rmtLoop(int pin, rmt_data_t* data, size_t size)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_TX_MODE, __FUNCTION__)) {
    return false;
  }

  rmt_transmit_config_t transmit_cfg = {0};
  transmit_cfg.loop_count = 1;  // enable infinite loop mode
  bool retCode = true;
  RMT_MUTEX_LOCK(bus);

  if (ESP_OK != rmt_transmit(bus->rmt_channel_h, bus->rmt_copy_encoder_h,
                             (const void *) data, size * sizeof(rmt_data_t), &transmit_cfg)) {
    retCode = false;
    log_w("GPIO %d - RMT Loop Transmission failed.", pin);
  }
  RMT_MUTEX_UNLOCK(bus);
  return retCode;
}

bool rmtWrite(int pin, rmt_data_t* data, size_t size)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_TX_MODE, __FUNCTION__)) {
    return false;
  }

  rmt_transmit_config_t transmit_cfg = {0};  // disable loop mode
  bool retCode = true;
  RMT_MUTEX_LOCK(bus);

  if (ESP_OK != rmt_transmit(bus->rmt_channel_h, bus->rmt_copy_encoder_h,
                             (const void *) data, size * sizeof(rmt_data_t), &transmit_cfg)) {
    retCode = false;
    log_w("GPIO %d - RMT single Transmission failed.", pin);
  }
  RMT_MUTEX_UNLOCK(bus);
  return retCode;
}

bool rmtWriteBlocking(int pin, rmt_data_t* data, size_t size)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_TX_MODE, __FUNCTION__)) {
    return false;
  }

  rmt_transmit_config_t transmit_cfg = {0};  // disable loop mode
  bool retCode = true;
  RMT_MUTEX_LOCK(bus);

  if (ESP_OK != rmt_transmit(bus->rmt_channel_h, bus->rmt_copy_encoder_h,
                             (const void *) data, size * sizeof(rmt_data_t), &transmit_cfg)) {
    retCode = false;
    log_w("GPIO %d - RMT Blocking Transmission failed.", pin);
  }
  if (ESP_OK != rmt_tx_wait_all_done(bus->rmt_channel_h, -1)) {
    retCode = false;
    log_w("GPIO %d - RMT Blocking TX Setup failed.", pin);
  }
  RMT_MUTEX_UNLOCK(bus);
  return retCode;
}

// Sets a user buffer and length for reading RMT signal.
// rmtReceiveCompleted() shall be used to check if there is data read fromthe RMT channel
bool rmtReadData(int pin, uint32_t* data, size_t size)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }

  rmtReadAsync(pin, (rmt_data_t*) data, size, NULL, false, 0);
  return true;
}

bool rmtBeginReceive(int pin)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  RMT_MUTEX_LOCK(bus);
  bus->rmt_async_rx_enabled = true;
  bus->rmt_rx_completed = false;
  RMT_MUTEX_UNLOCK(bus);
  return true;
}

bool rmtReceiveCompleted(int pin)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  // is read enabled?
  if(!bus->rmt_async_rx_enabled) {
    return false;
  }

  RMT_MUTEX_LOCK(bus);
  bool retCode = bus->rmt_rx_completed;
  RMT_MUTEX_UNLOCK(bus);

  return retCode;
}

bool rmtEnd(int pin)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  RMT_MUTEX_LOCK(bus);
  bus->rmt_async_rx_enabled = false;
  RMT_MUTEX_UNLOCK(bus);
  return true;
}

bool rmtReadAsync(int pin, rmt_data_t* data, size_t size, void* eventFlag, bool waitForData, uint32_t timeout)
{
  rmt_bus_handle_t bus = _rmtGetBus(pin, __FUNCTION__);
  if (bus == NULL) {
    return false;
  }
  if (!_rmtCheckDirection(pin, RMT_RX_MODE, __FUNCTION__)) {
    return false;
  }

  rmtEnd(pin); // disable reading
  if (eventFlag) {
    xEventGroupClearBits(eventFlag, RMT_FLAG_RX_DONE);
  }
  // if NULL, no problems - rmtReadAsync works as a plain rmtReadData()
  bus->rmt_rx_events = eventFlag;

  // if NULL, no problems - task will take care of it
  bus->rmt_data_rx_ptr = data;
  bus->rmt_data_rx_size = size;
  bus->rmt_rx_completed = false;

  // Start a read task
  RMT_MUTEX_LOCK(bus);
  _rmtCreateRxTask(bus);
  RMT_MUTEX_UNLOCK(bus);

  // wait for data if requested so
  rmtBeginReceive(pin); // enable reading
  if (waitForData && eventFlag) {
    xEventGroupWaitBits(eventFlag, RMT_FLAG_RX_DONE,
                        pdTRUE /* clear on exit */, pdFALSE /* wait for all bits */, timeout);
  }
  return true;
}

bool rmtInit(int pin, rmt_ch_dir_t channel_direction, rmt_reserve_memsize_t mem_size, uint32_t frequency_Hz)
{
  // check is pin is valid and in the right direction
  if ((channel_direction == RMT_TX_MODE && !GPIO_IS_VALID_OUTPUT_GPIO(pin)) || (!GPIO_IS_VALID_GPIO(pin))) {
    log_e("GPIO %d is not valid or can't be used for output in TX mode.", pin);
    return false;
  }

  // set Peripheral Manager deInit Callback
  perimanSetBusDeinit(ESP32_BUS_TYPE_RMT_TX, _rmtDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_RMT_RX, _rmtDetachBus);

  // validate the RMT ticks by the requested frequency
#if SOC_RMT_SUPPORT_APB
  // Based on 80Mhz using a divider of 8 bits (calculated as 1..256) :: ESP32 and ESP32S2
  if (frequency_Hz > 80000000 || frequency_Hz < 312500) {
    log_e("GPIO %d - Bad RMT frequency resolution. Must be between 312.5KHz to 80MHz.", pin);
    return false;
  }
#elif SOC_RMT_SUPPORT_XTAL
  // Based on 40Mhz using a divider of 8 bits (calculated as 1..256)  :: ESP32C3 and ESP32S3
  if (frequency_Hz > 40000000 || frequency_Hz < 156250) {
    log_e("GPIO %d - Bad RMT frequency resolution. Must be between 156.25KHz to 40MHz.", pin);
    return false;
  }
#endif

  // create common block mutex for protecting allocs from multiple threads
  if (!g_rmt_block_lock) {
    g_rmt_block_lock = xSemaphoreCreateMutex();
    if (g_rmt_block_lock == NULL) {
      log_e("GPIO %d - Failed creating RMT Mutex.", pin);
      return false;
    }
  }
  // lock it
  while (xSemaphoreTake(g_rmt_block_lock, portMAX_DELAY) != pdPASS) {}

  // Try to dettach any previous bus or just keep it as not attached
  if (perimanGetPinBusType(pin) != ESP32_BUS_TYPE_INIT && !perimanSetPinBus(pin, ESP32_BUS_TYPE_INIT, NULL)) {
    log_w("GPIO %d - Can't detach previous peripheral.", pin);
    xSemaphoreGive(g_rmt_block_lock);
    return false;
  }

  // allocate the rmt bus object
  rmt_bus_handle_t bus = (rmt_bus_handle_t)heap_caps_calloc(1, sizeof(struct rmt_obj_s), MALLOC_CAP_DEFAULT);
  if (bus == NULL) {
    log_e("GPIO %d - Bus Memory allocation fault.", pin);
    xSemaphoreGive(g_rmt_block_lock);
    return false;
  }

  // common RX/TX channel configuration - common fields
  bus->num_buffers = mem_size;
  // pulses with width smaller than min_ns will be ignored (as a glitch)
  bus->signal_range_min_ns = 1000000000 / (frequency_Hz * 2); // 1/2 pulse width
  // RMT stops reading if the input stays idle for longer than max_ns
  bus->signal_range_max_ns = (1000000000 / frequency_Hz) * 10; // 10 pulses width

  // channel particular configuration
  if (channel_direction == RMT_TX_MODE) {
    // TX Channel
    rmt_tx_channel_config_t tx_cfg;
    tx_cfg.gpio_num = pin;
#if SOC_RMT_SUPPORT_APB
    tx_cfg.clk_src = RMT_CLK_SRC_APB;
#elif SOC_RMT_SUPPORT_XTAL
    tx_cfg.clk_src = RMT_CLK_SRC_XTAL;
#endif
    tx_cfg.resolution_hz = frequency_Hz;
    tx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL * mem_size;
    tx_cfg.trans_queue_depth = 10;   // maximum allowed
    tx_cfg.flags.invert_out = 0;
    tx_cfg.flags.with_dma = 0;
    tx_cfg.flags.io_loop_back = 0;
    tx_cfg.flags.io_od_mode = 0;

    if (rmt_new_tx_channel(&tx_cfg, &bus->rmt_channel_h) != ESP_OK) {
      log_e("GPIO %d - RMT TX Initialization error.", pin);
      // release the RMT object
      _rmtDetachBus((void *)bus);
      xSemaphoreGive(g_rmt_block_lock);
      return false;
    }
  } else {
    // RX Channel
    rmt_rx_channel_config_t rx_cfg;
    rx_cfg.gpio_num = pin;
#if SOC_RMT_SUPPORT_APB
    rx_cfg.clk_src = RMT_CLK_SRC_APB;
#elif SOC_RMT_SUPPORT_XTAL
    rx_cfg.clk_src = RMT_CLK_SRC_XTAL;
#endif
    rx_cfg.resolution_hz = frequency_Hz;
    rx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL * mem_size;
    rx_cfg.flags.invert_in = 0;
    rx_cfg.flags.with_dma = 0;
    rx_cfg.flags.io_loop_back = 0;
    // try to allocate the RMT Channel
    if (ESP_OK != rmt_new_rx_channel(&rx_cfg, &bus->rmt_channel_h)) {
      log_e("GPIO %d RMT - RX Initialization error.", pin);
      // release the RMT object
      _rmtDetachBus((void *)bus);
      xSemaphoreGive(g_rmt_block_lock);
      return false;
    }
  }

  // allocate memory for the RMT Copy encoder
  rmt_copy_encoder_config_t copy_encoder_config = {};
  if (rmt_new_copy_encoder(&copy_encoder_config, &bus->rmt_copy_encoder_h) != ESP_OK) {
    log_e("GPIO %d - RMT Encoder Memory Allocation error.", pin);
    // release the RMT object
    _rmtDetachBus((void *)bus);
    xSemaphoreGive(g_rmt_block_lock);
    return false;
  }

  // create each channel Mutex for multi thread operations
#if !CONFIG_DISABLE_HAL_LOCKS
  bus->g_rmt_objlocks = xSemaphoreCreateMutex();
  if (bus->g_rmt_objlocks == NULL) {
    log_e("GPIO %d - Failed creating RMT Channel Mutex.", pin);
    // release the RMT object
    _rmtDetachBus((void *)bus);
    xSemaphoreGive(g_rmt_block_lock);
    return false;
  }
#endif

  rmt_enable(bus->rmt_channel_h); // starts the channel

  // Finally, allocate Peripheral Manager RMT bus and associate it to its GPIO
  peripheral_bus_type_t pinBusType =
    channel_direction == RMT_TX_MODE ? ESP32_BUS_TYPE_RMT_TX : ESP32_BUS_TYPE_RMT_RX;
  if (!perimanSetPinBus(pin, pinBusType, (void *) bus)) {
    log_e("Can't allocate the GPIO %d in the Peripheral Manager.", pin);
    // release the RMT object
    _rmtDetachBus((void *)bus);
    xSemaphoreGive(g_rmt_block_lock);
    return false;
  }

  // release the mutex
  xSemaphoreGive(g_rmt_block_lock);
  return true;
}
