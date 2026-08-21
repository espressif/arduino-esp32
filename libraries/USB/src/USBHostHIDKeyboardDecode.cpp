/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "USBHostHIDKeyboardDecode.h"

#include <string.h>

#include "keyboardLayout/KeyboardLayout.h"

/* Same encoding as KeyboardLayout.h (not all boards expose this header publicly). */
#ifndef SHIFT
#define SHIFT 0x80
#endif
#ifndef ALT_GR
#define ALT_GR 0x40
#endif

uint8_t usbHostHidKeyboardUsageToArduinoVirtualKey(uint8_t hid_usage) {
  /* HID usage = USBHIDKeyboard KEY - 0x88 for KEY >= 0x88 (see USBHIDKeyboard::press). */
  switch (hid_usage) {
    case 0x29:
      return 0xB1; /* KEY_ESC */
    case 0x2a:
      return 0xB2; /* KEY_BACKSPACE */
    case 0x2b:
      return 0xB3; /* KEY_TAB */
    case 0x28:
      return 0xB0; /* KEY_RETURN */
    case 0x39:
      return 0xC1; /* KEY_CAPS_LOCK */
    case 0x3a:
      return 0xC2; /* KEY_F1 */
    case 0x3b:
      return 0xC3;
    case 0x3c:
      return 0xC4;
    case 0x3d:
      return 0xC5;
    case 0x3e:
      return 0xC6;
    case 0x3f:
      return 0xC7;
    case 0x40:
      return 0xC8;
    case 0x41:
      return 0xC9;
    case 0x42:
      return 0xCA;
    case 0x43:
      return 0xCB;
    case 0x44:
      return 0xCC;
    case 0x45:
      return 0xCD;
    case 0x68:
      return 0xF0; /* KEY_F13 */
    case 0x69:
      return 0xF1;
    case 0x6a:
      return 0xF2;
    case 0x6b:
      return 0xF3;
    case 0x6c:
      return 0xF4;
    case 0x6d:
      return 0xF5;
    case 0x6e:
      return 0xF6;
    case 0x6f:
      return 0xF7;
    case 0x70:
      return 0xF8;
    case 0x71:
      return 0xF9;
    case 0x72:
      return 0xFA;
    case 0x73:
      return 0xFB; /* KEY_F24 */
    case 0x46:
      return 0xCE; /* KEY_PRINT_SCREEN */
    case 0x47:
      return 0xCF; /* KEY_SCROLL_LOCK */
    case 0x48:
      return 0xD0; /* KEY_PAUSE */
    case 0x49:
      return 0xD1; /* KEY_INSERT */
    case 0x4a:
      return 0xD2; /* KEY_HOME */
    case 0x4b:
      return 0xD3; /* KEY_PAGE_UP */
    case 0x4c:
      return 0xD4; /* KEY_DELETE */
    case 0x4d:
      return 0xD5; /* KEY_END */
    case 0x4e:
      return 0xD6; /* KEY_PAGE_DOWN */
    case 0x4f:
      return 0xD7; /* KEY_RIGHT_ARROW */
    case 0x50:
      return 0xD8; /* KEY_LEFT_ARROW */
    case 0x51:
      return 0xD9; /* KEY_DOWN_ARROW */
    case 0x52:
      return 0xDA; /* KEY_UP_ARROW */
    case 0x53:
      return 0xDB; /* KEY_NUM_LOCK */
    case 0x54:
      return 0xDC; /* KEY_KP_SLASH */
    case 0x55:
      return 0xDD; /* KEY_KP_ASTERISK */
    case 0x56:
      return 0xDE; /* KEY_KP_MINUS */
    case 0x57:
      return 0xDF; /* KEY_KP_PLUS */
    case 0x58:
      return 0xE0; /* KEY_KP_ENTER */
    case 0x59:
      return 0xE1; /* KEY_KP_1 */
    case 0x5a:
      return 0xE2;
    case 0x5b:
      return 0xE3;
    case 0x5c:
      return 0xE4;
    case 0x5d:
      return 0xE5;
    case 0x5e:
      return 0xE6;
    case 0x5f:
      return 0xE7;
    case 0x60:
      return 0xE8;
    case 0x61:
      return 0xE9;
    case 0x62:
      return 0xEA;
    case 0x63:
      return 0xEB; /* KEY_KP_DOT */
    case 0x65:
      return 0xED; /* KEY_MENU */
    default:
      return 0;
  }
}

char usbHostHidBootReportUsageToAscii(uint8_t modifiers, const uint8_t hid_usage, const uint8_t *layout) {
  if (layout == nullptr || hid_usage == 0) {
    return 0;
  }

  const bool shift = (modifiers & (uint8_t)(0x02u | 0x20u)) != 0;
  const bool altgr = (modifiers & 0x40u) != 0;

  for (unsigned ascii = 32; ascii < 128; ascii++) {
    uint8_t e = layout[ascii];
    if (e == 0) {
      continue;
    }
    const bool need_shift = (e & SHIFT) != 0;
    const bool need_altgr = (e & ALT_GR) != 0;
    uint8_t base = (uint8_t)(e & (uint8_t) ~(SHIFT | ALT_GR));
    if (base != hid_usage) {
      continue;
    }
    if (need_shift != shift || need_altgr != altgr) {
      continue;
    }
    return (char)ascii;
  }
  return 0;
}

size_t usbHostHidBootReportAppendAscii(char *buf, size_t buf_cap, uint8_t modifiers, const uint8_t keys[6],
                                       const uint8_t *layout) {
  if (buf == nullptr || buf_cap == 0 || keys == nullptr) {
    return 0;
  }

  size_t w = 0;
  buf[0] = '\0';

  for (int i = 0; i < 6; i++) {
    const uint8_t u = keys[i];
    if (u == 0) {
      continue;
    }
    const char c = usbHostHidBootReportUsageToAscii(modifiers, u, layout);
    if (c == 0) {
      continue;
    }
    if (w + 1 >= buf_cap) {
      break;
    }
    buf[w++] = c;
    buf[w] = '\0';
  }
  return w;
}
