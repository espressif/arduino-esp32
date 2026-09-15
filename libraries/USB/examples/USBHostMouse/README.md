# USB Host Mouse

Reads a USB HID mouse (boot protocol, or report-protocol with Generic Desktop Mouse usage) on ESP32-S2 / S3 / P4 in USB host mode.

## Flash / run checklist

1. **Board:** ESP32-S3-USB-OTG, or any USB-OTG board with host-capable **Tools → USB Mode**.
2. Call **`USBHostMouse.registerWithHost()`** in `setup()` **before** `USBHost.begin()`.
3. **VBUS:** On ESP32-S3-USB-OTG, `USBHost.begin()` enables the host port; the sketch may also call `usbHostEnable` / `usbHostPower`.
4. **Hub:** If direct attach misbehaves, try a USB hub between the board and the mouse.

## Sketch options

- **`MOUSE_PRINT_ON_ACTIVITY_ONLY`** (default `1`): skip Serial lines when the report is idle and buttons unchanged. Set to `0` to print every report.

For descriptor dumps while debugging claim issues, use `USBHostHIDCombo` with its dump flag set to `1`.
