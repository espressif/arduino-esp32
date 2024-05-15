# Arduino-ESP32 Zigbee Light Bulb Example

This example shows how to configure the Zigbee end device and use it as a Home Automation (HA) on/off light bulb.

**This example is based on ESP-Zigbee-SDK example esp_zigbee_HA_sample/HA_on_off_light.**

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* One development board (ESP32-H2 or ESP32-C6) acting  as Zigbee coordinator (loaded with Zigbee_Light_switch example)
* A USB cable for power supply and programming
* Choose another board (ESP32-H2 or ESP32-C6) as Zigbee end device (loaded with Zigbee_Light_bulb example)

### Configure the Project

Set the LED GPIO by changing the `LED_PIN` definition. By default, the LED_PIN is `RGB_BUILTIN`.
By default, the `neoPixelWrite` function is used to control the LED. You can change it to digitalWrite to control a simple LED.

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

## Troubleshooting

If the End device flashed with this example is not connecting to the coordinator, erase the flash before flashing it to the board. It is recommended to do this if you did changes to the coordinator.
You can do the following:

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`
* In the sketch uncomment function `esp_zb_nvram_erase_at_start(true);` located in `esp_zb_task` function.

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

The ESP Zigbee SDK provides more examples:
* ESP Zigbee SDK Docs: [Link](https://docs.espressif.com/projects/esp-zigbee-sdk)
* ESP Zigbee SDK Repo: [Link](https://github.com/espressif/esp-zigbee-sdk)

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* ESP32-S3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
