# Arduino-ESP32 Example/Library Name ==(REQUIRED)==

==*Add a brief description about this example/library here!*==

This example/library demonstrates how to create a new example README file.

# Supported Targets ==(REQUIRED)==

==*Add the supported devices here!*==

Currently, this example supports the following targets.

| Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 |
| ----------------- | ----- | -------- | -------- |

## How to Use Example/Library ==(OPTIONAL)==

==*Add a brief description on how to use this example.*==

* How to install the Arduino IDE: [Install Arduino IDE](https://github.com/espressif/arduino-esp32/tree/master/docs/arduino-ide).

### Hardware Connection ==(OPTIONAL)==

==*Add a brief description about wiring or any other hardware specific connection.*==

To use this example, you need to connect the LED to the `GPIOx`.

SDCard GPIO connection scheme:

| SDCard Pin | Function | GPIO |
| ----------- | -------- | ------ |
| 1 | CS | GPIO5 |
| 2 | DI/MOSI | GPIO23 |
| 3 | VSS/GND | GND |
| 4 | VDD/3V3 | 3V3 |
| 5 | SCLK | GPIO18 |
| 6 | VSS/GND | GND |
| 7 | DO/MISO | GPIO19 |

To add images, please create a folder `_asset` inside the example folder to add the relevant images.

### Configure the Project ==(OPTIONAL)==

==*Add a brief description about this example here!*==

Set the LED GPIO by changing the `LED_BUILTIN` value in the function `pinMode(LED_BUILTIN, OUTPUT);`. By default, the GPIO is: `GPIOx`.

#### Example for the GPIO4:

==*Add some code explanation if relevant to the example.*==

```cpp
// the setup function runs once when you press reset or power the board
void setup() {
// initialize digital pin 4 as an output.
pinMode(4, OUTPUT);
}
```

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

#### Using Platform IO

* Select the COM port: `Devices` or setting the `upload_port` option on the `platformio.ini` file.

## Example/Log Output ==(OPTIONAL)==

==*Add the log/serial output here!*==

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
ESP32 Chip model = ESP32-D0WDQ5 Rev 3
This chip has 2 cores
Chip ID: 3957392
```

## Troubleshooting ==(REQUIRED)==

==*Add specific issues you may find by using this example here!*==

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

* **LED not blinking:** Check the wiring connection and the IO selection.
* **Programming Fail:** If the programming/flash procedure fails, try reducing the serial connection speed.
* **COM port not detected:** Check the USB cable and the USB to Serial driver installation.

If the error persist, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute ==(REQUIRED)==

==*Do not change! Keep as is.*==

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try the Troubleshooting and to check if the same issue was already created by someone else.

## Resources ==(REQUIRED)==

==*Do not change here! Keep as is or add only relevant documents/info for this example. Do not add any purchase link/marketing stuff*==

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
