#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"

#include "esp32-hal.h"

#if CONFIG_IDF_TARGET_ESP32C3
// ESP32 C3 has only 2 channels for RX and 2 for TX, thus MAX RMT_MEM is 128
#define RMT_TX_PIN 4
#define RMT_RX_PIN 5
#define RMT_MEM_RX RMT_MEM_NUM_BLOCKS_2
#else 
#define RMT_TX_PIN 18
#define RMT_RX_PIN 21
#define RMT_MEM_RX RMT_MEM_NUM_BLOCKS_3
#endif

rmt_data_t my_data[256];
rmt_data_t data[256];

static EventGroupHandle_t events;

#define RMT_FREQ 10000000
#define RMT_NUM_EXCHANGED_DATA 30

void setup() 
{
    Serial.begin(115200);
    events = xEventGroupCreate();
    
    if (!rmtInit(RMT_TX_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, RMT_FREQ))
    {
        Serial.println("init sender failed\n");
    }
    if (!rmtInit(RMT_RX_PIN, RMT_RX_MODE, RMT_MEM_RX, RMT_FREQ))
    {
        Serial.println("init receiver failed\n");
    }

    // End of transmission shall be detected when line is idle for 2us
    rmtSetRxThreshold(RMT_RX_PIN, 2000);
    // Disable Glitch  filter
    rmtSetFilter(RMT_RX_PIN, false, 0);  

    Serial.println("real tick set to: 100ns");
    Serial.printf("\nPlease connect GPIO %d to GPIO %d, now.\n", RMT_TX_PIN, RMT_RX_PIN);
}

void loop() 
{
    // Init data
    int i;
    for (i=0; i<255; i++) {
        data[i].val = 0x80010001 + ((i%13)<<16) + 13-(i%13);
        my_data[i].val = 0;
    }
    data[255].val = 0;

    // Start receiving
    size_t rx_num_symbols = RMT_NUM_EXCHANGED_DATA;
    rmtReadAsync(RMT_RX_PIN, my_data, &rx_num_symbols, events, false, 0);

    // Send in continous mode by calling it in a loop
    rmtWrite(RMT_TX_PIN, data, RMT_NUM_EXCHANGED_DATA, false, false);

    // Wait for data
    xEventGroupWaitBits(events, RMT_FLAG_RX_DONE, 1, 1, portMAX_DELAY);
    Serial.printf("Got %d RMT symbols\n", rx_num_symbols);
    
    // Printout the received data plus the original values
    for (i=0; i<60; i++)
    {
        Serial.printf("%08lx=%08lx ", my_data[i].val, data[i].val );
        if (!((i+1)%4)) Serial.println("");
    }
    Serial.println("\n");

    delay(2000);
}
