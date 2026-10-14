# USB Host HID Combo

Runs the mouse, keyboard, and gamepad handlers at once on an ESP32-S2 / S3 / P4 in host mode. Flash it once and plug in any mix of the three, directly or through a hub.

Serial lines are tagged `[mouse]`, `[keyboard]`, and `[gamepad]`, plus a periodic `[status]` line showing which handlers have claimed a device.

## Flash / run checklist

1. **Board / USB mode:** see the [USBHostMouse checklist](../USBHostMouse/README.md) for the **USB Mode** / **USB CDC On Boot** combination to avoid.
2. **Declare the handlers in the sketch:** `USBHostHIDMouse`, `USBHostHIDKeyboard`, and `USBHostHIDGamepad`. The library ships no instances, so a sketch only pays for the handlers it names.
3. All three **`registerWithHost()`** calls run in `setup()` **before** `USBHost.begin()`. A device that is already powered can finish mounting before `loop()` starts, and a handler registered later never gets to claim it.
4. `loop()` must keep calling **`USBHost.task()`**.

Registration order matters when a device could match more than one handler: the gamepad registers first, so a report-protocol device advertising a Game Pad or Joystick usage goes to it rather than to the mouse handler.

## When a device is not claimed

If `[status]` keeps showing `gamepad=no` for a pad that clearly enumerates, set **`COMBO_DUMP_HID_DESCRIPTOR`** to `1`. That registers `USBHostHIDReportMapDumper`, which prints the parsed report descriptor of every HID interface — verbose with a hub attached, but it shows the usage page and usage the handlers are matching against. The standalone [USBHostGamepad](../USBHostGamepad/README.md) example is a simpler place to test a single pad.

## Sketch options

- **`COMBO_DUMP_HID_DESCRIPTOR`** (default `0`): dump the report descriptor of every HID interface.
- **`COMBO_STATUS_INTERVAL_MS`** (default `3000`): period of the `[status]` line; `0` disables it.
- **`MOUSE_PRINT_ON_ACTIVITY_ONLY`** (default `1`): skip idle mouse reports.
- **`KEYBOARD_NOTIFY_ON_CHANGE_ONLY`** (default `1`) and **`KEYBOARD_LOG_RELEASES`** (default `0`): control how much the keyboard prints.
- **`GAMEPAD_NOTIFY_ON_CHANGE_ONLY`** (default `1`): skip unchanged gamepad reports.
- **`GAMEPAD_TRY_STICK8`** (default `1`): also print the 8-bit stick decoding, which suits most pads.
