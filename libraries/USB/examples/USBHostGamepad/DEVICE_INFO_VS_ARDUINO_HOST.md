# Why TinyUSB `device_info` worked but Arduino `USBHost` did not

## 1. **VBUS and USB mux (most common)**

The **device_info** sketch drives the **ESP32-S3-USB-OTG** power path **before** `tusb_init()`:

| GPIO | Role |
|------|------|
| 18   | Host vs device mux (`HIGH` = host port) |
| 12   | VBUS to the USB-A/device connector |
| 17   | Current limiter enable |

If those are not asserted, the gamepad gets **no power** (LED off) and nothing enumerates.

Arduino **`USBHost.begin()`** now performs the same sequence **automatically** when the board profile defines **`USB_HOST_EN`** / **`DEV_VBUS_EN`** (i.e. **Board: ESP32-S3-USB-OTG**). If you use **“ESP32-S3 Dev Module”**, those macros are missing — the core cannot know your wiring; you must drive the same GPIOs yourself (copy the `setup()` block from device_info) or select the correct board.

## 2. **`usb_new_phy()` vs `tinyusb_host_init()`**

device_info calls **`usb_new_phy(..., USB_OTG_MODE_HOST)`** then **`tusb_init(..., TUSB_ROLE_HOST)`**.

Arduino **`USBHost.begin()`** calls **`tinyusb_host_init()`**, which uses **`init_usb_hal_host()`** then **`tusb_init()`** — same role, equivalent PHY bring-up for internal PHY.

## 3. **USB Mode / `ARDUINO_USB_MODE`**

Default menu on ESP32-S3-USB-OTG is **“USB-OTG”** → **`ARDUINO_USB_MODE=0`**. Older host examples required **`ARDUINO_USB_MODE==1`**, so the host code path was **not compiled** (empty `setup`/`loop`) unless you chose **“Hardware CDC and JTAG”**.

Host examples now also allow **`ARDUINO_ESP32_S3_USB_OTG`** so the default OTG menu still builds the host sketch.

## Checklist

1. **Board:** **ESP32-S3-USB-OTG** (or replicate GPIO 12/17/18 in code).
2. **USB Mode:** Default OTG is OK for the updated examples; other S3 boards still need host-capable USB mode where applicable.
3. **Plug device into the OTG host port** (not the USB-C used for flashing, unless that port is wired as host on your board).
