# USB Host MSC Test

Same exercises as **`libraries/SD/examples/SD_Test`** (`listDir`, `mkdir`, read/write/append, rename, `testFileIO`, `totalBytes` / `usedBytes`), but on a **USB flash drive** via **`USBMSCFS`**.

1. FAT-formatted stick, USB host port, **`USBHost.begin()`** + wait for **`USBHostMSC.mounted()`**.
2. **`USBMSCFS.begin("/usb")`** (or change **`USB_MSC_MOUNTPOINT`** in the sketch).
3. After tests, the sketch calls **`USBMSCFS.end()`** so you can unplug cleanly.

Optional: **`USB_MSC_WAIT_MS`** (default 60000) — max time to wait for enumeration in `setup()`.

`loop()` only runs **`USBHost.task()`** so the stack stays serviced after `setup()`; all tests run in `setup()` like **SD_Test**.

**Debug:** With **Tools → Core Debug Level → Verbose**, the library emits **`log_v`** / **`log_buf_v`** traces for **`[USBMSCFS]`** disk I/O and **`[USBHostMSC]`** SCSI submit/wait/timeout/CSW errors.
