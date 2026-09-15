# USB Host Mouse

Reads a USB HID mouse (boot protocol, or report-protocol with Generic Desktop Mouse usage) on ESP32-S2 / S3 / P4 in USB host mode.

## Flash / run checklist

1. **Board / USB mode:** any ESP32-S2 / S3 / P4 board with a USB OTG port. The host stack needs the OTG peripheral, so the TinyUSB *device* stack must not be holding it — on ESP32-S3, do not combine **Tools → USB Mode: USB-OTG (TinyUSB)** with **Tools → USB CDC On Boot: Enabled**. Either mode works on its own: **Hardware CDC and JTAG** puts `Serial` on the native port, **USB-OTG** with CDC on boot disabled puts it on UART0. On **ESP32-S3-USB-OTG** the two menus are tied together, so either choice is fine as-is, and **ESP32-P4** drives host and device from separate controllers, so any combination works.
2. **Declare the handler in the sketch:** `USBHostHIDMouse USBHostMouse;`. The library ships no instance, so a sketch only pays for the handlers it names.
3. Call **`USBHostMouse.registerWithHost()`** in `setup()` **before** `USBHost.begin()`.
4. **VBUS:** On ESP32-S3-USB-OTG, `USBHost.begin()` enables the host port; the sketch may also call `usbHostEnable` / `usbHostPower`.
5. **Hub:** If direct attach misbehaves, try a USB hub between the board and the mouse.

## Sketch options

- **`MOUSE_PRINT_ON_ACTIVITY_ONLY`** (default `1`): skip Serial lines when the report is idle and buttons unchanged. Set to `0` to print every report.

For descriptor dumps while debugging claim issues, use `USBHostHIDCombo` with its dump flag set to `1`.
