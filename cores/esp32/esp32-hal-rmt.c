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

#include "esp32-hal.h"
#include "driver/rmt.h"

/**
 * Internal macros
 */

#define MAX_CHANNELS                        (SOC_RMT_GROUPS * SOC_RMT_CHANNELS_PER_GROUP)

#define RMT_TX_CH_START       (0)
#define RMT_TX_CH_END         (SOC_RMT_TX_CANDIDATES_PER_GROUP - 1)
#define RMT_RX_CH_START       (SOC_RMT_CHANNELS_PER_GROUP - SOC_RMT_TX_CANDIDATES_PER_GROUP)
#define RMT_RX_CH_END         (SOC_RMT_CHANNELS_PER_GROUP - 1)

#define _LIMIT(a,b) (a>b?b:a)

#if CONFIG_DISABLE_HAL_LOCKS
# define RMT_MUTEX_LOCK(channel)
# define RMT_MUTEX_UNLOCK(channel)
#else
# define RMT_MUTEX_LOCK(channel)    do {} while (xSemaphoreTake(g_rmt_objlocks[channel], portMAX_DELAY) != pdPASS)
# define RMT_MUTEX_UNLOCK(channel)  xSemaphoreGive(g_rmt_objlocks[channel])
#endif /* CONFIG_DISABLE_HAL_LOCKS */

//#define _RMT_INTERNAL_DEBUG
#ifdef _RMT_INTERNAL_DEBUG
# define DEBUG_INTERRUPT_START(pin)    digitalWrite(pin, 1);
# define DEBUG_INTERRUPT_END(pin)      digitalWrite(pin, 0);
#else
# define DEBUG_INTERRUPT_START(pin)
# define DEBUG_INTERRUPT_END(pin)
#endif /* _RMT_INTERNAL_DEBUG */

#define RMT_DEFAULT_ARD_CONFIG_TX(gpio, channel_id, buffers)      \
    {                                                \
        .rmt_mode = RMT_MODE_TX,                     \
        .channel = channel_id,                       \
        .gpio_num = gpio,                            \
        .clk_div = 1,                                \
        .mem_block_num = buffers,                    \
        .flags = 0,                                  \
        .tx_config = {                               \
            .carrier_level = RMT_CARRIER_LEVEL_HIGH, \
            .idle_level = RMT_IDLE_LEVEL_LOW,        \
            .carrier_duty_percent = 50,              \
            .carrier_en = false,                     \
            .loop_en = false,                        \
            .idle_output_en = true,                  \
        }                                            \
    }

#define RMT_DEFAULT_ARD_CONFIG_RX(gpio, channel_id, buffers) \
    {                                           \
        .rmt_mode = RMT_MODE_RX,                \
        .channel = channel_id,                  \
        .gpio_num = gpio,                       \
        .clk_div = 1,                           \
        .mem_block_num = buffers,               \
        .flags = 0,                             \
        .rx_config = {                          \
            .idle_threshold = 0x80,             \
            .filter_ticks_thresh = 100,         \
            .filter_en = false,                 \
        }                                       \
    }




/**
 * Typedefs for internal stuctures, enums
 */
struct rmt_obj_s
{
    bool allocated;
    EventGroupHandle_t events;
    int channel;
    int buffers;
    int data_size;
    uint32_t* data_ptr;
    rmt_rx_data_cb_t cb;
    void * arg;
    TaskHandle_t rxTaskHandle;  
    bool rx_completed;
    bool tx_not_rx;
};

/**
 * Internal variables for channel descriptors
 */
static xSemaphoreHandle g_rmt_objlocks[MAX_CHANNELS] = {
    NULL, NULL, NULL, NULL, 
#if MAX_CHANNELS > 4
    NULL, NULL, NULL, NULL
#endif
};

static rmt_obj_t g_rmt_objects[MAX_CHANNELS] = {
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
#if MAX_CHANNELS > 4
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
    { false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, true, true},
#endif
};

/**
 * Internal variables for driver data
 */
static xSemaphoreHandle g_rmt_block_lock = NULL;

/**
 * Internal method (private) declarations
 */

static rmt_obj_t* _rmtAllocate(int pin, int from, int size)
{
    size_t i;
    // setup how many buffers shall we use
    g_rmt_objects[from].buffers = size;

    for (i=0; i<size; i++) {
        // mark the block of channels as used
        g_rmt_objects[i+from].allocated = true;
    }
    return &(g_rmt_objects[from]);
}

void _rmtDumpStatus(rmt_obj_t* rmt) 
{
   bool loop_en;
   uint8_t div_cnt;
   uint8_t memNum;
   bool lowPowerMode;
   rmt_mem_owner_t owner;
   uint16_t idleThreshold;
   uint32_t status;
   rmt_source_clk_t srcClk;
   rmt_channel_t channel = rmt->channel;

RMT_MUTEX_LOCK(channel);
   rmt_get_tx_loop_mode(channel, &loop_en);
   rmt_get_clk_div(channel, &div_cnt);
   rmt_get_mem_block_num(channel, &memNum);
   rmt_get_mem_pd(channel, &lowPowerMode);
   rmt_get_memory_owner(channel, &owner);
   rmt_get_rx_idle_thresh(channel, &idleThreshold);
   rmt_get_status(channel, &status);
   rmt_get_source_clk(channel, &srcClk);

   log_d("Status for RMT channel %d", channel);
   log_d("- Loop enabled: %d", loop_en);
   log_d("- Clock divisor: %d", div_cnt);
   log_d("- Number of memory blocks: %d", memNum);
   log_d("- Low power mode: %d", lowPowerMode);
   log_d("- Memory owner: %s", owner==RMT_MEM_OWNER_TX?"TX":"RX");
   log_d("- Idle threshold: %d", idleThreshold);
   log_d("- Status: %d", status);
   log_d("- Source clock: %s", srcClk==RMT_BASECLK_APB?"APB (80MHz)":"1MHz");
RMT_MUTEX_UNLOCK(channel);
}

static void _rmtRxTask(void *args) {
    rmt_obj_t *rmt = (rmt_obj_t *) args;
    RingbufHandle_t rb = NULL;
    size_t rmt_len = 0;
    rmt_item32_t *data = NULL;

    if (!rmt) {
        log_e(" -- Inavalid Argument");
        goto err;
    }

    int channel = rmt->channel;
    rmt_get_ringbuf_handle(channel, &rb);
    if (!rb) {
        log_e(" -- Failed to get RMT ringbuffer handle");
        goto err;  
    }
    
    for(;;) {
        data = (rmt_item32_t *) xRingbufferReceive(rb, &rmt_len, portMAX_DELAY);
        if (data) {
            log_d(" -- Got %d bytes on RX Ringbuffer - CH %d", rmt_len, rmt->channel);
            rmt->rx_completed = true;  // used in rmtReceiveCompleted()
            // callback
            if (rmt->cb) {
                (rmt->cb)((uint32_t *)data, rmt_len / sizeof(rmt_item32_t), rmt->arg);
            } else {
                // stop RX -- will force a correct call with a callback pointer / new rmtReadData() / rmtReadAsync()
                rmt_rx_stop(channel);
            }
            // Async Read -- copy data to caller
            if (rmt->data_ptr && rmt->data_size) {
                uint32_t data_size = rmt->data_size;
                uint32_t read_len = rmt_len / sizeof(rmt_item32_t);
                if (read_len < rmt->data_size) data_size = read_len;
                rmt_item32_t *p = (rmt_item32_t *)rmt->data_ptr;
                for (uint32_t i = 0; i < data_size; i++) {
                    p[i] = data[i];
                }
            }
            // set events
            if (rmt->events) {
                xEventGroupSetBits(rmt->events, RMT_FLAG_RX_DONE);
            }
            vRingbufferReturnItem(rb, (void *) data);
        } // xRingbufferReceive
    } // for(;;)

err:
  vTaskDelete(NULL);    
}


static bool _rmtCreateRxTask(rmt_obj_t* rmt) 
{
    if (!rmt) {
        return false;
    }
    if (rmt->rxTaskHandle) {   // Task already created
        return false;
    }

    xTaskCreate(_rmtRxTask, "rmt_rx_task", 4096, rmt, 20, &rmt->rxTaskHandle);

    if(rmt->rxTaskHandle == NULL){
        log_e("RMT RX Task create failed");
        return false;
    }
    return true;
}

// Helper function to test if an RMT channel is correctly assigned to TX or RX, issuing an error message if necessary
// Also test RMT pointer for NULL and returns false in case it is NULL
// return true when it is correctly assigned, false otherwise
static bool _rmtCheckTXnotRX(rmt_obj_t* rmt, bool tx_not_rx)
{
    if (!rmt) {           // also returns false on NULL
        return false;
    }

    if (rmt->tx_not_rx == tx_not_rx) {  // matches expected RX/TX channel
        return true;
    }
   
    if (tx_not_rx) {    // expected TX channel
        log_e("Can't write on a RX RMT Channel");
    } else{             // expected RX channel
        log_e("Can't read on a TX RMT Channel");
    }
    return false;       // missmatched
}

/**
 * Public method definitions
 */

bool rmtSetCarrier(rmt_obj_t* rmt, bool carrier_en, bool carrier_level, uint32_t low, uint32_t high)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_TX_MODE) || low > 0xFFFF || high > 0xFFFF) {
        return false;
    }
    size_t channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    rmt_set_tx_carrier(channel, carrier_en, high, low, carrier_level);
    RMT_MUTEX_UNLOCK(channel);
    return true;
}

bool rmtSetFilter(rmt_obj_t* rmt, bool filter_en, uint32_t filter_level)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_RX_MODE) || filter_level > 0xFF) {
        return false;
    }
    size_t channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    rmt_set_rx_filter(channel, filter_en, filter_level);
    RMT_MUTEX_UNLOCK(channel);
    return true;
}

bool rmtSetRxThreshold(rmt_obj_t* rmt, uint32_t value)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_RX_MODE) || value > 0xFFFF) {
        return false;
    }
    size_t channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    rmt_set_rx_idle_thresh(channel, value);
    RMT_MUTEX_UNLOCK(channel);
    return true;
}


bool rmtDeinit(rmt_obj_t *rmt)
{
    if (!rmt) {
        return false;
    }

    // sanity check
    if (rmt != &(g_rmt_objects[rmt->channel])) {
        return false;
    }

    RMT_MUTEX_LOCK(rmt->channel);
    // force stopping rmt processing
    if (rmt->tx_not_rx) {
        rmt_tx_stop(rmt->channel);
    } else {
        rmt_rx_stop(rmt->channel);
        if(rmt->rxTaskHandle){
            vTaskDelete(rmt->rxTaskHandle);
            rmt->rxTaskHandle = NULL;
        }       
    }

    rmt_driver_uninstall(rmt->channel);

    size_t from = rmt->channel;
    size_t to = rmt->buffers + rmt->channel;
    size_t i;

    for (i = from; i < to; i++) {
        g_rmt_objects[i].allocated = false;
    }

    g_rmt_objects[from].channel = 0;
    g_rmt_objects[from].buffers = 0;
    RMT_MUTEX_UNLOCK(rmt->channel);

#if !CONFIG_DISABLE_HAL_LOCKS
    if(g_rmt_objlocks[from] != NULL) {
        vSemaphoreDelete(g_rmt_objlocks[from]);
        g_rmt_objlocks[from] = NULL;
    }
#endif

    return true;
}

bool rmtLoop(rmt_obj_t* rmt, rmt_data_t* data, size_t size)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_TX_MODE)) {
        return false;
    }
    int channel = rmt->channel;
    RMT_MUTEX_LOCK(channel);
    rmt_tx_stop(channel);
    rmt_set_tx_loop_mode(channel, true);
    rmt_write_items(channel, (const rmt_item32_t *)data, size, false);
    RMT_MUTEX_UNLOCK(channel);
    return true;
}

bool rmtWrite(rmt_obj_t* rmt, rmt_data_t* data, size_t size)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_TX_MODE)) {
        return false;
    }
    int channel = rmt->channel;
    RMT_MUTEX_LOCK(channel);
    rmt_tx_stop(channel);
    rmt_set_tx_loop_mode(channel, false);
    rmt_write_items(channel, (const rmt_item32_t *)data, size, false);
    RMT_MUTEX_UNLOCK(channel);
    return true;
}

bool rmtWriteBlocking(rmt_obj_t* rmt, rmt_data_t* data, size_t size)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_TX_MODE)) {
        return false;
    }
    int channel = rmt->channel;
    RMT_MUTEX_LOCK(channel);
    rmt_tx_stop(channel);
    rmt_set_tx_loop_mode(channel, false);
    rmt_write_items(channel, (const rmt_item32_t *)data, size, true);
    RMT_MUTEX_UNLOCK(channel);
    return true;
}

bool rmtReadData(rmt_obj_t* rmt, uint32_t* data, size_t size)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_RX_MODE)) {
        return false;
    }
    rmtReadAsync(rmt, (rmt_data_t*) data, size, NULL, false, 0);
    return true;
}


bool rmtBeginReceive(rmt_obj_t* rmt)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_RX_MODE)) {
        return false;
    }
    int channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    rmt_set_memory_owner(channel, RMT_MEM_OWNER_RX);
    rmt_rx_start(channel, true);
    rmt->rx_completed = false;
    RMT_MUTEX_UNLOCK(channel);
    return true;
}

bool rmtReceiveCompleted(rmt_obj_t* rmt)
{
    if (!rmt) {
        return false;
    }

    return rmt->rx_completed;
}

bool rmtRead(rmt_obj_t* rmt, rmt_rx_data_cb_t cb, void * arg)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_RX_MODE)) {
        return false;
    }
    int channel = rmt->channel;

    rmt->arg = arg;
    rmt->cb = cb;

    RMT_MUTEX_LOCK(channel);
    // cb as NULL is a way to cancel the callback process
    if (cb == NULL) {        
        rmt_rx_stop(channel);
        return true;
    }
    // Start a read process but now with a call back function
    rmt_set_memory_owner(channel, RMT_MEM_OWNER_RX);
    rmt_rx_start(channel, true);
    rmt->rx_completed = false;
    _rmtCreateRxTask(rmt);
    RMT_MUTEX_UNLOCK(channel);
    return true;
}

bool rmtEnd(rmt_obj_t* rmt) 
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    if (rmt->tx_not_rx) {
        rmt_tx_stop(channel);
    } else {
        rmt_rx_stop(channel);
        rmt->rx_completed = true;
    }
    RMT_MUTEX_UNLOCK(channel);
    return  true;
}

bool rmtReadAsync(rmt_obj_t* rmt, rmt_data_t* data, size_t size, void* eventFlag, bool waitForData, uint32_t timeout)
{
    if (!_rmtCheckTXnotRX(rmt, RMT_RX_MODE)) {
        return false;
    }
    int channel = rmt->channel;

    // No limit on size with IDF ;-)
    //if (g_rmt_objects[channel].buffers < size/SOC_RMT_MEM_WORDS_PER_CHANNEL) {
    //    return false;
    //}

    RMT_MUTEX_LOCK(channel);
    if (eventFlag) {
        xEventGroupClearBits(eventFlag, RMT_FLAGS_ALL);
    }
    // if NULL, no problems - rmtReadAsync works as a plain rmtReadData()
    rmt->events = eventFlag;  

    // if NULL, no problems - task will take care of it
    rmt->data_ptr = (uint32_t*)data;
    rmt->data_size = size;
 
    // Start a read process
    rmt_set_memory_owner(channel, RMT_MEM_OWNER_RX); 
    rmt_rx_start(channel, true);
    rmt->rx_completed = false;
    _rmtCreateRxTask(rmt);
    RMT_MUTEX_UNLOCK(channel);

    // wait for data if requested so
    if (waitForData && eventFlag) {
        xEventGroupWaitBits(eventFlag, RMT_FLAGS_ALL,
                            pdTRUE /* clear on exit */, pdFALSE /* wait for all bits */, timeout);
    }
    return true;
}

float rmtSetTick(rmt_obj_t* rmt, float tick)
{
    if (!rmt) {
        return false;
    }    
    size_t channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    // RMT_BASECLK_REF (1MHz) is not supported in IDF upon Programmming Guide
    // Only APB works
    rmt_set_source_clk(channel, RMT_BASECLK_APB);   
    int apb_div = _LIMIT(tick/12.5f, 256);
    float apb_tick = 12.5f * apb_div;
    
    rmt_set_clk_div(channel, apb_div & 0xFF);
    RMT_MUTEX_UNLOCK(channel);
    return apb_tick;
}

rmt_obj_t* rmtInit(int pin, bool tx_not_rx, rmt_reserve_memsize_t memsize)
{
    int buffers = memsize;
    rmt_obj_t* rmt = NULL;
    size_t i = 0;
    size_t j = 0;

    // create common block mutex for protecting allocs from multiple threads
    if (!g_rmt_block_lock) {
        g_rmt_block_lock = xSemaphoreCreateMutex();
    }
    // lock
    while (xSemaphoreTake(g_rmt_block_lock, portMAX_DELAY) != pdPASS) {}

    // Some SoC may have fixed channel numbers for TX and RX - example: ESP32C3
    uint8_t ch_start, ch_end;
    if (tx_not_rx) {
        ch_start = RMT_TX_CH_START;
        ch_end = RMT_TX_CH_END;
    } else {
        ch_start = RMT_RX_CH_START;
        ch_end = RMT_RX_CH_END;
    }
    for (i=ch_start; i<=ch_end; i++) {
        for (j=0; j<buffers && i+j <= ch_end; j++) {
            // if the space is ocupied break and continue on other channel
            if (g_rmt_objects[i+j].allocated) {
                i += j; // continue searching from latter channel
                break;
            }
        }
        if (j == buffers) {
            // found a space in channel descriptors
            break;
        }
    }
    if (i == MAX_CHANNELS || i+j > MAX_CHANNELS || j != buffers)  {
        xSemaphoreGive(g_rmt_block_lock);
        log_e("rmInit Failed - not enough channels");
        return NULL;
    }
    
    // A suitable channel has been found, it has to block its resources in our internal data strucuture
    size_t channel = i;
    rmt = _rmtAllocate(pin, i, buffers);

    xSemaphoreGive(g_rmt_block_lock);

    rmt->buffers = buffers;
    rmt->channel = channel;
    rmt->arg = NULL;
    rmt->cb = NULL;
    rmt->data_ptr = NULL;
    rmt->data_size = 0;
    rmt->rx_completed = false;
    rmt->events = NULL;
    rmt->tx_not_rx = tx_not_rx;

#if !CONFIG_DISABLE_HAL_LOCKS
    if(g_rmt_objlocks[channel] == NULL) {
        g_rmt_objlocks[channel] = xSemaphoreCreateMutex();
        if(g_rmt_objlocks[channel] == NULL) {
            return NULL;
        }
    }
#endif

    RMT_MUTEX_LOCK(channel);
    esp_err_t esp_err_code = ESP_OK;

    if (tx_not_rx) {
        rmt_config_t config = RMT_DEFAULT_ARD_CONFIG_TX(pin, channel, buffers);
        esp_err_code = rmt_config(&config);
        if (esp_err_code == ESP_OK) 
            esp_err_code = rmt_driver_install(channel, 0, 0);
        log_d(" -- %s RMT - CH %d - %d RAM Blocks - pin %d", tx_not_rx?"TX":"RX", channel, buffers, pin);
    } else {
        rmt_config_t config = RMT_DEFAULT_ARD_CONFIG_RX(pin, channel, buffers);
        esp_err_code = rmt_config(&config);
        if (esp_err_code == ESP_OK) 
            esp_err_code = rmt_driver_install(channel, 1024, 0);
        if (esp_err_code == ESP_OK) 
            esp_err_code = rmt_set_memory_owner(channel, RMT_MEM_OWNER_RX);
        log_d(" -- %s RMT - CH %d - %d RAM Blocks - pin %d", tx_not_rx?"TX":"RX", channel, buffers, pin);
    } 

    RMT_MUTEX_UNLOCK(channel);

    if (esp_err_code == ESP_OK) {
        return rmt;
    } else {
        log_e("RMT failed to initilize.");
        return NULL;
    }
}
