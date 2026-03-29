# Arduino-ESP32 Zigbee Privilege Command Example

This example shows how to intercept standard ZCL cluster commands using the privilege command API.

## Overview

Normally, when a Zigbee controller sends a cluster-specific command (e.g. On/Off Toggle), the ZBOSS stack processes it internally and updates the attributes without notifying the application. The privilege command API allows you to **observe these commands** before (or instead of) the stack handling them.

This is useful for:
- Implementing custom behavior for commands like "Off with effect" (command `0x40` on the On/Off cluster)
- Logging or monitoring incoming ZCL commands
- Reacting to specific commands from controllers like Philips Hue

## How It Works

1. **Register commands** with `addPrivilegeCommand(cluster_id, command_id)` to tell the stack which commands to forward to your callback
2. **Set a callback** with `onPrivilegeCommand(callback)` to receive intercepted commands
3. The callback receives an `esp_zb_zcl_privilege_command_message_t` with full command details (cluster, command ID, source address, payload)

The stack still processes registered commands normally — the callback is an observer, not a filter.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* One development board (ESP32-H2 or ESP32-C6) running this example as a Zigbee end device
* A Zigbee coordinator to send commands — use the `Zigbee_On_Off_Switch` example on another board, or any Zigbee controller (e.g. Zigbee2MQTT)
* A USB cable for power supply and programming

### Configure the Project

Set the LED GPIO by changing the `led` variable. By default, `RGB_BUILTIN` is used.

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.

## Expected Serial Output

When a controller sends On/Off commands to this device:

```
--- Privilege command received ---
  Cluster: 0x0006
  Command: 0x02
  Direction: 0 (0=client-to-server, 1=server-to-client)
  Src endpoint: 1, address: 0x0001
---------------------------------
Light state: ON, level: 255
```

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
