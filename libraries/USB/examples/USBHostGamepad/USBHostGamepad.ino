/*
 * USB Host Gamepad / Joystick Example
 *
 * Connect a USB HID gamepad or joystick (ESP32-S2 / S3 / P4 in USB host mode).
 * Unlike boot-protocol mice, gamepads usually use "report protocol" with a
 * Game Pad or Joystick usage in the HID report descriptor — use this sketch
 * when a mouse example does not see your device.
 *
 * Report layout varies by device. By default only **changed** reports are
 * printed (callback + optional poll). Set GAMEPAD_NOTIFY_ON_CHANGE_ONLY 0
 * for every report. Comment out setReportCallback to use poll-only.
 */

#include <Arduino.h>
#include <USBHost.h>
#include <USBHostHID.h>
#include <USBHostHIDReportMapDump.h>
#include <USBHostHIDGamepad.h>

#if CFG_TUH_HID
static USBHostHIDReportMapDumper s_hidReportMapDumper(&Serial);
#endif

/** Set to 1 to print heuristic sticks when polling (see loop). */
#ifndef GAMEPAD_TRY_STICK8
#define GAMEPAD_TRY_STICK8 1
#endif
/**
 * 1 = only notify when report bytes change (callback + available); no spam from repeated identical HID reports.
 * 0 = every USB report updates state (noisy if the pad re-sends the same report often).
 */
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
#if CFG_TUH_HID
  USBHostHID.addDevice(&s_hidReportMapDumper);
#endif
  USBHostGamepad.registerWithHost();
  USBHostGamepad.setNotifyOnChangeOnly(GAMEPAD_NOTIFY_ON_CHANGE_ONLY != 0);
  USBHostGamepad.setReportCallback(onGamepadChanged, nullptr);

  /* VBUS + host mux: USBHost.begin() on ESP32-S3-USB-OTG; sketch can repeat for timing */
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
  Serial.println("USB Host started. Some pads need a USB hub on ESP32-S3-OTG; plug after boot or replug.");
}

void loop() {
  USBHost.task();

  /* Polling path: with notify-on-change, available() is true only when report changed. */
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
