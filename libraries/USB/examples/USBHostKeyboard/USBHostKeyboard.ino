/*
 * USB Host Keyboard Example
 *
 * Boot-protocol USB HID keyboard (ESP32-S2 / S3 / P4, USB host).
 * Register before USBHost.begin(); on ESP32-S3-USB-OTG, begin() enables VBUS.
 *
 * printReport() logs LEFT_CTRL+KEY_F1. The same names work in if ():
 *   if ((modifiers & LEFT_CTRL) && USBHostKeyboard.toVirtualKey(keys[0]) == KEY_F1)
 */

#include <Arduino.h>
#include <USBHost.h>
#include <USBHostHIDKeyboard.h>

#ifndef KEYBOARD_NOTIFY_ON_CHANGE_ONLY
#define KEYBOARD_NOTIFY_ON_CHANGE_ONLY 1
#endif

/** 1 = also log all-key-up reports (noisy). */
#ifndef KEYBOARD_LOG_RELEASES
#define KEYBOARD_LOG_RELEASES 0
#endif

static void onKeyboardReport(uint8_t modifiers, const uint8_t keys[6], void *) {
  bool any_key = (modifiers != 0);
  for (int i = 0; i < 6; i++) {
    if (keys[i] != 0) {
      any_key = true;
      break;
    }
  }

#if !KEYBOARD_LOG_RELEASES
  if (!any_key) {
    USBHostKeyboard.clear();
    return;
  }
#endif

  Serial.print(F("[keyboard] "));
  USBHostKeyboard.printReport(Serial, modifiers, keys);
  Serial.println();

  if ((modifiers & LEFT_CTRL) && USBHostKeyboard.toVirtualKey(keys[0]) == KEY_F1) {
    Serial.println(F("[app] Ctrl+F1"));
  }

  USBHostKeyboard.clear();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("USB Host Keyboard example");

  USBHostKeyboard.registerWithHost();
  USBHostKeyboard.setNotifyOnChangeOnly(KEYBOARD_NOTIFY_ON_CHANGE_ONLY != 0);
  USBHostKeyboard.setReportCallback(onKeyboardReport, nullptr);

  if (!USBHost.begin()) {
    Serial.println("USBHost.begin() failed");
    return;
  }
  Serial.println(F("Host ready. Plug a keyboard (hub OK if direct attach fails)."));
}

void loop() {
  USBHost.task();
  delay(2);
}
