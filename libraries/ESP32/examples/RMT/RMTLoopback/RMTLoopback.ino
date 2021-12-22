#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"

#include "esp32-hal.h"

#if CONFIG_IDF_TARGET_ESP32C3
// ESP32 C3 has only 2 channels for RX and 2 for TX, thus MAX RMT_MEM is 128
#define RMT_TX_PIN 4
#define RMT_RX_PIN 5
#define RMT_MEM_RX RMT_MEM_128
#else 
#define RMT_TX_PIN 18
#define RMT_RX_PIN 21
#define RMT_MEM_RX RMT_MEM_192
#endif

rmt_data_t my_data[256];
rmt_data_t data[256];

rmt_obj_t* rmt_send = NULL;
rmt_obj_t* rmt_recv = NULL;

static EventGroupHandle_t events;

void setup() 
{
    Serial.begin(115200);
    events = xEventGroupCreate();
    
    if ((rmt_send = rmtInit(RMT_TX_PIN, RMT_TX_MODE, RMT_MEM_64)) == NULL)
    {
        Serial.println("init sender failed\n");
    }
    if ((rmt_recv = rmtInit(RMT_RX_PIN, RMT_RX_MODE, RMT_MEM_RX)) == NULL)
    {
        Serial.println("init receiver failed\n");
    }

    float realTick = rmtSetTick(rmt_send, 100);
    printf("real tick set to: %fns\n", realTick);
    // both will keep same tick 
    realTick = rmtSetTick(rmt_recv, 100);
}

void loop() 
{
    // Init data
    int i;
    for (i=0; i<255; i++) {
        data[i].val = 0x80010001 + ((i%13)<<16) + 13-(i%13);
    }
    data[255].val = 0;

    // Start receiving
    rmtReadAsync(rmt_recv, my_data, 100, events, false, 0);

    // Send in continous mode
    rmtWrite(rmt_send, data, 100);

    // Wait for data
    xEventGroupWaitBits(events, RMT_FLAG_RX_DONE, 1, 1, portMAX_DELAY);

    // Printout the received data plus the original values
    for (i=0; i<60; i++)
    {
        Serial.printf("%08x=%08x ", my_data[i].val, data[i].val );
        if (!((i+1)%4)) Serial.println("\n");
    }
    Serial.println("\n");

    delay(2000);
}
