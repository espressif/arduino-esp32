#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp32-hal.h"
#include "esp8266-compat.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_reg.h"

#include "esp32-hal-rmt.h"
#include "driver/periph_ctrl.h"

#include "soc/rmt_struct.h"
#include "esp_intr_alloc.h"


#define MAX_CHANNELS 8
#define MAX_DATA_PER_CHANNEL 64
#define MAX_DATA_PER_ITTERATION 40
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

typedef enum {
    e_no_intr = 0,
    e_tx_intr = 1,
    e_txthr_intr = 2,
    e_rx_intr = 4,
} intr_mode_t;

typedef enum {
    e_inactive   = 0,
    e_first_half = 1,
    e_last_data  = 2,
    e_end_trans  = 4,
    e_set_conti =  8,
} transaction_state_t;


struct rmt_obj_s
{
    bool allocated;
    EventGroupHandle_t events;
    int pin;
    int channel;
    bool tx_not_rx;
    int buffers;
    int remaining_to_send;
    uint32_t* remaining_ptr;
    intr_mode_t intr_mode;
    transaction_state_t tx_state;
};

static rmt_obj_t g_rmt_objects[MAX_CHANNELS] = {
    { false, NULL, 0, 0, 0, 0, 0, NULL, e_no_intr, e_inactive},
    { false, NULL, 0, 0, 0, 0, 0, NULL, e_no_intr, e_inactive},
};

static  intr_handle_t intr_handle;

static bool periph_enabled = false;

static void _initPin(int pin, int channel, bool tx_not_rx);

static void IRAM_ATTR _rmt_isr(void* arg);


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
    
    for (i = from; i < to; i++) {
        g_rmt_objects[i].allocated = false;
    }
    g_rmt_objects[from].channel = 0;
    g_rmt_objects[from].buffers = 0;

    return true;
}

bool rmtSendQueued(rmt_obj_t* rmt, uint32_t* data, size_t size)
{
    if (!rmt) {
        return false;
    }
    
    int channel = rmt->channel;

    if (size > MAX_DATA_PER_ITTERATION) {
    // if (size > MAX_DATA_PER_CHANNEL) {

        // setup interrupt handler if not yet installed for half and full tx
        if (!intr_handle) {
            esp_intr_alloc(ETS_RMT_INTR_SOURCE, (int)ESP_INTR_FLAG_IRAM, _rmt_isr, NULL, &intr_handle);
        }

        int half_tx_nr = MAX_DATA_PER_ITTERATION/2;
        rmt->remaining_to_send = size - MAX_DATA_PER_ITTERATION;
        rmt->remaining_ptr = data + MAX_DATA_PER_ITTERATION;
        rmt->intr_mode = e_tx_intr | e_txthr_intr; 
        rmt->tx_state = e_set_conti | e_first_half;

        // init the tx limit for interruption
        RMT.tx_lim_ch[channel].limit = half_tx_nr;
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
        RMT.int_clr.val |= _INT_TX_END(channel);
        RMT.int_clr.val |= _INT_THR_EVNT(channel);
        RMT.int_clr.val |= _INT_ERROR(channel);
        
        RMT.int_ena.val |= _INT_TX_END(channel);
        RMT.int_ena.val |= _INT_THR_EVNT(channel);
        RMT.int_ena.val |= _INT_ERROR(channel);

        // start the transation
        return rmtSend(rmt, data, MAX_DATA_PER_ITTERATION);
    } else {
        // use one-go mode if data fits one buffer 
        return rmtSend(rmt, data, size);
    }
}


bool rmtWaitForData(rmt_obj_t* rmt, uint32_t* data, size_t size)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;

    if (g_rmt_objects[channel].buffers < size/MAX_DATA_PER_CHANNEL) {
        return false;
    }

    // wait for the interrupt
    while (!(RMT.int_raw.val&_INT_RX_END(channel))) {}

    // clear the interrupt
    RMT.int_clr.val |= _INT_RX_END(channel);

    size_t i;
    volatile uint32_t* rmt_mem_ptr = &(RMTMEM.chan[channel].data32[0].val);
    for (i=0; i<size; i++) {
        data[i] = *rmt_mem_ptr++;
    }

    return true;
}

bool rmtReceive(rmt_obj_t* rmt, size_t idle_thres)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;

    RMT.int_clr.val |= _INT_ERROR(channel);
    
    RMT.int_ena.val |= _INT_ERROR(channel);

    RMT.conf_ch[channel].conf1.mem_owner = 1;
    RMT.conf_ch[channel].conf0.idle_thres = idle_thres;

    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;

    RMT.conf_ch[channel].conf1.rx_en = 1;

    return true;
}

bool rmtReceiveAsync(rmt_obj_t* rmt, size_t idle_thres, uint32_t* data, size_t size, void* eventFlag)
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
        rmt->remaining_ptr = data;
        rmt->remaining_to_send = size;
    }

    rmt->intr_mode = e_rx_intr;

    RMT.conf_ch[channel].conf1.mem_owner = 1;
    RMT.conf_ch[channel].conf0.idle_thres = idle_thres;

    RMT.int_clr.val |= _INT_RX_END(channel);
    RMT.int_clr.val |= _INT_ERROR(channel);
    
    RMT.int_ena.val |= _INT_RX_END(channel);
    RMT.int_ena.val |= _INT_ERROR(channel);

    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;

    RMT.conf_ch[channel].conf1.rx_en = 1;

    return true;
}


bool rmtSend(rmt_obj_t* rmt, uint32_t* data, size_t size)
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
            *rmt_mem_ptr++ = data[i];
        }
        // tx end mark
        RMTMEM.chan[channel].data32[size].val = 0;
    }

    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;

    RMT.conf_ch[channel].conf1.tx_start = 1;

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

rmt_obj_t* rmtInit(int pin, bool tx_not_rx, int entries, int period)
{
    int buffers = 1 + entries/MAX_DATA_PER_CHANNEL;
    rmt_obj_t* rmt;
    size_t i;
    size_t j;
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
    if (i == MAX_CHANNELS || i+j >= MAX_CHANNELS || j != buffers)  {
        return NULL;
    }
    rmt = _rmtAllocate(pin, i, buffers);

    size_t channel = i;
    rmt->pin = pin;
    rmt->tx_not_rx = tx_not_rx;
    rmt->buffers =buffers;
    rmt->channel = channel;
    _initPin(pin, channel, tx_not_rx);

    // Initialize the registers in default mode:
    // - no carrier, filter
    // - timebase tick of 1us
    RMT.conf_ch[channel].conf0.div_cnt = 1;
    RMT.conf_ch[channel].conf0.mem_size = buffers;
    RMT.conf_ch[channel].conf0.carrier_en = 0;
    RMT.conf_ch[channel].conf0.carrier_out_lv = 0;
    RMT.conf_ch[channel].conf0.mem_pd = 0;

    RMT.conf_ch[channel].conf1.rx_en = 0;
    RMT.conf_ch[channel].conf1.tx_conti_mode = 0;
    RMT.conf_ch[channel].conf1.ref_cnt_rst = 0;
    RMT.conf_ch[channel].conf1.rx_filter_en = 0;
    RMT.conf_ch[channel].conf1.rx_filter_thres = 0;
    RMT.conf_ch[channel].conf1.idle_out_lv = 0;     // signal level for idle
    RMT.conf_ch[channel].conf1.idle_out_en = 1;     // enable idle
    RMT.conf_ch[channel].conf1.ref_always_on = 0;     // base clock

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

    return rmt;
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
    digitalWrite(4, 1);

    int intr_val = RMT.int_st.val;
    size_t ch;
    for (ch=0; ch<MAX_CHANNELS; ch++) {

        if (intr_val&_INT_RX_END(ch)) {
            // clear the flag
            RMT.int_clr.val |= _INT_RX_END(ch);
            //
            if ((g_rmt_objects[ch].intr_mode)&e_rx_intr) {

                if (g_rmt_objects[ch].events) {
                    xEventGroupSetBits(g_rmt_objects[ch].events, RMT_FLAG_RX_DONE);
                }
                if (g_rmt_objects[ch].remaining_ptr && g_rmt_objects[ch].remaining_to_send > 0) {
                    size_t i;
                    for (i = 0; i < g_rmt_objects[ch].remaining_to_send; i++ ) {
                        *(g_rmt_objects[ch].remaining_ptr)++ = RMTMEM.chan[ch].data32[i].val;
                    }
                }
                g_rmt_objects[ch].intr_mode &= ~e_rx_intr;

            } else {
                // Report error and disable Rx interrupt
                ets_printf("Unexpected Rx interrupt!\n");
                RMT.int_ena.val &= ~_INT_RX_END(ch);
            }
        }

        if (intr_val&_INT_ERROR(ch)) {
            // clear the flag
            RMT.int_clr.val |= _INT_ERROR(ch);
            RMT.int_ena.val &= ~_INT_ERROR(ch);
            // report error 
            ets_printf("RMT Error %d!\n", ch);
            if (g_rmt_objects[ch].events) {
                xEventGroupSetBits(g_rmt_objects[ch].events, RMT_FLAG_ERROR);
            }

            // reset memory
            RMT.conf_ch[ch].conf1.mem_rd_rst = 1;
            RMT.conf_ch[ch].conf1.mem_rd_rst = 0;
            RMT.conf_ch[ch].conf1.mem_wr_rst = 1;
            RMT.conf_ch[ch].conf1.mem_wr_rst = 0;
        }

        if (intr_val&_INT_TX_END(ch)) {

            RMT.int_clr.val |= _INT_TX_END(ch);

            if (g_rmt_objects[ch].tx_state&e_last_data) {
                g_rmt_objects[ch].tx_state = e_end_trans;
                ets_printf("Tx_End marked !\n");
                RMT.conf_ch[ch].conf1.tx_conti_mode = 0;
                
                
            } else if (g_rmt_objects[ch].tx_state&e_end_trans) {
                ets_printf("Tx completed !\n");
                // disable interrupts and clear states
                RMT.int_ena.val &= ~_INT_TX_END(ch);
                RMT.int_ena.val &= ~_INT_THR_EVNT(ch);
                g_rmt_objects[ch].intr_mode = e_no_intr;
                g_rmt_objects[ch].tx_state = e_inactive;
            }
        }

        if (intr_val&_INT_THR_EVNT(ch)) {
            // clear the flag
            RMT.int_clr.val |= _INT_THR_EVNT(ch);

            // initial setup of continuous mode
            if (g_rmt_objects[ch].tx_state&e_set_conti) {
                RMT.conf_ch[ch].conf1.tx_conti_mode = 1;
                g_rmt_objects[ch].intr_mode &= ~e_set_conti;
            }

            // check if still any data to be sent
            uint32_t* data = g_rmt_objects[ch].remaining_ptr;
            if (data)
            {
                int remaining_size = g_rmt_objects[ch].remaining_to_send;
                int half_tx_nr = MAX_DATA_PER_ITTERATION/2;
                int i;

                // will the remaining data occupy the entire halfbuffer
                if (remaining_size > half_tx_nr) {
                    if (g_rmt_objects[ch].tx_state&e_first_half) {
                        // ets_printf("first\n");
                        RMTMEM.chan[ch].data32[0].val = data[0] - 1;
                        for (i = 1; i < half_tx_nr; i++) {
                            RMTMEM.chan[ch].data32[i].val = data[i];
                        }
                        g_rmt_objects[ch].tx_state &= ~e_first_half;
                    } else {
                        // ets_printf("second\n");
                        for (i = 0; i < half_tx_nr; i++) {
                            RMTMEM.chan[ch].data32[half_tx_nr+i].val = data[i];
                        }
                        g_rmt_objects[ch].tx_state |= e_first_half;
                    }
                    g_rmt_objects[ch].remaining_to_send -= half_tx_nr;
                    g_rmt_objects[ch].remaining_ptr += half_tx_nr;
                } else {
                    // less remaining data than buffer size -> fill in with fake (inactive) pulses 
                    // ets_printf("last chunk...");
                    if (g_rmt_objects[ch].tx_state&e_first_half) {
                        // ets_printf("first\n");
                        RMTMEM.chan[ch].data32[0].val = data[0] - 1;
                        for (i = 1; i < half_tx_nr; i++) {
                            if (i < remaining_size) {
                                RMTMEM.chan[ch].data32[i].val = data[i];
                            } else {
                                RMTMEM.chan[ch].data32[i].val = 0x000F000F;
                            }
                        }
                        g_rmt_objects[ch].tx_state &= ~e_first_half;
                    } else {
                        // ets_printf("second\n");
                        for (i = 0; i < half_tx_nr; i++) {
                            if (i < remaining_size) {
                                RMTMEM.chan[ch].data32[half_tx_nr+i].val = data[i];
                            } else {
                                RMTMEM.chan[ch].data32[half_tx_nr+i].val = 0x000F000F;
                            }
                        }
                        g_rmt_objects[ch].tx_state |= e_first_half;
                    }
                    RMTMEM.chan[ch].data32[MAX_DATA_PER_ITTERATION].val = 0;
                    // mark 
                    g_rmt_objects[ch].remaining_ptr = NULL;
                }
            } else {
                // no data left, just copy the fake (inactive) pulses 
                if ( (!(g_rmt_objects[ch].tx_state&e_last_data)) && 
                     (!(g_rmt_objects[ch].tx_state&e_end_trans)) ) {
                    // ets_printf("tail (empty)");
                    int half_tx_nr = MAX_DATA_PER_ITTERATION/2;
                    int i;
                    if (g_rmt_objects[ch].tx_state&e_first_half) {
                        // ets_printf("...first\n");
                        for (i = 0; i < half_tx_nr; i++) {
                            RMTMEM.chan[ch].data32[i].val = 0x000F000F;
                        }
                        RMTMEM.chan[ch].data32[i].val = 0;
                        g_rmt_objects[ch].tx_state &= ~e_first_half;
                    } else {
                        // ets_printf("...second\n");
                        for (i = 0; i < half_tx_nr; i++) {
                            RMTMEM.chan[ch].data32[half_tx_nr+i].val = 0x000F000F;
                        }
                        RMTMEM.chan[ch].data32[i].val = 0;
                        g_rmt_objects[ch].tx_state |= e_first_half;
                    }
                    g_rmt_objects[ch].tx_state |= e_last_data;
                } else {
                    // ets_printf("do_nothing\n");
                }
            }
        }
    }
    digitalWrite(4, 0);
    digitalWrite(2, 0);
}
