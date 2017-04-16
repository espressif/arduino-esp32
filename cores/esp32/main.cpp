#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"

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
    }
}

void vMicrosOverflowTask(void *pvParameters)
{
	// In order no to miss overflow it will be enought to check micros() each half of full overflow
	static const uint32_t taskDelayMs = UINT32_MAX
		/ CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ // one full overflow occurs in uS
		/ 1000 // in millisec
		/ 2 // half of full overflow cycle
		/ portTICK_PERIOD_MS;  // RTOS const to convert in ms

	for (;;){
		micros(); //update overflow
		vTaskDelay(taskDelayMs); // ~ each 9 sec at 240Mhz
	}
}

extern "C" void app_main()
{
    initArduino();
    xTaskCreatePinnedToCore(vMicrosOverflowTask, "microsTask", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL, ARDUINO_RUNNING_CORE);
	xTaskCreatePinnedToCore(loopTask, "loopTask", 4096, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
}

#endif
