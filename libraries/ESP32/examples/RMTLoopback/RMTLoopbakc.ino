#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"

#include "esp32-hal-rmt.h"

uint32_t my_data[256];

uint32_t data[256];

rmt_obj_t* rmt_send = NULL;
rmt_obj_t* rmt_recv = NULL;


void setup() 
{
    if ((rmt_send = rmtInit(18, true, 2, 1000)) == NULL)
    {
        printf("init sender failed\n");
    }
    if ((rmt_recv = rmtInit(21, false, 2, 1000)) == NULL)
    {
        printf("init receiver failed\n");
    }


    float realTick = rmtSetTick(rmt_send, 400);
    printf("real tick set to: %f\n", realTick);
    realTick = rmtSetTick(rmt_recv, 400);
    printf("real tick set to: %f\n", realTick);

    events = xEventGroupCreate();
}

void loop() 
{
    // Init data
    int i;
    for (i=0; i<255; i++) {
        data[i] = 0x80010001 + ((i%13)<<16) + 13-(i%13);
    }
    data[255] = 0;

    // Start receiving
    rmtReceiveAsync(rmt_recv, 0x4F, my_data, 60, events);

    // Send in continous mode
    rmtSendQueued(rmt_send, data, 56);

    // Wait for data
    xEventGroupWaitBits(events, RMT_FLAG_RX_DONE, 1, 1, portMAX_DELAY);

    // Printout the received data plus the original values
    for (i=0; i<60; i++)
    {
        printf("%08x=%08x ", my_data[i], data[i] );
        if (!((i+1)%4)) printf("\n");
    }
    printf("\n");

    delay(2000);
}
