# USB Host Keyboard

Reads a **boot-protocol** USB HID keyboard on ESP32-S2 / S3 / P4 in USB host mode.

## Setup

1. **Board / USB mode:** Any OTG-capable ESP32-S2 / S3 / P4 board. See the [USBHostMouse checklist](../USBHostMouse/README.md) for the **USB Mode** / **USB CDC On Boot** combination to avoid.
2. **Declare the handler in the sketch:** `USBHostHIDKeyboard USBHostKeyboard;`. The library ships no instance, so a sketch only pays for the handlers it names.
3. **`USBHostKeyboard.registerWithHost()`** before **`USBHost.begin()`**. Plug the keyboard after boot or replug if it enumerated too early.
4. **VBUS:** On ESP32-S3-USB-OTG, `USBHost.begin()` enables the host port through `USBHostBoardInit()`; the sketch may also call `usbHostEnable` / `usbHostPower`.
5. **Hub:** If direct attach is unreliable, try a USB hub.

## API

- **`getModifiers()`** — bitmask (`USBHOST_KEY_MOD_LEFT_SHIFT`, etc.).
- **`getKeys(uint8_t[6])`** — up to six **HID key usages** (not ASCII); `0` = empty slot.
- **`isKeyDown(hid_usage)`** — scan current report.
- **`setNotifyOnChangeOnly(true)`** — ignore repeated identical reports (default **on** in the example via `KEYBOARD_NOTIFY_ON_CHANGE_ONLY`).
- **`setReportCallback(fn, arg)`** — optional; runs when a report arrives (or only on change if notify-on-change is on).
- **`capsLock()` / `numLock()` / `scrollLock()`**, **`getLeds()`** — current lock state.
- **`setLeds(mask)`** — force it, e.g. `setLeds(USBHOST_KEY_LED_NUM_LOCK)` to start with the keypad live.
- **`setLockHandling(false)`** — stop driving the locks and the LEDs; Caps/Num/Scroll then only arrive as key usages.

## Lock keys and LEDs

Caps Lock, Num Lock and Scroll Lock are handled the way a PC does it: the host owns the state, toggles it on each press and writes it back to the keyboard as an **OUTPUT report**, which is what lights the LEDs. It follows through to decoding — Caps Lock inverts Shift for letters in `toAscii()`, and the keypad only produces digits with Num Lock on (the operators `/ * - +` always work).

The LED write is a control transfer issued from the host worker. If the keyboard rejects it, the library stops retrying after a few attempts and everything else keeps working.

## Limitations

- Only interfaces that enumerate as **boot keyboard** (`HID_ITF_PROTOCOL_KEYBOARD`) are claimed. Some PC keyboards expose extra interfaces or report-only modes; use a standard USB HID keyboard for this example.
