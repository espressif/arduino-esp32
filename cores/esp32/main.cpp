#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"
#include "FunctionQueue.h"

#if CONFIG_AUTOSTART_ARDUINO

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif


void loopTask(void *pvParameters)
{
    setup();
    for(;;) {
        loop();
        if (FunctionQueue::allSynced != 0)
        {
        	uint16_t er = xEventGroupSync(FunctionQueue::loopEventHandle,0x01,FunctionQueue::allSynced,portMAX_DELAY);
        }
     }
}

extern "C" void app_main()
{
    initArduino();
    xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
}

#endif
