# Wi-Fi FTM Responder Arduino Example

This example demonstrates how to use the Fine Timing Measurement (FTM) to calculate the distance from the Access Point and the device. This is calculated by the Wi-Fi Round Trip Time (Wi-Fi RTT) introduced on the [IEEE Std 802.11-2016](https://en.wikipedia.org/wiki/IEEE_802.11mc) standard.

This example will simulate the Router with FTM capability.

This example was based on the [ESP-IDF FTM](https://github.com/espressif/esp-idf/tree/master/examples/wifi/ftm). See the README file for more details about on how to use this feature.

Some usages for this feature includes:

* Indoor positioning systems.
* Navigation.
* Device Location.
* Smart Devices.
* Alarms.

# Supported Targets

Currently, this example supports the following targets:

| Supported Targets | ESP32-S2 | ESP32-C3 |
| ----------------- | -------- | -------- |

## How to Use Example

See the **Initiator** example to prepare the environment.

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

### Configure the Project

To configure this project, you can change the following configuration related to STA:

```c
// Change the SSID and PASSWORD here if needed
const char * WIFI_FTM_SSID = "WiFi_FTM_Responder";
const char * WIFI_FTM_PASS = "ftm_responder";
```

* Change the Wi-Fi `SSID` and `PASSWORD` as the same as the Initiator.

To see more details about FTM, please see the [ESP-IDF docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/network/esp_wifi.html).

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or setting the `upload_port` option on the `platformio.ini` file.

## Log Output

Expected log output:

```
Build:Oct 25 2019
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3ffe6100,len:0x4b0
load:0x4004c000,len:0xa6c
load:0x40050000,len:0x25c4
entry 0x4004c198
Starting SoftAP with FTM Responder support
```

## Troubleshooting

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source.***

* **Programming Fail:** If the programming/flash procedure fails, try reducing the serial connection speed.
* **COM port not detected:** Check the USB cable and the USB to Serial driver installation.

If the error persist, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try the Troubleshooting and to check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
