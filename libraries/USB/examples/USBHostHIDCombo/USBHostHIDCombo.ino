/*
 * USB Host HID combo: mouse + keyboard + gamepad
 *
 * Flash once, then plug mice (boot or typical report-protocol), keyboards, and/or gamepads (hub OK).
 * Serial tags: [mouse] [keyboard] [gamepad]. Periodic [status] shows which handlers claimed a device.
 *
 * If gamepad stays "no", try the standalone USBHostGamepad example or set
 * COMBO_DUMP_HID_DESCRIPTOR to 1 to print the report descriptor.
 *
 * Same hardware requirements as USBHostMouse / USBHostKeyboard / USBHostGamepad
 * (ESP32-S2 / S3 / P4 with USB OTG host).
 */

#include <Arduino.h>
#include <USBHost.h>
#include <USBHostHID.h>
#include <USBHostHIDReportMapDump.h>
#include <USBHostHIDMouse.h>
#include <USBHostHIDKeyboard.h>
#include <USBHostHIDGamepad.h>

/** 1 = log usbhid_parse_report_map() for every HID interface (verbose when using a hub). */
#ifndef COMBO_DUMP_HID_DESCRIPTOR
#define COMBO_DUMP_HID_DESCRIPTOR 0
#endif

#if CFG_TUH_HID && COMBO_DUMP_HID_DESCRIPTOR
static USBHostHIDReportMapDumper s_hidReportMapDumper(&Serial);
#endif

#ifndef MOUSE_PRINT_ON_ACTIVITY_ONLY
#define MOUSE_PRINT_ON_ACTIVITY_ONLY 1
#endif

#ifndef KEYBOARD_NOTIFY_ON_CHANGE_ONLY
#define KEYBOARD_NOTIFY_ON_CHANGE_ONLY 1
#endif

#ifndef KEYBOARD_LOG_RELEASES
#define KEYBOARD_LOG_RELEASES 0
#endif

#ifndef GAMEPAD_NOTIFY_ON_CHANGE_ONLY
#define GAMEPAD_NOTIFY_ON_CHANGE_ONLY 1
#endif

#ifndef GAMEPAD_TRY_STICK8
#define GAMEPAD_TRY_STICK8 1
#endif

/** 0 = disable periodic [status] mount lines. */
#ifndef COMBO_STATUS_INTERVAL_MS
#define COMBO_STATUS_INTERVAL_MS 3000
#endif

#if MOUSE_PRINT_ON_ACTIVITY_ONLY
static uint8_t s_last_mouse_buttons = 0;
#endif

#if COMBO_STATUS_INTERVAL_MS > 0
static uint32_t s_last_status_ms;
#endif

static void onMouseReport(int16_t x, int16_t y, uint8_t buttons, int8_t wheel, void *) {
#if MOUSE_PRINT_ON_ACTIVITY_ONLY
  if (x == 0 && y == 0 && wheel == 0 && buttons == s_last_mouse_buttons) {
    USBHostMouse.clear();
    return;
  }
  s_last_mouse_buttons = buttons;
#endif
  Serial.printf("[mouse] dx=%d dy=%d btns=0x%02x wheel=%d\n", (int)x, (int)y, (unsigned)buttons, (int)wheel);
  USBHostMouse.clear();
}

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
  USBHostKeyboard.clear();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("USB Host HID combo: mouse + keyboard + gamepad (plug any subset; hub OK)."));

#if CFG_TUH_HID && COMBO_DUMP_HID_DESCRIPTOR
  USBHostHID.addDevice(&s_hidReportMapDumper);
#endif

  /* Register gamepad before mouse so overlapping report-protocol HID interfaces prefer the pad. */
  USBHostGamepad.registerWithHost();
  USBHostGamepad.setNotifyOnChangeOnly(GAMEPAD_NOTIFY_ON_CHANGE_ONLY != 0);
  USBHostMouse.registerWithHost();
  USBHostMouse.setReportCallback(onMouseReport, nullptr);
  USBHostKeyboard.registerWithHost();
  USBHostKeyboard.setNotifyOnChangeOnly(KEYBOARD_NOTIFY_ON_CHANGE_ONLY != 0);
  USBHostKeyboard.setReportCallback(onKeyboardReport, nullptr);

  if (!USBHost.begin()) {
    Serial.println("USBHost.begin() failed");
    return;
  }
  Serial.println(F("Host ready. Swap devices without reflashing."));
}

void loop() {
  USBHost.task(); /* HID IN arming runs on usbhTuh worker after tuh_task() */

  if (USBHostGamepad.mounted() && USBHostGamepad.available()) {
    uint16_t n = USBHostGamepad.reportLength();
    Serial.printf("[gamepad] len=%u hex:", (unsigned)n);
    for (uint16_t i = 0; i < n && i < 24; i++) {
      Serial.printf(" %02x", (unsigned)USBHostGamepad.reportData()[i]);
    }
    if (n > 24) {
      Serial.print(" ...");
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

#if COMBO_STATUS_INTERVAL_MS > 0
  {
    const uint32_t now = millis();
    if ((uint32_t)(now - s_last_status_ms) >= (uint32_t)COMBO_STATUS_INTERVAL_MS) {
      s_last_status_ms = now;
      Serial.printf("[status] mouse=%s keyboard=%s gamepad=%s\n",
                    USBHostMouse.mounted() ? "yes" : "no", USBHostKeyboard.mounted() ? "yes" : "no",
                    USBHostGamepad.mounted() ? "yes" : "no");
    }
  }
#endif

  delay(2);
}
