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

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp32-hal.h"
#include "esp8266-compat.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_reg.h"

#include "esp32-hal-rmt.h"
#include "driver/periph_ctrl.h"

#include "soc/rmt_struct.h"
#include "esp_intr_alloc.h"

/**
 * Internal macros
 */
#define MAX_CHANNELS 8
#define MAX_DATA_PER_CHANNEL 64
#define MAX_DATA_PER_ITTERATION 62
#define _ABS(a) (a>0?a:-a)
#define _LIMIT(a,b) (a>b?b:a)
#define __INT_TX_END     (1)
#define __INT_RX_END     (2)
#define __INT_ERROR      (4)
#define __INT_THR_EVNT   (1<<24)

#define _INT_TX_END(channel) (__INT_TX_END<<(channel*3))
#define _INT_RX_END(channel) (__INT_RX_END<<(channel*3))
#define _INT_ERROR(channel)  (__INT_ERROR<<(channel*3))
#define _INT_THR_EVNT(channel)  ((__INT_THR_EVNT)<<(channel))

#if CONFIG_DISABLE_HAL_LOCKS
# define RMT_MUTEX_LOCK(channel)
# define RMT_MUTEX_UNLOCK(channel)
#else
# define RMT_MUTEX_LOCK(channel)    do {} while (xSemaphoreTake(g_rmt_objlocks[channel], portMAX_DELAY) != pdPASS)
# define RMT_MUTEX_UNLOCK(channel)  xSemaphoreGive(g_rmt_objlocks[channel])
#endif /* CONFIG_DISABLE_HAL_LOCKS */

#define _RMT_INTERNAL_DEBUG
#ifdef _RMT_INTERNAL_DEBUG
# define DEBUG_INTERRUPT_START(pin)    digitalWrite(pin, 1);
# define DEBUG_INTERRUPT_END(pin)      digitalWrite(pin, 0);
#else
# define DEBUG_INTERRUPT_START(pin)
# define DEBUG_INTERRUPT_END(pin)
#endif /* _RMT_INTERNAL_DEBUG */

/**
 * Typedefs for internal stuctures, enums
 */
typedef enum {
    E_NO_INTR = 0,
    E_TX_INTR = 1,
    E_TXTHR_INTR = 2,
    E_RX_INTR = 4,
} intr_mode_t;

typedef enum {
    E_INACTIVE   = 0,
    E_FIRST_HALF = 1,
    E_LAST_DATA  = 2,
    E_END_TRANS  = 4,
    E_SET_CONTI =  8,
} transaction_state_t;

struct rmt_obj_s
{
    bool allocated;
    EventGroupHandle_t events;
    int pin;
    int channel;
    bool tx_not_rx;
    int buffers;
    int data_size;
    uint32_t* data_ptr;
    intr_mode_t intr_mode;
    transaction_state_t tx_state;
    rmt_rx_data_cb_t cb;
    bool data_alloc;
};

/**
 * Internal variables for channel descriptors
 */
static xSemaphoreHandle g_rmt_objlocks[MAX_CHANNELS] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static rmt_obj_t g_rmt_objects[MAX_CHANNELS] = {
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
    { false, NULL, 0, 0, 0, 0, 0, NULL, E_NO_INTR, E_INACTIVE, NULL, false},
};

/**
 * Internal variables for driver data
 */
static  intr_handle_t intr_handle;

static bool periph_enabled = false;

static xSemaphoreHandle g_rmt_block_lock = NULL;

/**
 * Internal method (private) declarations
 */
static void _initPin(int pin, int channel, bool tx_not_rx);

static bool _rmtSendOnce(rmt_obj_t* rmt, rmt_data_t* data, size_t size, bool continuous);

static void IRAM_ATTR _rmt_isr(void* arg);

static rmt_obj_t* _rmtAllocate(int pin, int from, int size);

static void _initPin(int pin, int channel, bool tx_not_rx);

static int IRAM_ATTR _rmt_get_mem_len(uint8_t channel);

static void IRAM_ATTR _rmt_tx_mem_first(uint8_t ch);

static void IRAM_ATTR _rmt_tx_mem_second(uint8_t ch);


/**
 * Public method definitions
 */
bool rmtSetCarrier(rmt_obj_t* rmt, bool carrier_en, bool carrier_level, uint32_t low, uint32_t high)
{
    if (!rmt || low > 0xFFFF || high > 0xFFFF) {
        return false;
    }
    size_t channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);

    RMT.carrier_duty_ch[channel].low = low;
    RMT.carrier_duty_ch[channel].high = high;
    RMT.conf_ch[channel].conf0.carrier_en = carrier_en;
    RMT.conf_ch[channel].conf0.carrier_out_lv = carrier_level;

    RMT_MUTEX_UNLOCK(channel);

    return true;

}

bool rmtSetFilter(rmt_obj_t* rmt, bool filter_en, uint32_t filter_level)
{
    if (!rmt || filter_level > 0xFF) {
        return false;
    }
    size_t channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);

    RMT.conf_ch[channel].conf1.rx_filter_thres = filter_level;
    RMT.conf_ch[channel].conf1.rx_filter_en = filter_en;

    RMT_MUTEX_UNLOCK(channel);

    return true;

}

bool rmtSetRxThreshold(rmt_obj_t* rmt, uint32_t value)
{
    if (!rmt || value > 0xFFFF) {
        return false;
    }
    size_t channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    RMT.conf_ch[channel].conf0.idle_thres = value;
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

    size_t from = rmt->channel;
    size_t to = rmt->buffers + rmt->channel;
    size_t i;

#if !CONFIG_DISABLE_HAL_LOCKS
    if(g_rmt_objlocks[from] != NULL) {
        vSemaphoreDelete(g_rmt_objlocks[from]);
    }
#endif

    if (g_rmt_objects[from].data_alloc) {
        free(g_rmt_objects[from].data_ptr);
    }
    
    for (i = from; i < to; i++) {
        g_rmt_objects[i].allocated = false;
    }

    g_rmt_objects[from].channel = 0;
    g_rmt_objects[from].buffers = 0;

    return true;
}

bool rmtLoop(rmt_obj_t* rmt, rmt_data_t* data, size_t size)
{
    if (!rmt) {
        return false;
    }

    int channel = rmt->channel;
    int allocated_size = MAX_DATA_PER_CHANNEL * rmt->buffers;

    if (size > allocated_size) {
        return false;
    }
    return _rmtSendOnce(rmt, data, size, true);
}

bool rmtWrite(rmt_obj_t* rmt, rmt_data_t* data, size_t size)
{
    if (!rmt) {
        return false;
    }
    
    int channel = rmt->channel;
    int allocated_size = MAX_DATA_PER_CHANNEL * rmt->buffers;

    if (size > allocated_size) {

        int half_tx_nr = MAX_DATA_PER_ITTERATION/2;
        RMT_MUTEX_LOCK(channel);
        // setup interrupt handler if not yet installed for half and full tx
        if (!intr_handle) {
            esp_intr_alloc(ETS_RMT_INTR_SOURCE, (int)ESP_INTR_FLAG_IRAM, _rmt_isr, NULL, &intr_handle);
        }

        rmt->data_size = size - MAX_DATA_PER_ITTERATION;
        rmt->data_ptr = ((uint32_t*)data) + MAX_DATA_PER_ITTERATION;
        rmt->intr_mode = E_TX_INTR | E_TXTHR_INTR;
        rmt->tx_state = E_SET_CONTI | E_FIRST_HALF;

        // init the tx limit for interruption
        RMT.tx_lim_ch[channel].limit = half_tx_nr+2;
        // reset memory pointer
        RMT.conf_ch[channel].conf1.apb_mem_rst = 1;
        RMT.conf_ch[channel].conf1.apb_mem_rst = 0;
        RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
        RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
        RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
        RMT.conf_ch[channel].conf1.mem_wr_rst = 0;

        // set the tx end mark
        RMTMEM.chan[channel].data32[MAX_DATA_PER_ITTERATION].val = 0;

        // clear and enable both Tx completed and half tx event
        RMT.int_clr.val = _INT_TX_END(channel);
        RMT.int_clr.val = _INT_THR_EVNT(channel);
        RMT.int_clr.val = _INT_ERROR(channel);
        
        RMT.int_ena.val |= _INT_TX_END(channel);
        RMT.int_ena.val |= _INT_THR_EVNT(channel);
        RMT.int_ena.val |= _INT_ERROR(channel);

        RMT_MUTEX_UNLOCK(channel);

        // start the transation
        return _rmtSendOnce(rmt, data, MAX_DATA_PER_ITTERATION, false);
    } else {
        // use one-go mode if data fits one buffer 
        return _rmtSendOnce(rmt, data, size, false);
    }
}

bool rmtReadData(rmt_obj_t* rmt, uint32_t* data, size_t size)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;

    if (g_rmt_objects[channel].buffers < size/MAX_DATA_PER_CHANNEL) {
        return false;
    }

    size_t i;
    volatile uint32_t* rmt_mem_ptr = &(RMTMEM.chan[channel].data32[0].val);
    for (i=0; i<size; i++) {
        data[i] = *rmt_mem_ptr++;
    }

    return true;
}

bool rmtBeginReceive(rmt_obj_t* rmt)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;

    RMT.int_clr.val = _INT_ERROR(channel);
    RMT.int_ena.val |= _INT_ERROR(channel);

    RMT.conf_ch[channel].conf1.mem_owner = 1;
    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
    RMT.conf_ch[channel].conf1.rx_en = 1;

    return true;
}

bool rmtReceiveCompleted(rmt_obj_t* rmt)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;

    if (RMT.int_raw.val&_INT_RX_END(channel)) {
        // RX end flag
        RMT.int_clr.val = _INT_RX_END(channel);
        return true;
    } else {
        return false;
    }
}

bool rmtRead(rmt_obj_t* rmt, rmt_rx_data_cb_t cb)
{
    if (!rmt && !cb) {
        return false;
    }
    int channel = rmt->channel;

    RMT_MUTEX_LOCK(channel);
    rmt->intr_mode = E_RX_INTR;
    rmt->tx_state = E_FIRST_HALF;
    rmt->cb = cb;
    // allocate internally two buffers which would alternate
    if (!rmt->data_alloc) {
        rmt->data_ptr = (uint32_t*)malloc(2*MAX_DATA_PER_CHANNEL*(rmt->buffers)*sizeof(uint32_t));
        rmt->data_size = MAX_DATA_PER_CHANNEL*rmt->buffers;
        rmt->data_alloc = true;
    }

    RMT.conf_ch[channel].conf1.mem_owner = 1;

    RMT.int_clr.val = _INT_RX_END(channel);
    RMT.int_clr.val = _INT_ERROR(channel);
    
    RMT.int_ena.val |= _INT_RX_END(channel);
    RMT.int_ena.val |= _INT_ERROR(channel);

    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;

    RMT.conf_ch[channel].conf1.rx_en = 1;
    RMT_MUTEX_UNLOCK(channel);

    return true;
}

bool rmtReadAsync(rmt_obj_t* rmt, rmt_data_t* data, size_t size, void* eventFlag, bool waitForData, uint32_t timeout)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;

    if (g_rmt_objects[channel].buffers < size/MAX_DATA_PER_CHANNEL) {
        return false;
    }

    if (eventFlag) {
        xEventGroupClearBits(eventFlag, RMT_FLAGS_ALL);
        rmt->events = eventFlag;
    }

    if (data && size>0) {
        rmt->data_ptr = (uint32_t*)data;
        rmt->data_size = size;
    }

    RMT_MUTEX_LOCK(channel);
    rmt->intr_mode = E_RX_INTR;

    RMT.conf_ch[channel].conf1.mem_owner = 1;

    RMT.int_clr.val = _INT_RX_END(channel);
    RMT.int_clr.val = _INT_ERROR(channel);
    
    RMT.int_ena.val |= _INT_RX_END(channel);
    RMT.int_ena.val |= _INT_ERROR(channel);

    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;

    RMT.conf_ch[channel].conf1.rx_en = 1;
    RMT_MUTEX_UNLOCK(channel);

    // wait for data if requested so
    if (waitForData && eventFlag) {
        uint32_t flags = xEventGroupWaitBits(eventFlag, RMT_FLAGS_ALL,
                            pdTRUE /* clear on exit */, pdFALSE /* wait for all bits */, timeout);
        if (flags & RMT_FLAG_ERROR) {
            return false;
        }
    }

    return true;
}

float rmtSetTick(rmt_obj_t* rmt, float tick)
{
    if (!rmt) {
        return false;
    }
    /*
     divider field span from 1 (smallest), 2, 3, ... , 0xFF, 0x00 (highest)
     * rmt tick from 1/80M -> 12.5ns  (1x)    div_cnt = 0x01
                              3.2 us (256x)   div_cnt = 0x00
     * rmt tick for 1 MHz  -> 1us (1x)        div_cnt = 0x01
                              256us (256x)    div_cnt = 0x00
    */
    int apb_div = _LIMIT(tick/12.5, 256);
    int ref_div = _LIMIT(tick/1000, 256);

    float apb_tick = 12.5 * apb_div;
    float ref_tick = 1000.0 * ref_div;
    
    size_t channel = rmt->channel;

    if (_ABS(apb_tick - tick) < _ABS(ref_tick - tick)) {
        RMT.conf_ch[channel].conf0.div_cnt = apb_div & 0xFF;
        RMT.conf_ch[channel].conf1.ref_always_on = 1;
        return apb_tick;
    } else {
        RMT.conf_ch[channel].conf0.div_cnt = ref_div & 0xFF;
        RMT.conf_ch[channel].conf1.ref_always_on = 0;
        return ref_tick;
    }
}

rmt_obj_t* rmtInit(int pin, bool tx_not_rx, rmt_reserve_memsize_t memsize)
{
    int buffers = memsize;
    rmt_obj_t* rmt;
    size_t i;
    size_t j;

    // create common block mutex for protecting allocs from multiple threads
    if (!g_rmt_block_lock) {
        g_rmt_block_lock = xSemaphoreCreateMutex();
    }
    // lock
    while (xSemaphoreTake(g_rmt_block_lock, portMAX_DELAY) != pdPASS) {}

    for (i=0; i<MAX_CHANNELS; i++) {
        for (j=0; j<buffers && i+j < MAX_CHANNELS; j++) {
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
        return NULL;
    }
    rmt = _rmtAllocate(pin, i, buffers);

    xSemaphoreGive(g_rmt_block_lock);

    size_t channel = i;

#if !CONFIG_DISABLE_HAL_LOCKS
    if(g_rmt_objlocks[channel] == NULL) {
        g_rmt_objlocks[channel] = xSemaphoreCreateMutex();
        if(g_rmt_objlocks[channel] == NULL) {
            return NULL;
        }
    }
#endif

    RMT_MUTEX_LOCK(channel);

    rmt->pin = pin;
    rmt->tx_not_rx = tx_not_rx;
    rmt->buffers =buffers;
    rmt->channel = channel;
    _initPin(pin, channel, tx_not_rx);

    // Initialize the registers in default mode:
    // - no carrier, filter
    // - timebase tick of 1us
    // - idle threshold set to 0x8000 (max pulse width + 1)
    RMT.conf_ch[channel].conf0.div_cnt = 1;
    RMT.conf_ch[channel].conf0.mem_size = buffers;
    RMT.conf_ch[channel].conf0.carrier_en = 0;
    RMT.conf_ch[channel].conf0.carrier_out_lv = 0;
    RMT.conf_ch[channel].conf0.mem_pd = 0;

    RMT.conf_ch[channel].conf0.idle_thres = 0x80;
    RMT.conf_ch[channel].conf1.rx_en = 0;
    RMT.conf_ch[channel].conf1.tx_conti_mode = 0;
    RMT.conf_ch[channel].conf1.ref_cnt_rst = 0;
    RMT.conf_ch[channel].conf1.rx_filter_en = 0;
    RMT.conf_ch[channel].conf1.rx_filter_thres = 0;
    RMT.conf_ch[channel].conf1.idle_out_lv = 0;     // signal level for idle
    RMT.conf_ch[channel].conf1.idle_out_en = 1;     // enable idle
    RMT.conf_ch[channel].conf1.ref_always_on = 0;     // base clock
    RMT.apb_conf.fifo_mask = 1;

    if (tx_not_rx) {
        // RMT.conf_ch[channel].conf1.rx_en = 0;
        RMT.conf_ch[channel].conf1.mem_owner = 0;
        RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    } else {
        // RMT.conf_ch[channel].conf1.rx_en = 1;
        RMT.conf_ch[channel].conf1.mem_owner = 1;
        RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
    }

    // install interrupt if at least one channel is active
    if (!intr_handle) {
        esp_intr_alloc(ETS_RMT_INTR_SOURCE, (int)ESP_INTR_FLAG_IRAM, _rmt_isr, NULL, &intr_handle);
    }
    RMT_MUTEX_UNLOCK(channel);

    return rmt;
}

/**
 * Private methods definitions
 */
bool _rmtSendOnce(rmt_obj_t* rmt, rmt_data_t* data, size_t size, bool continuous)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;
    RMT.apb_conf.fifo_mask = 1;
    if (data && size>0) {
        size_t i;
        volatile uint32_t* rmt_mem_ptr = &(RMTMEM.chan[channel].data32[0].val);
        for (i = 0; i < size; i++) {
            *rmt_mem_ptr++ = data[i].val;
        }
        // tx end mark
        RMTMEM.chan[channel].data32[size].val = 0;
    }

    RMT_MUTEX_LOCK(channel);
    RMT.conf_ch[channel].conf1.tx_conti_mode = continuous;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel].conf1.tx_start = 1;
    RMT_MUTEX_UNLOCK(channel);

    return true;
}


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


static void _initPin(int pin, int channel, bool tx_not_rx) 
{
    if (!periph_enabled) {
        periph_enabled = true;
        periph_module_enable( PERIPH_RMT_MODULE );
    }
    if (tx_not_rx) {
        pinMode(pin, OUTPUT);
        pinMatrixOutAttach(pin, RMT_SIG_OUT0_IDX + channel, 0, 0);
    } else {
        pinMode(pin, INPUT);
        pinMatrixInAttach(pin, RMT_SIG_IN0_IDX + channel, 0);

    }
}


static void IRAM_ATTR _rmt_isr(void* arg)
{
    int intr_val = RMT.int_st.val;
    size_t ch;
    for (ch = 0; ch < MAX_CHANNELS; ch++) {

        if (intr_val & _INT_RX_END(ch)) {
            // clear the flag
            RMT.int_clr.val = _INT_RX_END(ch);
            RMT.int_ena.val &= ~_INT_RX_END(ch);

            if ((g_rmt_objects[ch].intr_mode) & E_RX_INTR) {
                if (g_rmt_objects[ch].events) {
                    xEventGroupSetBits(g_rmt_objects[ch].events, RMT_FLAG_RX_DONE);
                }
                if (g_rmt_objects[ch].data_ptr && g_rmt_objects[ch].data_size > 0) {
                    size_t i;
                    uint32_t * data = g_rmt_objects[ch].data_ptr;
                    // in case of callback, provide switching between memories
                    if (g_rmt_objects[ch].cb) {
                        if (g_rmt_objects[ch].tx_state & E_FIRST_HALF) {
                            g_rmt_objects[ch].tx_state &= ~E_FIRST_HALF;
                        } else {
                            g_rmt_objects[ch].tx_state |= E_FIRST_HALF;
                            data += MAX_DATA_PER_CHANNEL*(g_rmt_objects[ch].buffers);
                        }
                    }
                    uint32_t *data_received = data;
                    for (i = 0; i < g_rmt_objects[ch].data_size; i++ ) {
                        *data++ = RMTMEM.chan[ch].data32[i].val;
                    }
                    if (g_rmt_objects[ch].cb) {
                        // actually received data ptr                        
                        (g_rmt_objects[ch].cb)(data_received, _rmt_get_mem_len(ch));

                        // restart the reception
                        RMT.conf_ch[ch].conf1.mem_owner = 1;
                        RMT.conf_ch[ch].conf1.mem_wr_rst = 1;
                        RMT.conf_ch[ch].conf1.rx_en = 1;
                        RMT.int_ena.val |= _INT_RX_END(ch);
                    } else {
                        // if not callback provide, expect only one Rx
                        g_rmt_objects[ch].intr_mode &= ~E_RX_INTR;
                    }
                }
            } else {
                // Report error and disable Rx interrupt
                log_e("Unexpected Rx interrupt!\n");  // TODO: eplace messages with log_X
                RMT.int_ena.val &= ~_INT_RX_END(ch);
            }


        }

        if (intr_val & _INT_ERROR(ch)) {
            // clear the flag
            RMT.int_clr.val = _INT_ERROR(ch);
            RMT.int_ena.val &= ~_INT_ERROR(ch);
            // report error 
            log_e("RMT Error %d!\n", ch);
            if (g_rmt_objects[ch].events) {
                xEventGroupSetBits(g_rmt_objects[ch].events, RMT_FLAG_ERROR);
            }
            // reset memory
            RMT.conf_ch[ch].conf1.mem_rd_rst = 1;
            RMT.conf_ch[ch].conf1.mem_rd_rst = 0;
            RMT.conf_ch[ch].conf1.mem_wr_rst = 1;
            RMT.conf_ch[ch].conf1.mem_wr_rst = 0;
        }

        if (intr_val & _INT_TX_END(ch)) {

            RMT.int_clr.val = _INT_TX_END(ch);
            _rmt_tx_mem_second(ch);
        }

        if (intr_val & _INT_THR_EVNT(ch)) {
            // clear the flag
            RMT.int_clr.val = _INT_THR_EVNT(ch);

            // initial setup of continuous mode
            if (g_rmt_objects[ch].tx_state & E_SET_CONTI) {
                RMT.conf_ch[ch].conf1.tx_conti_mode = 1;
                g_rmt_objects[ch].intr_mode &= ~E_SET_CONTI;
            }
            _rmt_tx_mem_first(ch);
        }
    }
}

static void IRAM_ATTR _rmt_tx_mem_second(uint8_t ch)
{
    DEBUG_INTERRUPT_START(4)
    uint32_t* data = g_rmt_objects[ch].data_ptr;
    int half_tx_nr = MAX_DATA_PER_ITTERATION/2;
    int i;

    RMT.tx_lim_ch[ch].limit = half_tx_nr+2;
    RMT.int_clr.val = _INT_THR_EVNT(ch);
    RMT.int_ena.val |= _INT_THR_EVNT(ch);

    g_rmt_objects[ch].tx_state |= E_FIRST_HALF;

    if (data) {
        int remaining_size = g_rmt_objects[ch].data_size;
        // will the remaining data occupy the entire halfbuffer
        if (remaining_size > half_tx_nr) {
            for (i = 0; i < half_tx_nr; i++) {
                RMTMEM.chan[ch].data32[half_tx_nr+i].val = data[i];
            }
            g_rmt_objects[ch].data_size -= half_tx_nr;
            g_rmt_objects[ch].data_ptr += half_tx_nr;
        } else {
            for (i = 0; i < half_tx_nr; i++) {
                if (i < remaining_size) {
                    RMTMEM.chan[ch].data32[half_tx_nr+i].val = data[i];
                } else {
                    RMTMEM.chan[ch].data32[half_tx_nr+i].val = 0x000F000F;
                }
            }
            g_rmt_objects[ch].data_ptr = NULL;

        }
    } else if   ((!(g_rmt_objects[ch].tx_state & E_LAST_DATA)) &&
                 (!(g_rmt_objects[ch].tx_state & E_END_TRANS))) {
        for (i = 0; i < half_tx_nr; i++) {
            RMTMEM.chan[ch].data32[half_tx_nr+i].val = 0x000F000F;
        }
        RMTMEM.chan[ch].data32[half_tx_nr+i].val = 0;
        g_rmt_objects[ch].tx_state |= E_LAST_DATA;
        RMT.conf_ch[ch].conf1.tx_conti_mode = 0;
    } else {
        log_d("RMT Tx finished %d!\n", ch);
        RMT.conf_ch[ch].conf1.tx_conti_mode = 0;
        RMT.int_ena.val &= ~_INT_TX_END(ch);
        RMT.int_ena.val &= ~_INT_THR_EVNT(ch);
        g_rmt_objects[ch].intr_mode = E_NO_INTR;
        g_rmt_objects[ch].tx_state = E_INACTIVE;
    }
    DEBUG_INTERRUPT_END(4);
}

static void IRAM_ATTR _rmt_tx_mem_first(uint8_t ch)
{
    DEBUG_INTERRUPT_START(2);
    uint32_t* data = g_rmt_objects[ch].data_ptr;
    int half_tx_nr = MAX_DATA_PER_ITTERATION/2;
    int i;
    RMT.int_ena.val &= ~_INT_THR_EVNT(ch);
    RMT.tx_lim_ch[ch].limit = 0;

    if (data) {
        int remaining_size = g_rmt_objects[ch].data_size;

        // will the remaining data occupy the entire halfbuffer
        if (remaining_size > half_tx_nr) {
            RMTMEM.chan[ch].data32[0].val = data[0] - 1;
            for (i = 1; i < half_tx_nr; i++) {
                RMTMEM.chan[ch].data32[i].val = data[i];
            }
            g_rmt_objects[ch].tx_state &= ~E_FIRST_HALF;
            // turn off the treshold interrupt
            RMT.int_ena.val &= ~_INT_THR_EVNT(ch);
            RMT.tx_lim_ch[ch].limit = 0;
            g_rmt_objects[ch].data_size -= half_tx_nr;
            g_rmt_objects[ch].data_ptr += half_tx_nr;
        } else {
            RMTMEM.chan[ch].data32[0].val = data[0] - 1;
            for (i = 1; i < half_tx_nr; i++) {
                if (i < remaining_size) {
                    RMTMEM.chan[ch].data32[i].val = data[i];
                } else {
                    RMTMEM.chan[ch].data32[i].val = 0x000F000F;
                }
            }

            g_rmt_objects[ch].tx_state &= ~E_FIRST_HALF;
            g_rmt_objects[ch].data_ptr = NULL;
        }
    } else { 
        for (i = 0; i < half_tx_nr; i++) {
            RMTMEM.chan[ch].data32[i].val = 0x000F000F;
        }
        RMTMEM.chan[ch].data32[i].val = 0;

        g_rmt_objects[ch].tx_state &= ~E_FIRST_HALF;
        RMT.tx_lim_ch[ch].limit = 0;
        g_rmt_objects[ch].tx_state |= E_LAST_DATA;
        RMT.conf_ch[ch].conf1.tx_conti_mode = 0;
    }
    DEBUG_INTERRUPT_END(2);
}

static int IRAM_ATTR _rmt_get_mem_len(uint8_t channel)
{
    int block_num = RMT.conf_ch[channel].conf0.mem_size;
    int item_block_len = block_num * 64;
    volatile rmt_item32_t* data = RMTMEM.chan[channel].data32;
    int idx;
    for(idx = 0; idx < item_block_len; idx++) {
        if(data[idx].duration0 == 0) {
            return idx;
        } else if(data[idx].duration1 == 0) {
            return idx + 1;
        }
    }
    return idx;
}
