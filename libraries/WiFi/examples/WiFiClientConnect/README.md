# NetworkClientConnect Example

This example demonstrates how to connect to the WiFi and manage the status and disconnection from STA.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 |
| ----------------- | ----- | -------- | -------- |

## How to Use Example

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or set the `upload_port`` option on the `platformio.ini` file.

## Example/Log Output

```
[WiFi] Connecting to MyWiFiNetwork
[    66][D][WiFiGeneric.cpp:929] _eventCallback(): Arduino Event: 0 - WIFI_READY
[   150][V][WiFiGeneric.cpp:338] _arduino_event_cb(): STA Started
[   151][V][WiFiGeneric.cpp:97] set_esp_interface_ip(): Configuring Station static IP: 0.0.0.0, MASK: 0.0.0.0, GW: 0.0.0.0
[   151][D][WiFiGeneric.cpp:929] _eventCallback(): Arduino Event: 2 - STA_START
[WiFi] WiFi is disconnected
[   234][V][WiFiGeneric.cpp:353] _arduino_event_cb(): STA Connected: SSID: MyWiFiNetwork, BSSID: xx:xx:xx:xx:xx:xx, Channel: 8, Auth: WPA2_PSK
[   235][D][WiFiGeneric.cpp:929] _eventCallback(): Arduino Event: 4 - STA_CONNECTED
[   560][V][WiFiGeneric.cpp:367] _arduino_event_cb(): STA Got New IP:192.168.68.114
[   561][D][WiFiGeneric.cpp:929] _eventCallback(): Arduino Event: 7 - STA_GOT_IP
[   564][D][WiFiGeneric.cpp:1004] _eventCallback(): STA IP: 192.168.68.114, MASK: 255.255.255.0, GW: 192.168.68.1
[WiFi] WiFi is connected!
[WiFi] IP address: 192.168.68.114
[WiFi] Disconnecting from WiFi!
[  2633][V][WiFiGeneric.cpp:360] _arduino_event_cb(): STA Disconnected: SSID: MyWiFiNetwork, BSSID: xx:xx:xx:xx:xx:xx, Reason: 8
[  2634][D][WiFiGeneric.cpp:929] _eventCallback(): Arduino Event: 5 - STA_DISCONNECTED
[  2635][V][WiFiGeneric.cpp:341] _arduino_event_cb(): STA Stopped
[  2641][W][WiFiGeneric.cpp:953] _eventCallback(): Reason: 8 - ASSOC_LEAVE
[  2654][D][WiFiGeneric.cpp:975] _eventCallback(): WiFi the station is disconnected
[  2661][D][WiFiGeneric.cpp:929] _eventCallback(): Arduino Event: 3 - STA_STOP
[WiFi] Disconnected from WiFi!
...
```

## Troubleshooting

***Important: Be sure you're using a good quality USB cable that has enough power for your project.***

* **Programming Fail:** If the programming/flash procedure fails, try to reduce the serial connection speed.
* **COM port not detected:** Check the USB cable connection and the USB to Serial driver installation.

If the error persists, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
