# Arduino-ESP32 Zigbee Attribute Storage Example

This example shows how to persist a Zigbee attribute in NVS. Analog Input `PresentValue`
is marked persistent with `setAttributePersistent()`. After a reboot the last written
value is restored from the Zigbee NVS dataset.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware Required

* One development board (ESP32-C6 or ESP32-H2) running this example as a Zigbee coordinator
* A USB cable for power supply and programming

### Configure the Project

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select Zigbee coordinator/router mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 8MB with spiffs` (or `Zigbee 4MB with spiffs` on 4MB boards)
* Leave `Erase All Flash Before Sketch Upload` **Disabled** after the first flash, or persist cannot be tested
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose: `Tools -> Core Debug Level: Verbose`.

## Expected Serial Output

On first boot the PresentValue is the cluster default:

```
Persistent PresentValue: 0.0
```

Short-press BOOT to increment PresentValue, then reset the board. The next boot should print the stored value, for example:

```
Persistent PresentValue: 3.0
```

A 3-second BOOT press factory-resets Zigbee NVS (including persisted attributes).

## Troubleshooting

If the persisted value does not survive reboot, make sure you are not erasing flash on upload. A power cycle or EN/reset is enough.

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP-IDF: [ESP-IDF](https://idf.espressif.com)
