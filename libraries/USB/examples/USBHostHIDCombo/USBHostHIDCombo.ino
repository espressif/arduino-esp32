/*
 * USB Host HID combo: mouse + keyboard + gamepad
 *
 * Flash once, then plug mice (boot or typical report-protocol), keyboards, and/or gamepads (hub OK).
 * Serial tags: [mouse] [keyboard] [gamepad]. Periodic [status] shows which handlers claimed a device.
 *
 * If gamepad stays "no", the descriptor may not match USBHostHIDGamepad heuristics — try
 * the standalone USBHostGamepad example or enable **Core Debug Level → Verbose** for `log_v` / `log_buf_v` traces.
 *
 * Same hardware / USB Mode requirements as USBHostMouse / USBHostKeyboard / USBHostGamepad.
 */

#include <Arduino.h>

#if !SOC_USB_OTG_SUPPORTED
#error This SoC has no USB OTG use ESP32-S2 / S3 / P4 with host support.
#elif (ARDUINO_USB_MODE != 1) && !defined(ARDUINO_ESP32_S3_USB_OTG)
#warning USB host: set USB Mode (e.g. Hardware CDC) OR use board ESP32-S3-USB-OTG.
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("USB host: Tools -> USB Mode -> Hardware CDC, or board ESP32-S3-USB-OTG."));
}
void loop() {}
#else

#include <USBHost.h>
#include <USBHostHID.h>
#include <USBHostHIDReportMapDump.h>
#include <USBHostHIDMouse.h>
#include <USBHostHIDKeyboard.h>
#include <USBHostHIDKeyboardDecode.h>
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
  usbHostHidBootReportAppendAscii(ascii, sizeof(ascii), modifiers, keys, KeyboardLayout_en_US);

  Serial.printf("[keyboard] mod=0x%02x", (unsigned)modifiers);

  if (ascii[0] != '\0') {
    Serial.printf("  %s", ascii);
  } else if (any_key) {
    for (int i = 0; i < 6; i++) {
      if (keys[i] == 0) {
        continue;
      }
      uint8_t vk = usbHostHidKeyboardUsageToArduinoVirtualKey(keys[i]);
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
  Serial.println(F("USB Host HID combo: mouse + keyboard + gamepad (plug any subset; hub OK)."));

#if CFG_TUH_HID && COMBO_DUMP_HID_DESCRIPTOR
  USBHostHID.addDevice(&s_hidReportMapDumper);
#endif

  /* Register gamepad before mouse so overlapping report-protocol HID interfaces prefer the pad. */
  USBHostGamepad.registerWithHost();
  USBHostGamepad.setNotifyOnChangeOnly(GAMEPAD_NOTIFY_ON_CHANGE_ONLY != 0);
  USBHostMouse.registerWithHost();
  USBHostKeyboard.registerWithHost();
  USBHostKeyboard.setNotifyOnChangeOnly(KEYBOARD_NOTIFY_ON_CHANGE_ONLY != 0);
  USBHostKeyboard.setReportCallback(onKeyboardReport, nullptr);

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
  Serial.println(F("Host ready. Swap devices without reflashing."));
}

void loop() {
  USBHost.task();

  /* available() runs startReceiveIfPending() and returns true when a report is waiting. */
  const bool mouse_ready = USBHostMouse.available();
  const bool pad_ready = USBHostGamepad.available();

  if (USBHostMouse.mounted() && mouse_ready) {
    int8_t dx = USBHostMouse.getX();
    int8_t dy = USBHostMouse.getY();
    uint8_t btns = USBHostMouse.getButtons();
    int8_t wh = USBHostMouse.getWheel();

#if MOUSE_PRINT_ON_ACTIVITY_ONLY
    if (dx == 0 && dy == 0 && wh == 0 && btns == s_last_mouse_buttons) {
      USBHostMouse.clear();
    } else {
      s_last_mouse_buttons = btns;
      Serial.printf("[mouse] dx=%d dy=%d btns=0x%02x wheel=%d\n", (int)dx, (int)dy, (unsigned)btns, (int)wh);
      USBHostMouse.clear();
    }
#else
    Serial.printf("[mouse] dx=%d dy=%d btns=0x%02x wheel=%d\n", (int)dx, (int)dy, (unsigned)btns, (int)wh);
    USBHostMouse.clear();
#endif
  }

  if (USBHostGamepad.mounted() && pad_ready) {
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
#endif /* host build allowed */
