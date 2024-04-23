# Multi Homed Servers

This example tests support for multi-homed servers, i.e. a distinct web servers on distinct IP interface.

It only tests the case n=2 because on a basic ESP32 device, we only have two IP interfaces, namely the WiFi station interfaces and the WiFi soft AP interface.
For this to work, the WebServer and the NetworkServer classes must correctly handle the case where an IP address is passed to their relevant constructor.
It also requires WebServer to work with multiple, simultaneous instances.

Testing the WebServer and the NetworkServer constructors was the primary purpose of this script.
The part of WebServer used by this sketch does seem to work with multiple, simultaneous instances.
However there is much functionality in WebServer that is not tested here. It may all be well, but that is not proven here.

This sketch starts the mDNS server, as did HelloServer, and it resolves esp32.local on both interfaces, but was not otherwise tested.
This script also tests that a server not bound to a specific IP address still works.

We create three, simultaneous web servers, one specific to each interface and one that listens on both:

| name    | IP Address      | Port |
| ----    | ----------      | ---- |
| server0 | INADDR_ANY      | 8080 |
| server1 | station address | 8081 |
| server2 | soft AP address | 8081 |

The expected responses to a browser's requests are as follows:

#### 1. The Client connected to the same WLAN as the station:

| Request URL                | Response |
| -----------                | -------- |
| [http://stationaddress:8080](http://stationaddress:8080) | Hello from server0 who listens on both WLAN and own Soft AP |
| [http://stationaddress:8081](http://stationaddress:8081) | Hello from server1 who listens only on WLAN |

#### 2. The Client is connected to the soft AP:

| Request URL               | Response |
| -----------               | -------- |
| [http://softAPaddress:8080](http://softAPaddress:8080) | Hello from server0 who listens on both WLAN and own Soft AP |
| [http://softAPaddress:8081](http://softAPaddress:8081) | Hello from server2 who listens only on own Soft AP |

#### 3. The Client is connect to either WLAN or SoftAP:

| Request URL               | Response |
| -----------               | -------- |
| [http://esp32.local:8080](http://esp32.local:8080) | Hello from server0 who listens on both WLAN and own Soft AP |
| [http://esp32.local:8081](http://esp32.local:8081) | Hello from server1 who listens only on WLAN |

MultiHomedServers was originally based on HelloServer.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- |

## How to Use Example

Change the SSID and password in the example to your WiFi and flash the example.
Open a serial terminal and the example will write the exact addresses with used IP addresses you can use to test the servers.

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or set the `upload_port` option on the `platformio.ini` file.

## Example Log Output

```
Multi-homed Servers example starting
Connecting ...
Connected to "WiFi_SSID", IP address: "192.168.42.24
Soft AP SSID: "ESP32", IP address: 192.168.4.1
MDNS responder started
SSID: WiFi_SSID
	http://192.168.42.24:8080
	http://192.168.42.24:8081
SSID: ESP32
	http://192.168.4.1:8080
	http://192.168.4.1:8081
Any of the above SSIDs
	http://esp32.local:8080
	http://esp32.local:8081
HTTP server0 started
HTTP server1 started
HTTP server2 started
```

## Known issues

`http://esp32.local` Does not work on some Android phones

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
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
