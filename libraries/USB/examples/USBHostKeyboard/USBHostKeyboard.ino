/*
 * USB Host Keyboard Example
 *
 * Boot-protocol USB HID keyboard (ESP32-S2 / S3 / P4, USB host).
 * Register before USBHost.begin(); on ESP32-S3-USB-OTG, begin() enables VBUS.
 *
 * Log line shows modifiers + decoded text (US layout) when possible, else HID usage or
 * Arduino virtual key (KEY_*) for specials (arrows, F-keys, …).
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

  char ascii[8];
  USBHostKeyboard.toAscii(ascii, sizeof(ascii), modifiers, keys);

  Serial.printf("[keyboard] mod=0x%02x", (unsigned)modifiers);

  if (ascii[0] != '\0') {
    Serial.printf("  %s", ascii);
  } else if (any_key) {
    for (int i = 0; i < 6; i++) {
      if (keys[i] == 0) {
        continue;
      }
      uint8_t vk = USBHostKeyboard.toVirtualKey(keys[i]);
      if (vk != 0) {
        Serial.printf("  vk=0x%02x", (unsigned)vk);
      } else {
        Serial.printf("  hid=0x%02x", (unsigned)keys[i]);
      }
    }
  } else {
    Serial.print(F("  (released)"));
  }

  Serial.println();
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
