#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "Arduino.h"
#if (ARDUINO_USB_CDC_ON_BOOT|ARDUINO_USB_MSC_ON_BOOT|ARDUINO_USB_DFU_ON_BOOT) && !ARDUINO_USB_MODE
#include "USB.h"
#if ARDUINO_USB_MSC_ON_BOOT
#include "FirmwareMSC.h"
#endif
#endif

#include "chip-debug-report.h"

#ifndef ARDUINO_LOOP_STACK_SIZE
#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define ARDUINO_LOOP_STACK_SIZE 8192
#else
#define ARDUINO_LOOP_STACK_SIZE CONFIG_ARDUINO_LOOP_STACK_SIZE
#endif
#endif

TaskHandle_t loopTaskHandle = NULL;

#if CONFIG_AUTOSTART_ARDUINO
#if CONFIG_FREERTOS_UNICORE
void yieldIfNecessary(void){
    static uint64_t lastYield = 0;
    uint64_t now = millis();
    if((now - lastYield) > 2000) {
        lastYield = now;
        vTaskDelay(5); //delay 1 RTOS tick
    }
}
#endif

bool loopTaskWDTEnabled;

__attribute__((weak)) size_t getArduinoLoopTaskStackSize(void) {
    return ARDUINO_LOOP_STACK_SIZE;
}

__attribute__((weak)) bool shouldPrintChipDebugReport(void) {
    return false;
}

void loopTask(void *pvParameters)
{
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
    // sets UART0 (default console) RX/TX pins as already configured in boot or as defined in variants/pins_arduino.h
    Serial0.setPins(gpioNumberToDigitalPin(SOC_RX0), gpioNumberToDigitalPin(SOC_TX0));
#endif
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    printBeforeSetupInfo();
#else
    if(shouldPrintChipDebugReport()){
        printBeforeSetupInfo();
    }
#endif
    setup();
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    printAfterSetupInfo();
#else
    if(shouldPrintChipDebugReport()){
        printAfterSetupInfo();
    }
#endif
    for(;;) {
#if CONFIG_FREERTOS_UNICORE
        yieldIfNecessary();
#endif
        if(loopTaskWDTEnabled){
            esp_task_wdt_reset();
        }
        loop();
        if (serialEventRun) serialEventRun();
    }
}

extern "C" void app_main()
{
#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
    Serial.begin();
#endif
#if ARDUINO_USB_MSC_ON_BOOT && !ARDUINO_USB_MODE
    MSC_Update.begin();
#endif
#if ARDUINO_USB_DFU_ON_BOOT && !ARDUINO_USB_MODE
    USB.enableDFU();
#endif
#if ARDUINO_USB_ON_BOOT && !ARDUINO_USB_MODE
    USB.begin();
#endif
    loopTaskWDTEnabled = false;
    initArduino();
    xTaskCreateUniversal(loopTask, "loopTask", getArduinoLoopTaskStackSize(), NULL, 1, &loopTaskHandle, ARDUINO_RUNNING_CORE);
}

#endif
