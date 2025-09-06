# Arduino-ESP32 Zigbee Multistate Input Output Example

This example shows how to configure the Zigbee end device and use it as a Home Automation (HA) multistate input/output device.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Multistate Device Functions

This example demonstrates two different multistate devices:

1. **Standard Multistate Device** (`zbMultistateDevice`): Uses predefined application states from the Zigbee specification
   - Application Type 0: Fan states (Off, On, Auto)
   - Application Type 7: Light states (High, Normal, Low)

2. **Custom Multistate Device** (`zbMultistateDeviceCustom`): Uses user-defined custom states
   - Custom fan states: Off, On, UltraSlow, Slow, Fast, SuperFast

* After this board first starts up, it will be configured as two multistate devices with different state configurations.
* By clicking the button (BOOT) on this board, the devices will cycle through their respective states and report the changes to the network.

## Hardware Required

* A USB cable for power supply and programming

### Configure the Project

Set the Button GPIO by changing the `button` variable. By default, it's the pin `BOOT_PIN` (BOOT button on ESP32-C6 and ESP32-H2).

The example creates two multistate devices:
- **Endpoint 1**: Standard multistate device using predefined Zigbee application types
- **Endpoint 2**: Custom multistate device using user-defined states

You can modify the state names and configurations by changing the following variables:
- `multistate_custom_state_names[]`: Array of custom state names
- Application type and length macros: `ZB_MULTISTATE_APPLICATION_TYPE_X_STATE_NAMES`,
`ZB_MULTISTATE_APPLICATION_TYPE_X_NUM_STATES`, `ZB_MULTISTATE_APPLICATION_TYPE_X_INDEX`
- Device descriptions and application types in the setup() function

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the Coordinator/Router Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee ZCZR 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.

## Troubleshooting

If the End device flashed with this example is not connecting to the coordinator, erase the flash of the End device before flashing the example to the board. It is recommended to do this if you re-flash the coordinator.
You can do the following:

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.
* Add to the sketch `Zigbee.factoryReset();` to reset the device and Zigbee stack.

By default, the coordinator network is closed after rebooting or flashing new firmware.
To open the network you have 2 options:

* Open network after reboot by setting `Zigbee.setRebootOpenNetwork(time);` before calling `Zigbee.begin();`.
* In application you can anytime call `Zigbee.openNetwork(time);` to open the network for devices to join.

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
