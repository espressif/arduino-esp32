/*
 * USB Host Gamepad / Joystick Example
 *
 * Report-protocol gamepad/joystick (ESP32-S2 / S3 / P4, USB host).
 * Register before USBHost.begin(); on ESP32-S3-USB-OTG, begin() enables VBUS.
 *
 * Report layout varies by device. By default only changed reports are printed.
 * Set GAMEPAD_NOTIFY_ON_CHANGE_ONLY 0 for every report.
 */

#include <Arduino.h>
#include <USBHost.h>
#include <USBHostHIDGamepad.h>

/** Set to 1 to print heuristic sticks when polling (see loop). */
#ifndef GAMEPAD_TRY_STICK8
#define GAMEPAD_TRY_STICK8 1
#endif

#ifndef GAMEPAD_NOTIFY_ON_CHANGE_ONLY
#define GAMEPAD_NOTIFY_ON_CHANGE_ONLY 1
#endif

static void onGamepadChanged(const uint8_t *r, uint16_t len, void *) {
  Serial.printf("[gamepad] changed len=%u:", (unsigned)len);
  for (uint16_t i = 0; i < len && i < 24; i++) {
    Serial.printf(" %02x", (unsigned)r[i]);
  }
  if (len > 24) {
    Serial.print(" ...");
  }
  Serial.println();
  USBHostGamepad.clear();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("USB Host Gamepad example");

  USBHostGamepad.registerWithHost();
  USBHostGamepad.setNotifyOnChangeOnly(GAMEPAD_NOTIFY_ON_CHANGE_ONLY != 0);
  USBHostGamepad.setReportCallback(onGamepadChanged, nullptr);

#if defined(USB_HOST_EN) && defined(DEV_VBUS_EN)
  usbHostEnable(true);
  delay(10);
  usbHostPower(USB_HOST_POWER_VBUS);
  delay(10);
#endif

  if (!USBHost.begin()) {
    Serial.println("USBHost.begin() failed");
    return;
  }
  Serial.println("USB Host started. Some pads need a USB hub on ESP32-S3; plug after boot or replug.");
}

void loop() {
  USBHost.task();

  if (USBHostGamepad.mounted() && USBHostGamepad.available()) {
    uint16_t n = USBHostGamepad.reportLength();
    Serial.printf("[poll] len=%u hex:", (unsigned)n);
    for (uint16_t i = 0; i < n && i < 24; i++) {
      Serial.printf(" %02x", (unsigned)USBHostGamepad.reportData()[i]);
    }
    Serial.printf(" | btn16=0x%04x", (unsigned)USBHostGamepad.getButtons16());
#if GAMEPAD_TRY_STICK8
    int8_t lx, ly, rx, ry;
    USBHostGamepad.getSticks8(&lx, &ly, &rx, &ry, false);
    Serial.printf(" | L(%d,%d) R(%d,%d)", (int)lx, (int)ly, (int)rx, (int)ry);
#endif
    Serial.println();
    USBHostGamepad.clear();
  }

  delay(2);
}
