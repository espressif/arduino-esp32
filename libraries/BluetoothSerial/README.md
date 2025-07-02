## Bluetooth Serial Library

A simple Serial compatible library using ESP32 classical Bluetooth Serial Port Profile (SPP)

Note: Since version 3.0.0 this library does not support legacy pairing (using fixed PIN consisting of 4 digits).

### How to use it?

There are 3 basic use cases: phone, other ESP32 or any MCU with a Bluetooth serial module

#### Phone

- Download one of the Bluetooth terminal apps to your smartphone

    - For [Android](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal)
    - For [iOS](https://itunes.apple.com/us/app/hm10-bluetooth-serial-lite/id1030454675)

- Flash an example sketch to your ESP32

- Scan and pair the device to your smartphone

- Open the Bluetooth terminal app and connect

- Enjoy

#### ESP32

You can flash one of the ESP32 with the example [`SerialToSerialBTM`](https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBTM/SerialToSerialBTM.ino) (the Master) and another ESP32 with [`SerialToSerialBT`](https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino) (the Slave).
Those examples are preset to work out-of-the-box but they should be scalable to connect multiple Slaves to the Master.

#### 3rd party Serial Bluetooth module

Using a 3rd party Serial Bluetooth module will require to study the documentation of the particular module in order to make it work, however, one side can utilize the mentioned [`SerialToSerialBTM`](https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBTM/SerialToSerialBTM.ino) (the Master) or [`SerialToSerialBT`](https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino) (the Slave).

### Pairing options

There are two easy options and one difficult.

The easy options can be used as usual. These offer pairing with and without Secure Simple Pairing (SSP).

The difficult option offers legacy pairing (using fixed PIN) however this must be compiled with Arduino as an IDF component with disabled sdkconfig option `CONFIG_BT_SSP_ENABLED`.

#### Without SSP

This method will authenticate automatically any attempt to pair and should not be used if security is a concern! This option is used for the examples [`SerialToSerialBTM`](https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBTM/SerialToSerialBTM.ino) and [`SerialToSerialBT`](https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino).

### With SSP

The usage of SSP provides a secure connection. This option is demonstrated in the example `SerialToSerialBT_SSP``](https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBT_SSP/SerialToSerialBT_SSP.ino)

The Secure Simple Pairing is enabled by calling method `enableSSP` which has two variants - one is backward compatible without parameter `enableSSP()` and second with parameters `enableSSP(bool inputCapability, bool outputCapability)`. Similarly, the SSP can be disabled by calling `disableSSP()`.

Both options must be called before `begin()` or if it is called after `begin()` the driver needs to be restarted (call `end()` followed by `begin()`) in order to take in effect enabling or disabling the SSP.

#### The parameters define the method of authentication:

**inputCapability** - Defines if ESP32 device has input method (Serial terminal, keyboard or similar)

**outputCapability** - Defines if ESP32 device has output method (Serial terminal, display or similar)

* **inputCapability=true and outputCapability=true**
    * Both devices display randomly generated code and if they match the user will authenticate pairing on both devices.
    * This must be implemented by registering a callback via `onConfirmRequest()` and in this callback the user will input the response and call `confirmReply(true)` if the authenticated, otherwise call `confirmReply(false)` to reject the pairing.
* **inputCapability=false and outputCapability=false**
    * Only the other device authenticates pairing without any pin.
* **inputCapability=false and outputCapability=true**
    * Only the other device authenticates pairing without any pin.
* **inputCapability=true and outputCapability=false**
    * The user will be required to input the passkey to the ESP32 device to authenticate.
    * This must be implemented by registering a callback via `onKeyRequest`()` and in this callback the entered passkey will be responded via `respondPasskey(passkey)`

### Legacy Pairing (IDF component)

To use Legacy pairing you will have to use [Arduino as an IDF component](https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/esp-idf_component.html) and disable option `CONFIG_BT_SSP_ENABLED`.
Please refer to the documentation on how to setup Arduino as an IDF component and when you are done, run `idf.py menuconfig` navigate to `Component Config -> Bluetooth -> Bluedroid -> [ ] Secure Simple Pairing` and disable it.
While in the menuconfig you will also need to change the partition scheme `Partition Table -> Partition Table -> (X) Single Factory app (large), no OTA`.
After these changes save & quit menuconfig and you are ready to go: `idf.py  monitor flash`.
Please note that to use the PIN in smartphones and computers you need to use characters `SerialBT.setPin("1234", 4);` not a number `SerialBT.setPin(1234, 4);` . Numbers CAN be used if the other side uses them too, but phones and computers use characters.
