#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void initVariant() __attribute__((weak));
void initVariant() {}

void init() __attribute__((weak));
void init() {}

void startWiFi() __attribute__((weak));
void startWiFi() {}

void initWiFi() __attribute__((weak));
void initWiFi() {}

extern void loop();
extern void setup();

void loopTask(void *pvParameters)
{
    bool setup_done = false;
    for(;;) {
        if(!setup_done) {
            startWiFi();
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
    initWiFi();
    xTaskCreatePinnedToCore(loopTask, "loopTask", 4096, NULL, 1, NULL, 1);
}

