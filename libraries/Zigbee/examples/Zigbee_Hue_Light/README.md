# Arduino-ESP32 Zigbee Hue Light Example

This example is an on/off light router that can join a Philips Hue Bridge.

The bridge does not accept the stock on/off light. After `Zigbee.role()` the sketch turns on distributed security, sets a 16-byte Trust Center link key, and sets the endpoint device version to 1. Hue caches version 0 and then refuses the device.

The array `hue_trust_center_key` in the sketch is a placeholder. Replace it with a 16-byte Hue Trust Center link key before pairing. This library does not include that key.

Hue does not use the public `ZigbeeAlliance09` key. The Hue app does not show a key either. The same key works for every Hue Bridge, and public copies that work with Hue are available online. Copy those 16 bytes into `hue_trust_center_key`.

The On/Off cluster includes `OnTime`, `OffWaitTime`, and `GlobalSceneControl`, so a Hue Off with effect turns this light off. There is no Level Control cluster, so the LED does not fade.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C5 | ESP32-C6 | ESP32-H2 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

## Hardware Required

* A USB cable for power supply and programming
* One development board (ESP32-C5, ESP32-C6, ESP32-H2 or ESP32-S31) as a Zigbee router
* A Philips Hue Bridge with the Hue app open for pairing

### Configure the Project

Set the LED GPIO by changing the `led` definition. By default it is `RGB_BUILTIN`. Boards without that pin use GPIO 2.

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the router Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee ZCZR 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.

## Troubleshooting

Erase the flash before the first pairing, and again if the board was previously joined to another network. Hue remembers a device that joined with version 0.

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.
* Hold the BOOT button for more than 3 seconds to factory-reset Zigbee and reboot.

Open the Hue app and search for lights while the sketch is connecting. The placeholder key will not join.


***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

* **LED not changing:** Check the wiring connection and the IO selection.
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
* ESP32-C5 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c5_datasheet_en.pdf)
* ESP32-C6 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
* ESP32-H2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf)
* ESP32-S31 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s31_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
