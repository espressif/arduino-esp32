# WiFiScan Example

This example demonstrates how to use the WiFi library to scan available WiFi networks and print the results.

This example shows the basic functionality of the dual antenna capability.

# Supported Targets

This example is compatible with the ESP32-WROOM-DA.

## How to Use Example

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or set the `upload_port` option on the `platformio.ini` file.

## Example/Log Output

```
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:1412
load:0x40078000,len:13400
load:0x40080400,len:3672
entry 0x400805f8
Setup done
scan start
scan done
17 networks found
1: IoTNetwork (-62)*
2: WiFiSSID (-62)*
3: B3A7992 (-63)*
4: WiFi (-63)
5: IoTNetwork2 (-64)*
...
```

## Troubleshooting

***Important: Be sure you're using a good quality USB cable and you have enough power source for your project.***

* **Programming Fail:** If the programming/flash procedure fails, try to reduce the serial connection speed.
* **COM port not detected:** Check the USB cable connection and the USB to Serial driver installation.

If the error persists, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try the Troubleshooting and to check if the same issue was already created by someone else.

## Resources

* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-WROOM-DA Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-da_datasheet_en.pdf)
