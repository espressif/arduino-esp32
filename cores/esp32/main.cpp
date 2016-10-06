#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void initVariant() __attribute__((weak));
void initVariant() {}

void init() __attribute__((weak));
void init() {}

void bootWiFi() __attribute__((weak));
void bootWiFi() {}

extern void loop();
extern void setup();

void loopTask(void *pvParameters)
{
    bool setup_done = false;
    for(;;) {
        if(!setup_done) {
            bootWiFi();
            setup();
            setup_done = true;
        }
        loop();
    }
}

extern "C" void app_main()
{
    init();
    initVariant();
    xTaskCreatePinnedToCore(loopTask, "loopTask", 4096, NULL, 1, NULL, 0);
}

