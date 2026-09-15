/*
 * USB Host Mouse Example
 *
 * Boot- or report-protocol USB HID mouse (ESP32-S2 / S3 / P4, USB host).
 * Register before USBHost.begin(); on ESP32-S3-USB-OTG, begin() enables VBUS.
 */

#include <Arduino.h>
#include <USBHost.h>
#include <USBHostHIDMouse.h>

/** 1 = print only when movement, wheel, or buttons change (fewer idle lines). */
#ifndef MOUSE_PRINT_ON_ACTIVITY_ONLY
#define MOUSE_PRINT_ON_ACTIVITY_ONLY 1
#endif

#if MOUSE_PRINT_ON_ACTIVITY_ONLY
static uint8_t s_last_buttons = 0;
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("USB Host Mouse example");

  USBHostMouse.registerWithHost();

  if (!USBHost.begin()) {
    Serial.println("USBHost.begin() failed");
    return;
  }
  Serial.println(F("Host ready. Plug a mouse (hub OK if direct attach fails)."));
}

void loop() {
  USBHost.task();

  if (USBHostMouse.mounted() && USBHostMouse.available()) {
    int16_t dx = USBHostMouse.getX();
    int16_t dy = USBHostMouse.getY();
    uint8_t btns = USBHostMouse.getButtons();
    int8_t wh = USBHostMouse.getWheel();

#if MOUSE_PRINT_ON_ACTIVITY_ONLY
    if (dx == 0 && dy == 0 && wh == 0 && btns == s_last_buttons) {
      USBHostMouse.clear();
    } else {
      s_last_buttons = btns;
      Serial.printf("dx=%d dy=%d btns=0x%02x wheel=%d\n",
                    (int)dx, (int)dy, (unsigned)btns, (int)wh);
      USBHostMouse.clear();
    }
#else
    Serial.printf("dx=%d dy=%d btns=0x%02x wheel=%d\n",
                  (int)dx, (int)dy, (unsigned)btns, (int)wh);
    USBHostMouse.clear();
#endif
  }

  delay(1);
}
