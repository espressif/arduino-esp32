# USBHostHID_MultiTest

Integration soak for **multiple HID devices at once** (mouse + keyboard + gamepad).

Before changing HID host arming/abort/mount sync, read
[`docs/USBHostHID_hub_lessons.md`](../../docs/USBHostHID_hub_lessons.md).

## Why this exists

`USBHostHIDCombo` is a demo. This sketch is a clearer pass/fail test:

- `[MOUNT]` / `[UMOUNT]` edges with TinyUSB `addr` / `idx`
- Callback-driven reports (less polling race)
- HID interrupt IN armed on the host worker (after `tuh_task()`), not from loop
- `[stats]` counters so you can see which class is alive

## Limits

Each Arduino handler (`USBHostMouse`, `USBHostKeyboard`, `USBHostGamepad`) claims **one** interface. Two mice at once are not supported by the global singletons.

Composite BLE dongles (keyboard+mouse) plus other HIDs on a hub can exceed ESP32-S3 host channel capacity and fail to enumerate until a device is removed.

Do not use `tuh_hid_receive_abort` from application code during hub plug/unplug — it can wedge DWC2 so further attach/remove events stop.

## Quick run

1. Flash this example (USB-OTG host mode).
2. Open Serial Monitor 115200.
3. Follow the numbered test plan in `USBHostHID_MultiTest.ino`.

Optional: set `MULTI_DUMP_HID_DESCRIPTOR` to `1` if a device never mounts.
