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

struct rmt_obj_s
{
    bool allocated;
    intr_handle_t intr_handle;
    int pin;
    int channel;
    bool tx_not_rx;
    int buffers;
    int remaining_to_send;
    uint32_t* remaining_ptr;
    intr_mode_t intr_mode;

};

static rmt_obj_t g_rmt_objects[MAX_CHANNELS] = {
    { false, NULL, 0, 0, 0, 0, 0, NULL, e_no_intr},
    { false, NULL, 0, 0, 0, 0, 0, NULL, e_no_intr},
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
    return true;
    g_rmt_objects[from].channel = 0;
    g_rmt_objects[from].buffers = 0;
}

bool rmtSendQueued(rmt_obj_t* rmt, uint32_t* data, size_t size)
{
    if (!rmt) {
        return false;
    }
    
    int channel = rmt->channel;
    if (size > MAX_DATA_PER_CHANNEL) {
        /*  setup interrupts for half and full tx
        */
        int half_tx_nr = MAX_DATA_PER_CHANNEL/2;
        RMT.tx_lim_ch[channel].limit = half_tx_nr;
        rmt->remaining_to_send = size - MAX_DATA_PER_CHANNEL;
        rmt->remaining_ptr = data + MAX_DATA_PER_CHANNEL;
        rmt->intr_mode = e_tx_intr | e_txthr_intr;
        RMT.conf_ch[channel].conf1.tx_conti_mode = 1; 
       
        if (!intr_handle) {
            esp_intr_alloc(ETS_RMT_INTR_SOURCE, (int)ESP_INTR_FLAG_IRAM, _rmt_isr, NULL, &intr_handle);
        }
        // clear and enable both Tx completed and half tx event
        RMT.int_clr.val |= _INT_TX_END(channel);
        RMT.int_clr.val |= _INT_THR_EVNT(channel);
        
        RMT.int_ena.val |= _INT_TX_END(channel);
        RMT.int_ena.val |= _INT_THR_EVNT(channel);
        return rmtSend(rmt, data, MAX_DATA_PER_CHANNEL);

    }


    if (size < MAX_DATA_PER_CHANNEL) {
        return rmtSend(rmt, data, size);
    }

    return true;
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

    RMT.conf_ch[channel].conf0.idle_thres = idle_thres;

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
        for (i = 0; i < size; i++) {
            // TODO: use ptr for writing more than 1 channel mem
            RMTMEM.chan[channel].data32[i].val = data[i];
        }
    }

    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;

    RMT.conf_ch[channel].conf1.tx_start = 1;

    return true;
}

rmt_obj_t* _rmtAllocate(int pin, int from, int size)
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
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
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
    } else {
        // RMT.conf_ch[channel].conf1.rx_en = 1;
        RMT.conf_ch[channel].conf1.mem_owner = 1;
    }
    //     RMT.conf_ch[channel].conf0.val = ((uint32_t*)config)[0];
    // RMT.conf_ch[channel].conf1.val = ((uint32_t*)config)[1];

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

static int state = 0;

static void IRAM_ATTR _rmt_isr(void* arg)
{
    digitalWrite(4, 1);

    int intr_val = RMT.int_st.val;
    size_t ch;
    for (ch=0; ch<MAX_CHANNELS; ch++) {

        if (intr_val&_INT_RX_END(ch)) {
            if ((g_rmt_objects[ch].intr_mode)&e_rx_intr) {
                // Not yet implemented
            } else {
                // REPORT error
            }
        }
        if (intr_val&_INT_TX_END(ch)) {

            RMT.int_clr.val |= _INT_TX_END(ch);
            
            if ((g_rmt_objects[ch].intr_mode)&e_tx_intr) {
                // have to copy the remaining bytes
                size_t i;
                int half_tx_nr = MAX_DATA_PER_CHANNEL/2;
                uint32_t* data = g_rmt_objects[ch].remaining_ptr;

                // prepare the data to send
                for (i = 0; i < half_tx_nr; i++) {
                    RMTMEM.chan[ch].data32[i+half_tx_nr].val = data[i];
                }

                // check for the transaction end
                if (g_rmt_objects[ch].remaining_to_send > half_tx_nr) {
                    g_rmt_objects[ch].remaining_to_send -= half_tx_nr;
                    g_rmt_objects[ch].remaining_ptr += half_tx_nr;
                } else {
                    // mark transaction completed
                    g_rmt_objects[ch].intr_mode = e_no_intr;
                    RMT.int_ena.val &= ~_INT_TX_END(ch);
                    RMT.int_ena.val &= ~_INT_THR_EVNT(ch);
                    RMT.conf_ch[ch].conf1.tx_conti_mode = 0; 

                    // TODO ... a callback or event?
                }
            } else {
                // REPORT error
            }
        }
        if (intr_val&_INT_THR_EVNT(ch)) {

            RMT.int_clr.val |= _INT_THR_EVNT(ch);

            if ((g_rmt_objects[ch].intr_mode)&e_txthr_intr) {
                // have to copy the remaining bytes
                size_t i;
                int half_tx_nr = MAX_DATA_PER_CHANNEL/2;
                uint32_t* data = g_rmt_objects[ch].remaining_ptr;

                // prepare the data to send
                for (i = 0; i < half_tx_nr; i++) {
                    RMTMEM.chan[ch].data32[i].val = data[i];
                }

                // check for the transaction end
                if (g_rmt_objects[ch].remaining_to_send > half_tx_nr) {
                    g_rmt_objects[ch].remaining_to_send -= half_tx_nr;
                    g_rmt_objects[ch].remaining_ptr += half_tx_nr;
                } else {
                    // mark transaction completed
                    g_rmt_objects[ch].intr_mode = e_no_intr;
                    RMT.int_ena.val &= ~_INT_TX_END(ch);
                    RMT.int_ena.val &= ~_INT_THR_EVNT(ch);
                    RMT.conf_ch[ch].conf1.tx_conti_mode = 0; 

                    // TODO ... a callback or event?
                }
            } else {
                // REPORT error
            }
        }

    }
    // switch (RMT.int_st.val)
    // RMT.int_clr.val = 1;
    // state ^= 1;
    digitalWrite(4, 0);
}

int config();

int setxpin(int pin)
{
    int channel = 0;
    pinMode(pin, OUTPUT);
// *((uint32_t*)0x3FF49070) = 0x002A0000;
    pinMatrixOutAttach(pin, RMT_SIG_OUT0_IDX + channel, 0, 0);
    periph_module_enable( PERIPH_RMT_MODULE );

// 0x3ff49000 : 0x3FF49000 <Hex>
//   Address   0 - 3     4 - 7     8 - B     C - F               
//   3FF49000  FF030000  00080000  00080000  00080000          
//   3FF49010  00080000  00080000  00080000  00080000          
//   3FF49020  00080000  00080000  00080000  000A0000          
//   3FF49030  000A0000  800A0000  000A0000  000B0000          
//   3FF49040  800A0000  000B0000  800A0000  000A0000          
//   3FF49050  000A0000  002B0000  002B0000  002B0000          
//   3FF49060  001B0000  002B0000  002B0000  000B0000          
//   3FF49070  002A0000  000A0000  000A0000  000A0000          
//   3FF49080  000A0000  000B0000  000A0000  000A0000          
//   3FF49090  00080000  00080000  00080000  00080000          
//   3FF490A0  00080000  00080000  00080000  00080000          

*((unsigned int*)(0x3FF560F0)) = 1;
    // SET(RMT_DATA_REG, 0x8010020);
    *((uint32_t*)0x3ff56800) = 0x8FFF0FFF;
    *((uint32_t*)0x3ff56804) = 0x9FFF0FFF;
    *((uint32_t*)0x3ff56808) = 0;
    *((uint32_t*)0x3ff5680C) = 0;
    *((uint32_t*)0x3ff56810) = 0;
    *((uint32_t*)0x3FF560D0) = 0;
    // uint32_t test = 0x8010;
    // uint32_t zero = 0;
    // memcpy((uint8_t*)0x3ff56800, &test, 4);
    // memcpy((uint8_t*)0x3ff56804, &zero, 4);

    RMT.conf_ch[0].conf0.val = 0x01000001;
    RMT.conf_ch[0].conf1.val = 0x20000;

    esp_intr_alloc(ETS_RMT_INTR_SOURCE, (int)ESP_INTR_FLAG_IRAM, _rmt_isr, NULL, &intr_handle);
    RMT.int_ena.val = 1;

//     RMT[0].conf0 = 0x01000001;
// // SET(RMT_CHnCONF0_REG(0), 0x31000001);
// // SET(RMT_CHnCONF1_REG(0), 0x20000);
//     RMT[1].conf1 = 0x20000;


// 0x311000ff
//--   0x01000ff

//    0xa0f00
    return 1;
}

int run()
{
// SET(RMT_CHnCONF0_REG(0), 0x1000001);

// *((unsigned int*)(0x3FF560F0)) = 1;
//     // SET(RMT_DATA_REG, 0x8010020);
//     *((uint32_t*)0x3ff56800) = 0x8010;
//     *((uint32_t*)0x3ff56804) = 0;

// SET(RMT_CHnCONF1_REG(0), 0x20009);

return 0;

}