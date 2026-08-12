# USB Host Gamepad

Reads HID **Game Pad**, **Joystick**, or **Multi-axis controller** interfaces over USB host (ESP32-S2 / S3 / P4).

## Why not the mouse example?

Boot-protocol **mice** use `HID_ITF_PROTOCOL_MOUSE`. Many USB devices (gamepads, some “air mice”, composite gadgets) expose **report protocol** HID with a Game Pad/Joystick usage instead. Use this sketch for those devices.

## Usage

1. Board with USB OTG in host mode (e.g. ESP32-S3 USB OTG).
2. **`USBHostGamepad.registerWithHost()` must run in `setup()` before `USBHost.begin()`.**  
   If the pad is already powered when `begin()` runs, HID mount can finish before `loop()` — then a handler registered only from `available()` never runs `claim()`, and you get no reports until **unplug/replug**.
3. Plug the gamepad **after** boot (or replug once).

### Less Serial spam (only when inputs change)

HID gamepads often resend the **same** report many times per second. Use:

- `USBHostGamepad.setNotifyOnChangeOnly(true);` (the example enables this via `GAMEPAD_NOTIFY_ON_CHANGE_ONLY`)
- `USBHostGamepad.setReportCallback(fn, user);` — callback runs only when report bytes **change** (with notify-on-change on).

You can also poll `available()` / `reportData()`; with notify-on-change, `available()` is true only after a **changed** report.

### ESP32-S3-USB-OTG and hubs

Some gamepads work more reliably **through a USB hub** than when plugged directly into the OTG port.

Claim matching uses Desktop + Button page, Rx/Ry, hat, or multiple axes — not only “Game Pad” usage. For descriptor dumps while debugging, use `USBHostHIDCombo` / `USBHostHID_MultiTest` with their dump flag set to `1`.
