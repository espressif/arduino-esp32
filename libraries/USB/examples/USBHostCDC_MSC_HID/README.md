# USB Host CDC + MSC + HID

Runs three host classes side by side on an ESP32-S2 / S3 / P4: a USB serial adapter, a flash drive, and a mouse and keyboard. This is the Arduino equivalent of TinyUSB's `cdc_msc_hid` host demo.

- **CDC:** bytes from the USB serial device appear on the Serial Monitor, and what you type is sent back to it.
- **MSC:** a flash drive is mounted as a FAT volume at `/usb` and its root directory is listed.
- **HID:** mouse and keyboard reports are printed as they arrive.

Use a hub if you want more than one of them attached at the same time.

## Flash / run checklist

1. **Board / USB mode:** see the [USBHostMouse checklist](../USBHostMouse/README.md) for the **USB Mode** / **USB CDC On Boot** combination to avoid. This sketch prints its own `ARDUINO_USB_MODE` / `CDC_ON_BOOT` / `MSC_ON_BOOT` / `DFU_ON_BOOT` build flags at startup, which is the quickest way to confirm what you actually built.
2. **Declare the instances in the sketch:** `USBHostSerialClass`, `fs::USBMSCFS`, `USBHostHIDMouse`, and `USBHostHIDKeyboard`. The library ships no instances, so a sketch only pays for the classes it names. `USBHostMSCClass USBHostMSC` is the exception — the FatFs layer needs a fixed instance, so the library still provides it.
3. Both HID **`registerWithHost()`** calls run in `setup()` **before** `USBHost.begin()`.
4. `loop()` must keep calling **`USBHost.task()`**.

## Flash drive paths

The volume is registered with VFS at `/usb`, but `USBMSCFS` paths are volume-relative — open `/` and `/file.txt`, not `/usb/file.txt`. Unplugging while a file is open loses unwritten data, so call `USBMSCFS.end()` before pulling the drive. See [USBHostMSC_Test](../USBHostMSC_Test/README.md) for the full File API exercise.

## Only one CDC device at a time

The prebuilt libraries are built with `CFG_TUH_CDC == 1`, so TinyUSB tracks a single CDC interface. A second serial adapter is enumerated but not claimed.
