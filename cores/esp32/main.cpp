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
    	xEventGroupSync(FunctionQueue::loopEventHandle,0x00,FunctionQueue::allSynced,portMAX_DELAY);
    	loop();
        xEventGroupSetBits(FunctionQueue::loopEventHandle, FunctionQueue::loopIndex);// enable synced fq's to run
     }
}

extern "C" void app_main()
{
    initArduino();
    xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
}

#endif
