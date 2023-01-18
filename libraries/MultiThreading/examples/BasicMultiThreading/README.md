# Basic Multi Threading

This example demonstrates basic usage of FreeRTOS Tasks for multi threading.

Please refer to other examples in this folder to better utilize their the full potential and safe-guard potential problems.
It is also advised to read documentation on FreeRTOS web pages:
[https://www.freertos.org/a00106.html](https://www.freertos.org/a00106.html)

This example will blink builtin LED and read analog data.
  Additionally this example demonstrates usage of task handle, simply by deleting the analog
  read task after 10 seconds from main loop by calling the function `vTaskDelete`.

### Theory:
A task is simply a function which runs when the operating system (FreeeRTOS) sees fit.
This task can have an infinite loop inside if you want to do some work periodically for the entirety of the program run.
This however can create a problem - no other task will ever run and also the Watch Dog will trigger and your program will restart.
A nice behaving tasks knows when it is useless to keep the processor for itself and gives it away for other tasks to be used.
This can be achieved by many ways, but the simplest is calling `delay(milliseconds)`.
During that delay any other task may run and do it's job.
When the delay runs out the Operating System gives the processor to the task which can continue.
For other ways to yield the CPU in task please see other examples in this folder.
It is also worth mentioning that two or more tasks running the exact same function will run them with separated stack, so if you want to run the same code (could be differentiated by the argument) there is no need to have multiple copies of the same function.

**Task creation has few parameters you should understand:**
```
  xTaskCreate(TaskFunction_t pxTaskCode,
              const char * const pcName,
              const uint16_t usStackDepth,
              void * const pvParameters,
              UBaseType_t uxPriority,
              TaskHandle_t * const pxCreatedTask )
```
  - **pxTaskCode**      is the name of your function which will run as a task
  - **pcName**          is a string of human readable description for your task
  - **usStackDepth**    is number of words (word = 4B) available to the task. If you see error similar to this "Debug exception reason: Stack canary watchpoint triggered (Task Blink)" you should increase it
  - **pvParameters**    is a parameter which will be passed to the task function - it must be explicitly converted to (void*) and in your function explicitly converted back to the intended data type.
  - **uxPriority**      is number from 0 to configMAX_PRIORITIES which determines how the FreeRTOS will allow the tasks to run. 0 is lowest priority.
  - **pxCreatedTask**   task handle is basically a pointer to the task which allows you to manipulate with the task - delete it, suspend and resume.
                    If you don't need to do anything special with you task, simply pass NULL for this parameter.
                    You can read more about task control here: https://www.freertos.org/a00112.html

# Supported Targets

This example supports all SoCs.

### Hardware Connection

If your board does not have built in LED, please connect one to the pin specified by the `LED_BUILTIN` in code (you can also change the number and connect it on pin you desire).

Optionally you can connect analog element to pin ? such as variable resistor, or analog input such as audio signal, or any signal generator. However if the pin is left unconnected it will receive background noise and you will also see change in the signal when the pin is touched by finger.
Please refer to the ESP-IDF ADC documentation for specific SoC for info which pins are available:
[ESP32](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/peripherals/adc.html),
 [ESP32-S2](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s2/api-reference/peripherals/adc.html),
 [ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/peripherals/adc.html),
 [ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32c3/api-reference/peripherals/adc.html)


#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or set the `upload_port` option on the `platformio.ini` file.

## Troubleshooting

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
