#include "esp32-hal.h"
#include "esp8266-compat.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_reg.h"

#include "esp32-hal-rmt.h"
#include "driver/periph_ctrl.h"

#include "soc/rmt_struct.h"
#include "esp_intr_alloc.h"


#define MAX_CHANNELS 8
#define ABS(a) (a>0?a:-a)


struct rmt_obj_s
{
    bool allocated;
    intr_handle_t intr_handle;
    int pin;
    int channel;
    bool tx_not_rx;
    int buffers;
};

static rmt_obj_t g_rmt_objects[MAX_CHANNELS] = {
    { false, NULL, 0, 0, 0, 0},
    { false, NULL, 0, 0, 0, 0},
};

static  intr_handle_t intr_handle;

static bool periph_enabled = false;

static void _initPin(int pin, int channel);


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

bool rmtSend(uint32_t* data, size_t size, rmt_obj_t* rmt)
{
    if (!rmt) {
        return false;
    }
    int channel = rmt->channel;
    RMT.apb_conf.fifo_mask = 1;
    size_t i;
    for (i = 0; i < size; i++) {
        RMTMEM.chan[channel].data32[i].val = data[i];
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

    int apb_div = 0xFF & (int)(tick/12.5);
    int ref_div = 0xFF & (int)(tick/1000);

    float apb_tick = 12.5 * apb_div;
    float ref_tick = 1000.0 * ref_div;
    
     size_t channel = rmt->channel;
    if (ABS(apb_tick - tick) < ABS(ref_tick - tick)) {
        RMT.conf_ch[channel].conf0.div_cnt = apb_div;
        RMT.conf_ch[channel].conf1.ref_always_on = 1;
        return apb_tick;
    } else {
        RMT.conf_ch[channel].conf0.div_cnt = ref_div;
        RMT.conf_ch[channel].conf1.ref_always_on = 0;
        return ref_tick;
    }
}

rmt_obj_t* rmtInit(int pin, bool tx_not_rx, int entries, int period)
{
    int buffers = 1 + entries/64;
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
    if (i == MAX_CHANNELS) {
        return NULL;
    }
    rmt = _rmtAllocate(pin, i, buffers);

    size_t channel = i;
    rmt->pin = pin;
    rmt->tx_not_rx = tx_not_rx;
    rmt->buffers =buffers;
    rmt->channel = channel;
    _initPin(pin, channel);

    // rmt tick from 1/80M -> 12.5ns (1x)
    //                         3.2 us (FFx 00 is more)
    // rmt tick for 1 MHz -> 1us (1x)
    //                       256us (00x)

    // default config (period of 1us)
    RMT.conf_ch[channel].conf0.div_cnt = 1;
    RMT.conf_ch[channel].conf0.mem_size = buffers;
    RMT.conf_ch[channel].conf0.carrier_en = 0;
    RMT.conf_ch[channel].conf0.carrier_out_lv = 0;
    RMT.conf_ch[channel].conf0.mem_pd = 0;

    RMT.conf_ch[channel].conf1.rx_en = 1;
    RMT.conf_ch[channel].conf1.tx_conti_mode = 0;
    RMT.conf_ch[channel].conf1.ref_cnt_rst = 0;
    RMT.conf_ch[channel].conf1.rx_filter_en = 0;
    RMT.conf_ch[channel].conf1.rx_filter_thres = 0;
    RMT.conf_ch[channel].conf1.idle_out_lv = 0;     // signal level for idle
    RMT.conf_ch[channel].conf1.idle_out_en = 1;     // enable idle
    RMT.conf_ch[channel].conf1.ref_always_on = 0;     // base clock

    if (tx_not_rx) {
        RMT.conf_ch[channel].conf1.rx_en = 0;
        RMT.conf_ch[channel].conf1.mem_owner = 0;
    } else {
        RMT.conf_ch[channel].conf1.rx_en = 1;
        RMT.conf_ch[channel].conf1.mem_owner = 1;
    }
    //     RMT.conf_ch[channel].conf0.val = ((uint32_t*)config)[0];
    // RMT.conf_ch[channel].conf1.val = ((uint32_t*)config)[1];

    return rmt;
}



static void _initPin(int pin, int channel) 
//  rmt_driver_config_t* config)
{
    if (!periph_enabled) {
        periph_enabled = true;
        periph_module_enable( PERIPH_RMT_MODULE );
    }
    pinMode(pin, OUTPUT);
    pinMatrixOutAttach(pin, RMT_SIG_OUT0_IDX + channel, 0, 0);
    
    // memcpy( (uint8_t*)&RMT.conf_ch[channel].conf0.val, (uint8_t*)config, 8);
    // memcpy( (uint8_t*)&RMT.conf_ch[channel].conf1.val, ((uint8_t*)config) + 4, sizeof(uint32_t));
}

void initMemory(int offset, uint32_t* data, size_t size)
{
    RMT.apb_conf.fifo_mask = 1;
    size_t i;
    // uint16_t * ptr = (uint16_t*)&(RMTMEM.chan[offset].data16[0]);
    // uint16_t * ptr = (uint16_t*)(0x3ff56800);
    // // uint16_t ptr[32];
    // memset(ptr, 0, 32);
    // for (i = 0; i < size; i++) {
    //     printf(">> %x\n", ptr[i]);
    // }

    for (i = 0; i < size; i++) {
        RMTMEM.chan[offset].data32[i].val = data[i];
        // printf(">> %x\n", data[i]);
        // RMTMEM.chan[offset].data16[i]
        // RMTMEM.chan[offset].data32[i].val = data[i];
        // printf("   %x\n", RMTMEM.chan[offset].data16[i].val);
        // RMTMEM.chan[offset].data16[i] = data[i];
        // *ptr++ = i;
        // ptr[i] = i;
        // RMTMEM.chan[i].data1 = 0x11+ i;
        // RMTMEM.chan[i].data2 = 0x22+ i;
        
        // printf("   %x\n", RMTMEM.chan[offset].data16[i].val);
        // printf("   %x\n", RMTMEM.chan[offset].data32[i/2].val);
        // printf("--------\n");

        // printf("   %x\n", RMTMEM.chan[offset].xx.data16[i]);
        // RMTMEM.chan[offset].xx.data16[i] = data[i];
        // printf("   %x\n", RMTMEM.chan[offset].xx.data16[i]);
        // printf("   %x\n", RMTMEM.chan[offset].xx.data32[i/2]);
        // printf("--------\n");

    }

}

// 00000000
// 00010000



void startTx(int channel)
{
    // reset rd value
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;

    RMT.conf_ch[channel].conf1.tx_start = 1;
}

// struct rmt_struct_t {
//     uart_dev_t * dev;
// #if !CONFIG_DISABLE_HAL_LOCKS
//     xSemaphoreHandle lock;
// #endif
//     uint8_t num;
//     xQueueHandle queue;
//     intr_handle_t intr_handle;
// };

static int state = 0;

static void IRAM_ATTR _rmt_isr(void* arg)
{
RMT.int_clr.val = 1;
// printf("test");
state ^= 1;
digitalWrite(4, state);
}

int config();

int setpin(int pin)
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