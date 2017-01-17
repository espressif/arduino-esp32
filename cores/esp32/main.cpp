#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"

#if CONFIG_AUTOSTART_ARDUINO

void loopTask(void *pvParameters)
{
    setup();
    for(;;) {
        loop();
    }
}

extern "C" void app_main()
{
    initArduino();
    xTaskCreatePinnedToCore(loopTask, "loopTask", 4096, NULL, 1, NULL, 1);
}

#endif

/*
**  Block below to be removed with next toolchain
*/
void *operator new(size_t size)
{
    return malloc(size);
}

void *operator new[](size_t size)
{
    return malloc(size);
}

void operator delete(void * ptr)
{
    free(ptr);
}

void operator delete[](void * ptr)
{
    free(ptr);
}

extern "C" void __cxa_pure_virtual(void) __attribute__ ((__noreturn__));
extern "C" void __cxa_deleted_virtual(void) __attribute__ ((__noreturn__));

void __cxa_pure_virtual(void)
{
    abort();
}

void __cxa_deleted_virtual(void)
{
    abort();
}

