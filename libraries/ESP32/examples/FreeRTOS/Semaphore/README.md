# Semaphore Example

This example demonstrates the basic usage of FreeRTOS Semaphores and queue sets for coordination between tasks for multi-threading.
Please refer to other examples in this folder to better understand the usage of tasks.
It is also advised to read the documentation on FreeRTOS web pages:
[https://www.freertos.org/a00106.html](https://www.freertos.org/a00106.html)

### Theory:
Semaphore is in essence a variable. Tasks can set the value, wait until one or more
semaphores are set and thus communicate between each other their state.
A binary semaphore is a semaphore that has a maximum count of 1, hence the 'binary' name.
A task can only 'take' the semaphore if it is available, and the semaphore is only available if its count is 1.

Semaphores can be controlled by any number of tasks. If you use semaphore as a one-way
signalization with only one task giving and only one task taking there is a much faster option
called Task Notifications - please see FreeRTOS documentation and read more about them: [https://www.freertos.org/RTOS-task-notifications.html](https://www.freertos.org/RTOS-task-notifications.html)

This example uses a semaphore to signal when a package is delivered to a warehouse by multiple
delivery trucks, and multiple workers are waiting to receive the package.

# Supported Targets

This example supports all ESP32 SoCs.

## How to Use Example

Read the code and try to understand it, then flash and observe the Serial output.

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

## Example Log Output

```
Anything you write will return as echo.
Maximum line length is 63 characters (+ terminating '0').
Anything longer will be sent as a separate line.

```
< Input text "Short input"

``Echo line of size 11: "Short input"``

< Input text "An example of very long input which is longer than default 63 characters will be split."

```
Echo line of size 63: "An example of very long input which is longer than default 63 c"
Echo line of size 24: "haracters will be split."
```

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
