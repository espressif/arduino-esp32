# USB Host Keyboard

Reads a **boot-protocol** USB HID keyboard on ESP32-S2 / S3 / P4 in USB host mode.

## Setup

1. **Board / USB mode:** Same as [USBHostMouse](../USBHostMouse/README.md) and [USBHostGamepad](../USBHostGamepad/README.md) — OTG-capable board, **Hardware CDC** on most boards, or **ESP32-S3-USB-OTG**.
2. **`USBHostKeyboard.registerWithHost()`** before **`USBHost.begin()`**. Plug the keyboard after boot or replug if it enumerated too early.
3. **VBUS** on ESP32-S3-USB-OTG is enabled in the sketch.
4. **Hub:** If direct attach is unreliable, try a USB hub.

## API

- **`getModifiers()`** — bitmask (`USBHOST_KEY_MOD_LEFT_SHIFT`, etc.).
- **`getKeys(uint8_t[6])`** — up to six **HID key usages** (not ASCII); `0` = empty slot.
- **`isKeyDown(hid_usage)`** — scan current report.
- **`setNotifyOnChangeOnly(true)`** — ignore repeated identical reports (default **on** in the example via `KEYBOARD_NOTIFY_ON_CHANGE_ONLY`).
- **`setReportCallback(fn, arg)`** — optional; runs when a report arrives (or only on change if notify-on-change is on).

## Limitations

- Only interfaces that enumerate as **boot keyboard** (`HID_ITF_PROTOCOL_KEYBOARD`) are claimed. Some PC keyboards expose extra interfaces or report-only modes; use a standard USB HID keyboard for this example.
- **LEDs** (Num/Caps lock) are not driven here; that would require host **OUTPUT** reports (`tuh_hid_set_report`), which can be added later if needed.
