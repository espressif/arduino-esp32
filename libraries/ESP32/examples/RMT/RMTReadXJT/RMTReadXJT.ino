#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"

#include "esp32-hal.h"

//
// Note: This example uses a FrSKY device communication 
//          using XJT D12 protocol 
//
// ; 0 bit = 6us low/10us high
// ; 1 bit = 14us low/10us high
// ;
// ; --------+       +----------+                 +----------+
// ;         |       |          |                 |          |
// ;         |   0   |          |        1        |          |
// ;         |       |          |                 |          |
// ;         |       |          |                 |          |
// ;         +-------+          +-----------------+          +---------
// ;
// ;         |  6us      10us   |       14us          10us   |
// ;         |-------|----------|-----------------|----------|--------
// ;         |       16us       |            24us            |

//  Typedef of received frame
//  
// ; 0x00 - Sync, 0x7E (sync header ID)
// ; 0x01 - Rx ID, 0x?? (receiver ID number, 0x00-0x??)
// ; 0x02 - Flags 1, 0x?? (used for failsafe and binding)
// ; 0x03 - Flags 2, 0x00 (reserved)
// ; 0x04-0x06, Channels 1/9 and 2/10
// ; 0x07-0x09, Channels 3/11 and 4/12
// ; 0x0A-0x0C, Channels 5/13 and 6/14
// ; 0x0D-0x0F, Channels 7/15 and 8/16
// ; 0x10 - 0x00, always zero
// ; 0x11 - CRC-16 High
// ; 0x12 - CRC-16 Low
// ; 0x13 - Tail, 0x7E (tail ID)
typedef union {
        struct {
                uint8_t head;//0x7E
                uint8_t rxid;//Receiver Number
                uint8_t flags;//Range:0x20, Bind:0x01
                uint8_t reserved0;//0x00
                union {
                        struct {
                                uint8_t ch0_l;
                                uint8_t ch0_h:4;
                                uint8_t ch1_l:4;
                                uint8_t ch1_h;
                        };
                        uint8_t bytes[3];
                } channels[4];
                uint8_t reserved1;//0x00
                uint8_t crc_h;
                uint8_t crc_l;
                uint8_t tail;//0x7E
        };
        uint8_t buffer[20];
} xjt_packet_t;

#define XJT_VALID(i) (i->level0 && !i->level1 && i->duration0 >= 8 && i->duration0 <= 11)

rmt_obj_t* rmt_recv = NULL;

static uint32_t *s_channels;
static uint32_t channels[16];
static uint8_t xjt_flags = 0x0;
static uint8_t xjt_rxid = 0x0;

static bool xjtReceiveBit(size_t index, bool bit){
    static xjt_packet_t xjt;
    static uint8_t xjt_bit_index = 8;
    static uint8_t xht_byte_index = 0;
    static uint8_t xht_ones = 0;

    if(!index){
        xjt_bit_index = 8;
        xht_byte_index = 0;
        xht_ones = 0;
    }

    if(xht_byte_index > 19){
        //fail!
        return false;
    }
    if(bit){
        xht_ones++;
        if(xht_ones > 5 && xht_byte_index && xht_byte_index < 19){
            //fail!
            return false;
        }
        //add bit
        xjt.buffer[xht_byte_index] |= (1 << --xjt_bit_index);
    } else if(xht_ones == 5 && xht_byte_index && xht_byte_index < 19){
        xht_ones = 0;
        //skip bit
        return true;
    } else {
        xht_ones = 0;
        //add bit
        xjt.buffer[xht_byte_index] &= ~(1 << --xjt_bit_index);
    }
    if ((!xjt_bit_index) || (xjt_bit_index==1 && xht_byte_index==19) ) {
        xjt_bit_index = 8;
        if(!xht_byte_index && xjt.buffer[0] != 0x7E){
            //fail!
            return false;
        }
        xht_byte_index++;
        if(xht_byte_index == 20){
            //done
            if(xjt.buffer[19] != 0x7E){
                //fail!
                return false;
            }
            //check crc?

            xjt_flags = xjt.flags;
            xjt_rxid = xjt.rxid;
            for(int i=0; i<4; i++){
                uint16_t ch0 = xjt.channels[i].ch0_l | ((uint16_t)(xjt.channels[i].ch0_h & 0x7) << 8);
                ch0 = ((ch0 * 2) + 2452) / 3;
                uint16_t ch1 = xjt.channels[i].ch1_l | ((uint16_t)(xjt.channels[i].ch1_h & 0x7F) << 4);
                ch1 = ((ch1 * 2) + 2452) / 3;
                uint8_t c0n = i*2;
                if(xjt.channels[i].ch0_h & 0x8){
                    c0n += 8;
                }
                uint8_t c1n = i*2+1;
                if(xjt.channels[i].ch1_h & 0x80){
                    c1n += 8;
                }
                s_channels[c0n] = ch0;
                s_channels[c1n] = ch1;
            }
        }
    }
    return true;
}

void parseRmt(rmt_data_t* items, size_t len, uint32_t* channels){
    bool valid = true;
    rmt_data_t* it = NULL;

    if (!channels)  {
        log_e("Please provide data block for storing channel info");
        return;
    }
    s_channels = channels;

    it = &items[0];
    for(size_t i = 0; i<len; i++){

        if(!valid){
            break;
        }
        it = &items[i];
        if(XJT_VALID(it)){
            if(it->duration1 >= 5 && it->duration1 <= 8){
                valid = xjtReceiveBit(i, false);
            } else if(it->duration1 >= 13 && it->duration1 <= 16){
                valid = xjtReceiveBit(i, true);
            } else {
                valid = false;
            }
        } else if(!it->duration1 && !it->level1 && it->duration0 >= 5 && it->duration0 <= 8) {
                    valid = xjtReceiveBit(i, false);

        }
    }
}

extern "C" void receive_data(uint32_t *data, size_t len, void * arg)
{
    parseRmt((rmt_data_t*) data, len, channels);
}

void setup() 
{
    Serial.begin(115200);
    
    // Initialize the channel to capture up to 192 items
    if ((rmt_recv = rmtInit(21, false, RMT_MEM_192)) == NULL)
    {
        Serial.println("init receiver failed\n");
    }

    // Setup 1us tick
    float realTick = rmtSetTick(rmt_recv, 1000);
    Serial.printf("real tick set to: %fns\n", realTick);

    // Ask to start reading
    rmtRead(rmt_recv, receive_data, NULL);
}

void loop() 
{
    // printout some of the channels
    Serial.printf("%04x %04x %04x %04x\n", channels[0], channels[1], channels[2], channels[3]);
    delay(500);
}
