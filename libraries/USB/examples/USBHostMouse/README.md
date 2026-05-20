# USB Host Mouse

Reads a **boot-protocol** USB HID mouse (buttons, X/Y motion, wheel) on ESP32-S2 / S3 / P4 in USB host mode.

## Flash / run checklist

1. **Board:** ESP32-S3-USB-OTG, or any board with USB OTG where you set **Tools → USB Mode** to host-capable mode (e.g. **Hardware CDC and JTAG**). The sketch matches the gamepad example: on **ESP32-S3-USB-OTG** the default USB-OTG menu is OK (`ARDUINO_ESP32_S3_USB_OTG`).
2. **`USBHostMouse.registerWithHost()`** runs in `setup()` **before** `USBHost.begin()`. If the mouse enumerates before your handler is registered, you may need to **plug after boot** or **unplug/replug** once.
3. **VBUS:** On ESP32-S3-USB-OTG the sketch calls `usbHostEnable` / `usbHostPower` so the device port is powered.
4. **Hub:** If direct attach misbehaves (like some gamepads), try a **USB hub** between the board and the mouse.

## What works on PC vs ESP

The host stack only claims interfaces that report **boot protocol mouse** (`HID_ITF_PROTOCOL_MOUSE`). Mice that work on a PC but only as **report-protocol** or composite-only may not be claimed here—use a standard HID boot mouse for this example.

## Sketch options

- **`MOUSE_PRINT_ON_ACTIVITY_ONLY`** (default `1`): skip Serial lines when the report is all zeros and buttons unchanged (many mice resend idle reports constantly). Set to `0` to print every report.
