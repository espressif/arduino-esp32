/*
 * USB Host serial (CDC ACM / CP210x / CH34x / FTDI via TinyUSB host CDC driver).
 *
 * Same host setup as other USBHost examples: USBHost.begin(), USBHost.task() in loop.
 *
 * USBHostSerial follows the same Stream-style API as device-side USBCDC (begin/read/write/flush).
 *
 * Reference: TinyUSB examples/host/cdc_msc_hid/src/cdc_app.c
 */

#include <Arduino.h>
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
