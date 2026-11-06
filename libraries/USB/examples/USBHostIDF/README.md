# ESP-IDF USB Host

Enumerates USB devices with the **ESP-IDF USB Host Library** (`usb/usb_host.h`) and prints their device descriptors. Use this as a starting point when a class driver you need exists for ESP-IDF (`usb_host_uvc`, `cdc_acm_host`, `iot_usbh_*`, …) but not for TinyUSB.

This is a different stack from the rest of the USB host examples here, which use TinyUSB through [`USBHost`](../USBHostKeyboard/README.md).

## An enumeration filter callback is required

Since core **3.3.11** the prebuilt libraries are built with `CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK` enabled, so **`usb_host_config_t::enum_filter_cb` has to be set**. ESP-IDF treats a `NULL` callback as *"do not enumerate this device"* and cancels enumeration for every device — no error is logged, no `USB_HOST_CLIENT_EVENT_NEW_DEV` arrives, and the bus simply looks dead. The header comment "Set to NULL otherwise" only applies when the option is disabled, which is the ESP-IDF default and was also the case up to core 3.3.10.

An allow-all callback is enough if you do not need to filter:

```cpp
static bool enumFilter(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue) {
  *bConfigurationValue = 1;
  return true;
}

host_config.enum_filter_cb = enumFilter;
```

The callback is also the only place to pick a configuration other than the first one, which some devices need — a CH397A Ethernet adapter exposes ECM on configuration 2, for instance. Return `false` to leave a device unenumerated. It must not block and must not submit transfers.

See [espressif/arduino-esp32#12778](https://github.com/espressif/arduino-esp32/issues/12778).

## Setup

1. **Target:** ESP32-S3, ESP32-P4 or ESP32-S31. The ESP-IDF USB Host Library is **not** part of the ESP32-S2 prebuilt libraries.
2. **Console:** set `Tools > USB CDC On Boot > Disabled` and watch the board's UART port. The host needs the native USB port, so `Serial` must not live there: with CDC on boot enabled it is the native port, and everything printed to it is lost as soon as the host stack takes over the PHY. ESP-IDF logging (`log_v()` and friends) always goes to UART0, so seeing only those in the monitor means `Serial` is on the wrong port.
   On the **ESP32-S3-USB-OTG** there is no separate menu for this - it follows `USB Mode`: `USB-OTG` leaves CDC on boot off (`Serial` is UART0, on the UART connector), while `Hardware CDC and JTAG` turns it on and moves `Serial` to the native port.
3. **Do not call `USBHost.begin()`** here: TinyUSB and the ESP-IDF host library cannot share the peripheral. With CDC, MSC and DFU on boot all disabled the core never starts TinyUSB on its own.
4. **VBUS:** the sketch calls `USBHostBoardInit()`, which is an empty weak function on most boards and switches the mux and VBUS on the ESP32-S3-USB-OTG. Other boards have to power the port themselves.
5. On ESP32-P4 the library defaults to the high-speed peripheral; set `host_config.peripheral_map = BIT1` to use the full-speed one instead.

## Output

```
ESP-IDF USB Host example
Host ready. Plug in a USB device.
[usbh] device 1: 058F:6387, full speed, configuration 1
*** Device descriptor ***
bLength 18
...
[usbh] device removed
```

Devices stay open until they are unplugged, because a removal event is only delivered for a device the client still holds open. Up to `MAX_OPEN_DEVICES` are tracked, which is enough for a small hub.
