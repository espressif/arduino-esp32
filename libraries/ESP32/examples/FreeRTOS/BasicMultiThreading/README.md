# Basic Multi Threading Example

This example demonstrates the basic usage of FreeRTOS Tasks for multi threading.

Please refer to other examples in this folder to better utilize their full potential and safeguard potential problems.
It is also advised to read the documentation on FreeRTOS web pages:
[https://www.freertos.org/a00106.html](https://www.freertos.org/a00106.html)

This example will blink the built-in LED and read analog data.
Additionally, this example demonstrates the usage of the task handle, simply by deleting the analog
read task after 10 seconds from the main loop by calling the function `vTaskDelete`.

### Theory:
A task is simply a function that runs when the operating system (FreeeRTOS) sees fit.
This task can have an infinite loop inside if you want to do some work periodically for the entirety of the program run.
This, however, can create a problem - no other task will ever run and also the Watch Dog will trigger and your program will restart.
A nice behaving tasks know when it is useless to keep the processor for itself and give it away for other tasks to be used.
This can be achieved in many ways, but the simplest is called `delay(`milliseconds)`.
During that delay, any other task may run and do its job.
When the delay runs out the Operating System gives the processor the task which can continue.
For other ways to yield the CPU in a task please see other examples in this folder.
It is also worth mentioning that two or more tasks running the same function will run them with separate stacks, so if you want to run the same code (which could be differentiated by the argument) there is no need to have multiple copies of the same function.

**Task creation has a few parameters you should understand:**
```
  xTaskCreate(TaskFunction_t pxTaskCode,
              const char * const pcName,
              const uint16_t usStackDepth,
              void * const pvParameters,
              UBaseType_t uxPriority,
              TaskHandle_t * const pxCreatedTask )
```
  - **pxTaskCode**      is the name of your function which will run as a task
  - **pcName**          is a string of human-readable descriptions for your task
  - **usStackDepth**    is the number of words (word = 4 B) available to the task. If you see an error similar to this "Debug exception reason: Stack canary watchpoint triggered (Task Blink)" you should increase it
  - **pvParameters**    is a parameter that will be passed to the task function - it must be explicitly converted to (void*) and in your function explicitly converted back to the intended data type.
  - **uxPriority**      is a number from 0 to configMAX_PRIORITIES which determines how the FreeRTOS will allow the tasks to run. 0 is the lowest priority.
  - **pxCreatedTask**   task handle is a pointer to the task which allows you to manipulate the task - delete it, suspend and resume.
                    If you don't need to do anything special with your task, simply pass NULL for this parameter.
                    You can read more about task control here: https://www.freertos.org/a00112.html

# Supported Targets

This example supports all SoCs.

### Hardware Connection

If your board does not have a built-in LED, please connect one to the pin specified by the `LED_BUILTIN` in the code (you can also change the number and connect it to the pin you desire).

Optionally you can connect the analog element to the pin. such as a variable resistor, analog input such as an audio signal, or any signal generator. However, if the pin is left unconnected it will receive background noise and you will also see a change in the signal when the pin is touched by a finger.
Please refer to the ESP-IDF ADC documentation for specific SoC for info on which pins are available:
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
* ESP32-S3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
