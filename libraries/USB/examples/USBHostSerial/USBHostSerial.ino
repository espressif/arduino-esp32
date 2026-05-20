/*
 * USB Host serial (CDC ACM / CP210x / CH34x / FTDI via TinyUSB host CDC driver).
 *
 * Same host setup as other USBHost examples: USB Mode for host (e.g. Hardware CDC),
 * USBHost.begin(), USBHost.task() in loop.
 *
 * USBHostSerial follows the same Stream-style API as device-side USBCDC (begin/read/write/flush).
 *
 * Reference: TinyUSB examples/host/cdc_msc_hid/src/cdc_app.c
 */

#include <Arduino.h>

#if !SOC_USB_OTG_SUPPORTED
#error USB host requires ESP32-S2 / S3 / P4 with OTG.
#elif (ARDUINO_USB_MODE != 1) && !defined(ARDUINO_ESP32_S3_USB_OTG)
#warning Set Tools -> USB Mode for host (e.g. Hardware CDC) or use ESP32-S3-USB-OTG.
void setup() {
  Serial.begin(115200);
}
void loop() {}
#else

#include <USBHost.h>
#include <USBHostSerial.h>

#ifndef HOST_SERIAL_BAUD
#define HOST_SERIAL_BAUD 115200
#endif

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println(F("USB host serial: plug a USB CDC/serial adapter (or cable device)."));

#if defined(USB_HOST_EN) && defined(DEV_VBUS_EN)
  usbHostEnable(true);
  delay(10);
  usbHostPower(USB_HOST_POWER_VBUS);
  delay(10);
#endif

  if (!USBHost.begin()) {
    Serial.println(F("USBHost.begin() failed"));
    return;
  }

  /* Like USBCDC: pending baud + DTR/RTS are applied when the CDC interface mounts. */
  USBHostSerial.begin(HOST_SERIAL_BAUD);
}

void loop() {
  USBHost.task();

  static bool s_announced = false;
  if (USBHostSerial) {
    if (!s_announced) {
      s_announced = true;
      Serial.printf("[host] CDC ready, baudRate()=%lu — echoing USB <-> Serial\n", (unsigned long)USBHostSerial.baudRate());
    }

    uint8_t buf[64];
    const size_t n = USBHostSerial.read(buf, sizeof(buf));
    if (n > 0) {
      Serial.write(buf, n);
    }
    while (Serial.available()) {
      const int c = Serial.read();
      if (c < 0) {
        break;
      }
      USBHostSerial.write((uint8_t)c);
    }
    USBHostSerial.flush();
  } else {
    s_announced = false;
  }

  delay(1);
}

#endif
