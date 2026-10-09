# Arduino-ESP32 Door Lock Example

This example shows how to configure the Zigbee end device and use it as a Home Automation (HA) door lock device.

The door lock accepts the Lock Door and Unlock Door commands from the Zigbee network (for example from Home Assistant using ZHA)
and reports the LockState attribute. The RGB LED shows the current state (green = locked/secured, red = unlocked/unsecured).
A short press of the BOOT button simulates a local operation of the lock (toggles and reports the state),
a long press (more than 3 seconds) performs a factory reset.

To see if the communication with your Zigbee network works, use the Serial monitor and watch for output there.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 | ESP32-C5 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

## Hardware Required

* A USB cable for power supply and programming
* Board (ESP32-H2 or ESP32-C6) as Zigbee end device and upload the Zigbee_Door_Lock example
* Zigbee network / coordinator (Zigbee2mqtt, ZigbeeHomeAssistant (ZHA) like application, ...)

### Configure the Project

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Tools / USB CDC On Boot: "Enabled"
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.

## Supported features

Only the features implemented by the Zigbee stack are available:

* Lock Door and Unlock Door commands (the sketch callbacks can reject a command by returning `false`)
* LockState attribute (reported to the network), LockType and ActuatorEnabled attributes
* PIN code users managed by the coordinator (SetPINCode, GetPINCode, ClearPINCode, ClearAllPINCodes, SetUserStatus), for example with the Home Assistant "Set lock user code" action.
  The users are stored in NVS with `setUserStorage(true)` and restored after a reboot.
* The last LockState is kept after a reboot as a Zigbee persistent attribute (`setAttributePersistent()`, see the Zigbee_Attribute_Storage example). `restoreLockState()` reads it back.
* Operation event notifications (`reportOperationEvent()`), shown by ZHA as lock events (keypad / manual / RF source and the user)
* Simulated keypad: type a PIN code in the Serial Monitor to lock/unlock with it

Schedules, RFID users, user types and event masks are not supported yet.

The PIN code features need the Zigbee libraries built with the esp-zigbee changes for the Door Lock PIN commands. The persistent attribute needs esp-zigbee-lib 2.0.5 or newer.
A long press of the BOOT button also removes the stored PIN users, the PIN codes are stored in NVS as plain text.

## Managing PIN codes in Home Assistant (ZHA)

ZHA has no user interface for the lock PIN codes, they are managed with actions.
Open `Developer tools -> Actions`, switch to `YAML mode`, paste one of the actions below and press `Perform action`.
Replace `lock.zigbee_door_lock` with the entity ID of your lock (`Settings -> Devices & services -> Entities`).

The `code_slot` is the user number and starts at 1 in Home Assistant (user ID 0 in the sketch). The number of slots is set by `setMaxUsers()` (10 by default).
The PIN code has 4 to 16 digits.

Set (add or change) a PIN code:

```yaml
action: zha.set_lock_user_code
data:
  entity_id: lock.zigbee_door_lock
  code_slot: 1
  user_code: "1234"
```

Disable a PIN code, it stays stored but the lock does not accept it:

```yaml
action: zha.disable_lock_user_code
data:
  entity_id: lock.zigbee_door_lock
  code_slot: 1
```

Enable it again:

```yaml
action: zha.enable_lock_user_code
data:
  entity_id: lock.zigbee_door_lock
  code_slot: 1
```

Remove a PIN code:

```yaml
action: zha.clear_lock_user_code
data:
  entity_id: lock.zigbee_door_lock
  code_slot: 1
```

Notes:

* Home Assistant does not show the stored codes. Use the Serial Monitor of the sketch to check the changes, the `userChanged()` callback prints the user, its status and the PIN length.
* A PIN code can be used by one user only, setting a code which is already used in another slot fails.
* To test a code, type it in the Serial Monitor of the sketch (the simulated keypad). The lock toggles and the operation event with the user is sent to Home Assistant.
  A wrong code is sent as an "invalid PIN" event.
* The events are shown in the lock's `Logbook` and are fired as `zha_event` events (`Developer tools -> Events -> Listen to events`), so they can be used in automations.

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
