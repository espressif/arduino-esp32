# Arduino-ESP32 Zigbee Temperature Dimmable Light Example

This example shows how to configure a Zigbee end device and use it as a Home Automation (HA) temperature dimmable light (white tunable).

## Supported Targets

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* A USB cable for power supply and programming

### Configure the Project

Set the LED GPIO by changing the `led` definition. By default, the LED is `RGB_BUILTIN`.

#### Using Arduino IDE

* Select the correct board: `Tools -> Board`.
* Select Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Partition Scheme: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where `xxx` is the detected COM port.
* Optional: Set debug level to verbose: `Tools -> Core Debug Level: Verbose`.

## Troubleshooting

If the End device flashed with this example is not connecting to the coordinator, erase the flash of the End device before flashing the example to the board. It is recommended to do this if you re-flash the coordinator.

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.
* Add to the sketch `Zigbee.factoryReset();` to reset the device and Zigbee stack.

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

* **LED not blinking:** Check the wiring connection and the IO selection.
* **Programming Fail:** If the programming/flash procedure fails, try reducing the serial connection speed.
* **COM port not detected:** Check the USB cable and the USB to Serial driver installation.

If the error persists, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32-C6 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
* ESP32-H2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
