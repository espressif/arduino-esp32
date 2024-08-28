# Arduino-ESP32 Zigbee Color Dimmer Switch Example

This example shows how to configure Zigbee Coordinator and use it as a Home Automation (HA) color dimmer light switch.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* One development board (ESP32-H2 or ESP32-C6) acting as Zigbee end device (loaded with Zigbee_Color_Dimmable_Light example).
* A USB cable for power supply and programming.
* Choose another board (ESP32-H2 or ESP32-C6) as Zigbee coordinator and upload the Zigbee_Color_Dimmable_Light example.

### Configure the Project

Set the Button Switch GPIO by changing the `GPIO_SWITCH` definition. By default, it's the pin `9` (BOOT button on ESP32-C6 and ESP32-H2).

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the Coordinator Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator)`.
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`.
* Select the COM port: `Tools -> Port: xxx where the `xxx` is the detected COM port.
* Optional: Set debug level to info to see logs from Zigbee stack: `Tools -> Core Debug Level: Info`.

## Troubleshooting

If the End device flashed with the example `Zigbee_Light_Bulb` is not connecting to the coordinator, erase the flash of the End device before flashing the example to the board. It is recommended to do this if you re-flash the coordinator.
You can do the following:

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.
* In the `Zigbee_Light_Bulb` example sketch uncomment function `esp_zb_nvram_erase_at_start(true);` located in `esp_zb_task` function.

By default, the coordinator network is open for 180s after rebooting or flashing new firmware. After that, the network is closed for adding new devices.
You can change it by editing `esp_zb_bdb_open_network(180);` in `esp_zb_app_signal_handler` function.

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
