#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp32-hal.h"

#if CONFIG_AUTOSTART_ARDUINO

extern "C" void initArduino();
extern void loop();
extern void setup();

void startWiFi() __attribute__((weak));
void startWiFi() {}

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
    initArduino();
    xTaskCreatePinnedToCore(loopTask, "loopTask", 4096, NULL, 1, NULL, 1);
}

#endif
