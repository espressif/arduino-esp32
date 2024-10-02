# Arduino-ESP32 Zigbee Networks Scan Example

This example shows how to scan Zigbee Networks.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Example Output

    Setup done
    Loop running...
    Loop running...
    Loop running...
    Loop running...

    Scan done
    2 networks found:
    Nr | PAN ID | CH | Permit Joining | Router Capacity | End Device Capacity | Extended PAN ID
     1 | 0xe6f0 | 14 | Yes            | Yes             | Yes                 | f0:f5:bd:ff:fe:02:3f:24
     2 | 0xa9bb | 24 | No             | Yes             | Yes                 | 60:55:f9:00:00:f7:52:d0

## Hardware Required

* One development board (ESP32-H2 or ESP32-C6) acting as Zigbee coordinator (loaded with `Zigbee_Thermostat` example)
* A USB cable for power supply and programming
* Choose another board (ESP32-H2 or ESP32-C6) as Zigbee end device (loaded with `Zigbee_Temperature_Sensor` example)

### Configure the Project

In this example, the internal temperature sensor task is reading the chip temperature.
Set the Button Switch GPIO by changing the `GPIO_INPUT_IO_TOGGLE_SWITCH` definition. By default, it's the `GPIO_NUM_9` (BOOT button on ESP32-C6 and ESP32-H2).

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.

## Troubleshooting

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
