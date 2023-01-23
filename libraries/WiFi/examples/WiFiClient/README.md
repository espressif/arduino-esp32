# WiFiClient

This example demonstrates reading and writing data from and to a web service which can be used for logging data, creating insights and taking actions based on those data.

# Supported Targets

Currently, this example supports all SoC with WiFi.


| Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 |


## How to Use Example

Flash this example and observe the serial output. You can also take a look at the values at [https://thingspeak.com/channels/2005329](https://thingspeak.com/channels/2005329)

Please note that this public channel can be accessed by anyone and it is possible that more people will write their values.

### Configure the Project

Change `SSID` and `password` to connect to your WiFi.
Default values will allow you to use this example without any changes. If you want to use your own channel and you don't have one already follow these steps:

* Create an account on [thingspeak.com](https://www.thingspeak.com).
* After logging in, click on the "New Channel" button to create a new channel for your data. This is where your data will be stored and displayed.
* Fill in the Name, Description, and other fields for your channel as desired, then click the "Save Channel" button.
* Take note of the "Write API Key" located in the "API keys" tab, this is the key you will use to send data to your channel.
* Replace the channelID from tab "Channel Settings" and privateKey with "Read API Keys" from "API Keys" tab.
* Replace the host variable with the thingspeak server hostname "api.thingspeak.com"
* Upload the sketch to your ESP32 board and make sure that the board is connected to the internet. The ESP32 should now send data to your Thingspeak channel at the intervals specified by the loop function.
* Go to the channel view page on thingspeak and check the "Field1" for the new incoming data.
* You can use the data visualization and analysis tools provided by Thingspeak to display and process your data in various ways.
* Please note, that Thingspeak accepts only integer values.

#### Config example:

You can find the data to be changed at the top of the file:

```cpp
const char* ssid     = "your-ssid"; // Change this to your WiFi SSID
const char* password = "your-password"; // Change this to your WiFi password

const char* host = "api.thingspeak.com"; // This should not be changed
const int httpPort = 80; // This should not be changed
const String channelID   = "2005329"; // Change this to your channel ID
const String writeApiKey = "V6YOTILH9I7D51F9"; // Change this to your Write API key
const String readApiKey = "34W6LGLIFXD56MPM"; // Change this to your Read API key

// The default example accepts one data filed named "field1"
// For your own server you can ofcourse create more of them.
int field1 = 0;

int numberOfResults = 3; // Number of results to be read
int fieldNumber = 1; // Field number which will be read out
```

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or set the `upload_port` option on the `platformio.ini` file.

## Example Log Output

The initial output which is common for all examples can be ignored:
```
SP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x1 (POWERON),boot:0xc (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd5810,len:0x438
load:0x403cc710,len:0x918
load:0x403ce710,len:0x24e4
entry 0x403cc710
```
Follows the setup output where connection to your WiFi happens:
```
******************************************************
Connecting to your-ssid
.
WiFi connected
IP address:
192.168.1.2
```
Then you can see the write log:
```
HTTP/1.1 200 OK
Date: Fri, 13 Jan 2023 13:12:31 GMT
Content-Type: text/plain; charset=utf-8
Content-Length: 1
Connection: close
Status: 200 OK
Cache-Control: max-age=0, private, must-revalidate
Access-Control-Allow-Origin: *
Access-Control-Max-Age: 1800
X-Request-Id: 188e3464-f155-44b0-96f6-0f3614170bb0
Access-Control-Allow-Headers: origin, content-type, X-Requested-With
Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS, DELETE, PATCH
ETag: W/"5feceb66ffc86f38d952786c6d696c79"
X-Frame-Options: SAMEORIGIN

0
Closing connection
```
Last portion is the read log:
```
HTTP/1.1 200 OK
Date: Fri, 13 Jan 2023 13:12:32 GMT
Content-Type: application/json; charset=utf-8
Transfer-Encoding: chunked
Connection: close
Status: 200 OK
Cache-Control: max-age=7, private
Access-Control-Allow-Origin: *
Access-Control-Max-Age: 1800
X-Request-Id: 91b97016-7625-44f6-9797-1b2973aa57b7
Access-Control-Allow-Headers: origin, content-type, X-Requested-With
Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS, DELETE, PATCH
ETag: W/"8e9c308fe2c50309f991586be1aff28d"
X-Frame-Options: SAMEORIGIN

1e3
{"channel":{"id":2005329,"name":"WiFiCLient example","description":"Default setup for Arduino ESP32 WiFiClient example","latitude":"0.0","longitude":"0.0","field1":"data0","created_at":"2023-01-11T15:56:08Z","updated_at":"2023-01-13T08:13:58Z","last_entry_id":2871},"feeds":[{"created_at":"2023-01-13T13:11:30Z","entry_id":2869,"field1":"359"},{"created_at":"2023-01-13T13:11:57Z","entry_id":2870,"field1":"361"},{"created_at":"2023-01-13T13:12:23Z","entry_id":2871,"field1":"363"}]}
0


Closing connection
```
After this the write+read log repeat every 10 seconds.


## Troubleshooting

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

* **WiFi not connected:** Check the SSID and password and also that the signal has sufficient strength.
* **400 Bad Request:** Check the writeApiKey.
* **404 Not Found:** Check the channel ID.
* **No data on chart / reading NULL:** Data must be sent as an integer, without commas.

If the error persists, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try the Troubleshooting and to check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* ESP32-S3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
