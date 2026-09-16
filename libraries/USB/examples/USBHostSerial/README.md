# USB Host Serial

Talks to a USB serial adapter (CDC ACM, and the CP210x / CH34x / FTDI drivers TinyUSB provides) from an ESP32-S2 / S3 / P4 in host mode, and bridges it to the Serial Monitor.

`USBHostSerialClass` is a `Stream`, so it has the same `begin` / `available` / `read` / `write` / `flush` API as the device-side `USBCDC`.

## Flash / run checklist

1. **Board / USB mode:** see the [USBHostMouse checklist](../USBHostMouse/README.md) for the **USB Mode** / **USB CDC On Boot** combination to avoid.
2. **Declare the instance in the sketch:** `USBHostSerialClass USBHostSerial;`. The library ships no instance, so a sketch only pays for the classes it names.
3. **`USBHost.begin()`**, then **`USBHostSerial.begin(baud)`**. Calling `begin()` before anything is plugged in is fine — the baud rate and DTR/RTS are applied once the CDC interface mounts, exactly like `USBCDC`.
4. `loop()` must keep calling **`USBHost.task()`**.

Use `if (USBHostSerial)` (or `mounted()`) to test whether a device is attached; the sketch uses it to print the negotiated `baudRate()` once per connection.

## Only one CDC device at a time

The prebuilt libraries are built with `CFG_TUH_CDC == 1`, so TinyUSB tracks a single CDC interface and only one `USBHostSerialClass` instance can receive its events. Declaring a second one logs an error and leaves it unbound. A composite device that exposes several CDC interfaces will only have its first one claimed.

## Sending data

`write()` is bounded by `setTxTimeoutMs()` (default 250 ms), which limits how long it waits for the device to drain a full FIFO. Set it to `0` for a non-blocking write that returns as soon as the FIFO is full, and check the return value for how much was actually accepted.
