# WiFiScanAsync Example

This example demonstrates how to use the WiFi library to scan available WiFi networks in asynchronous mode and print the results.

## Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 | ESP32-C6 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

## How to Use Example

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or setting the `upload_port` option on the `platformio.ini` file.

## Example/Log Output

```
Setup done
Scan start
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...
Loop running...

Scan done
17 networks found
Nr | SSID            | RSSI | CH | Encryption
 1 | IoTNetwork      |  -62 |  1 | WPA2
 2 | WiFiSSID        |  -62 |  1 | WPA2-EAP
 3 | B3A7992         |  -63 |  6 | WPA+WPA2
 4 | WiFi            |  -63 |  6 | WPA3
 5 | IoTNetwork2     |  -64 | 11 | WPA2+WPA3
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
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* ESP32-C6 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
