# Arduino-ESP32 Zigbee Multi-Switch Example

This example demonstrates how to configure a Zigbee device as a multi-switch controller that can control up to three different Zigbee lights independently. The switch can operate in either coordinator or router mode, making it compatible with Home Assistant integration.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* One development board (ESP32-H2 or ESP32-C6) acting as Zigbee multi-switch controller
* One or more Zigbee light devices (loaded with Zigbee_On_Off_Light example)
* A USB cable for power supply and programming

### Configure the Project

The example uses the BOOT button (pin 9) on ESP32-C6 and ESP32-H2 as the physical switch input. The switch can be configured to operate in two modes:

1. **Coordinator Mode**: For running your own Zigbee network
2. **Router Mode**: For Home Assistant integration

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`
* Select the Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`

## Features

The multi-switch example provides the following functionality:

1. **Light Configuration**
   - Configure up to 3 different lights using their endpoint and IEEE address
   - Configuration is stored in NVS (Non-Volatile Storage) and persists after power loss
   - Remove configured lights when needed

2. **Control Commands**
   - Control all bound lights simultaneously:
     - Turn all bound lights ON
     - Turn all bound lights OFF
     - Toggle all bound lights
   - Control individual lights (1-3):
     - Turn light ON
     - Turn light OFF
     - Toggle light

3. **Network Management**
   - Factory reset capability
   - Open network for device joining
   - View bound devices and current light configurations

## Serial Commands

The example accepts the following commands through the serial interface:

* `config` - Configure a new light (requires light number, endpoint, and IEEE address)
* `remove` - Remove a configured light
* `on` - Turn all bound lights ON
* `off` - Turn all bound lights OFF
* `toggle` - Toggle all bound lights
* `1on`, `2on`, `3on` - Turn individual light ON
* `1off`, `2off`, `3off` - Turn individual light OFF
* `1toggle`, `2toggle`, `3toggle` - Toggle individual light
* `freset` - Perform factory reset
* `open_network` - Open network for device joining (only for coordinator role)

## Troubleshooting

If the End device flashed with the example `Zigbee_On_Off_Light` is not connecting to the coordinator, erase the flash of the End device before flashing the example to the board. It is recommended to do this if you re-flash the coordinator.
You can do the following:

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`
* In the `Zigbee_On_Off_Light` example sketch call `Zigbee.factoryReset()`

By default, the coordinator network is closed after rebooting or flashing new firmware.
To open the network you have 2 options:

* Open network after reboot by setting `Zigbee.setRebootOpenNetwork(time)` before calling `Zigbee.begin()`
* In application you can anytime call `Zigbee.openNetwork(time)` to open the network for devices to join

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

* **LED not blinking:** Check the wiring connection and the IO selection
* **Programming Fail:** If the programming/flash procedure fails, try reducing the serial connection speed
* **COM port not detected:** Check the USB cable and the USB to Serial driver installation

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
