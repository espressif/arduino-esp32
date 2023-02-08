# Queue Example

This example demonstrates the basic usage of FreeRTOS Queues which enables tasks to pass data between each other in a secure asynchronous way.
Please refer to other examples in this folder to better understand the usage of tasks.
It is also advised to read the documentation on FreeRTOS web pages:
[https://www.freertos.org/a00106.html](https://www.freertos.org/a00106.html)

This example reads data received on the serial port (sent by the user) pass it via queue to another task which will send it back on Serial Output.

### Theory:
A queue is a simple-to-use data structure (in the most basic way) controlled by `xQueueSend` and `xQueueReceive` functions.
Usually, one task writes into the queue and the other task reads from it.
Usage of queues enables the reading task to yield the CPU until there are data in the queue and therefore not waste precious computation time.

# Supported Targets

This example supports all ESP32 SoCs.

## How to Use Example

Flash and write anything to serial input.

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or set the `upload_port` option on the `platformio.ini` file.

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
