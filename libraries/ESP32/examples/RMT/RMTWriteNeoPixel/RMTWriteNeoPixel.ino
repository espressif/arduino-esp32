#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"

#include "esp32-hal.h"

#define LED_MATRIX_SIZE 8*4*24

//
// Note: This example uses Neopixel LED board, 32 LEDs chained one
//      after another, each RGB LED has its 24 bit value 
//      for color configuration (8b for each color)
//
//      Bits encoded as pulses as follows:
//
//      "0":
//      ---+       +--------------+
//         |       |              |
//         |       |              |
//         |       |              |
//         |       |              |
//         +-------+              +--
//         | 0.4us |   0.85 0us   |
//
//      "1":
//      ---+             +-------+
//         |             |       |
//         |             |       |
//         |             |       |
//         |             |       |
//         +-------------+       +--
//         |    0.8us    | 0.4us |

rmt_data_t led_data[LED_MATRIX_SIZE];

rmt_obj_t* rmt_send = NULL;

static EventGroupHandle_t events;

void setup() 
{
    Serial.begin(115200);
    
    if ((rmt_send = rmtInit(18, true, RMT_MEM_64)) == NULL)
    {
        Serial.println("init sender failed\n");
    }

    float realTick = rmtSetTick(rmt_send, 100);
    Serial.printf("real tick set to: %fns\n", realTick);

}

int color[] =  { 0x55, 0x11, 0x77 };  // RGB value
int led_index = 0;

void loop() 
{
    // Init data with only one led ON
    int i;
    int item, bit, inx;
    for (i=0; i<LED_MATRIX_SIZE; i++) {

        item = i/24;

        bit = i%8;
        inx = i/8;

        if ( (color[inx%3] & (1<<(8-bit))) && (item == led_index) )
            led_data[i].val = 0x80070006;
        else
            led_data[i].val = 0x80040008;
    }
    // make the led travel in the pannel
    if ((++led_index)>=4*8) {
        led_index = 0;
    }

    // Send the data
    rmtWrite(rmt_send, led_data, LED_MATRIX_SIZE);

    delay(100);
}
