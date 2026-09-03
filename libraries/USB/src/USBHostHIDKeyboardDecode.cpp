/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "USBHostHIDKeyboardDecode.h"

#include <stddef.h>
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
#define USBHOST_VK_HID(hid, vk, name)                                                                                  \
  case hid:                                                                                                            \
    return vk;
    USBHOST_VK_MAP(USBHOST_VK_HID)
#undef USBHOST_VK_HID
    default:
      return 0;
  }
}

const char *usbHostHidArduinoVirtualKeyName(uint8_t vk) {
  switch (vk) {
#define USBHOST_VK_NAME(hid, vk, name)                                                                                 \
  case vk:                                                                                                             \
    return #name;
    USBHOST_VK_MAP(USBHOST_VK_NAME)
#undef USBHOST_VK_NAME
    default:
      return NULL;
  }
}

const char *usbHostHidBootUsageLogName(uint8_t hid_usage) {
  switch (hid_usage) {
    case 0x01:
      return "ERR_OVF"; /* Keyboard ErrorRollOver — boot 6KRO overflow */
    case 0x02:
      return "ERR_POST";
    case 0x03:
      return "ERR_UNDEF";
    case 0x32:
      return "NON_US_HASH"; /* HID Keyboard Non-US # and ~ (ISO, next to Enter) */
    case 0x64:
      return "NON_US_BS"; /* HID Keyboard Non-US \ and | */
#define USBHOST_VK_HID_NAME(hid, vk, name)                                                                             \
  case hid:                                                                                                            \
    return #name;
      USBHOST_VK_MAP(USBHOST_VK_HID_NAME)
#undef USBHOST_VK_HID_NAME
    default:
      return NULL;
  }
}

char usbHostHidBootReportUsageToAscii(uint8_t modifiers, const uint8_t hid_usage, const uint8_t *layout) {
  if (layout == nullptr || hid_usage == 0) {
    return 0;
  }

  const bool shift = (modifiers & (uint8_t)(0x02u | 0x20u)) != 0;
  const bool altgr = (modifiers & 0x40u) != 0;

  /* Space is still space with Shift/Ctrl held; the US table only lists unshifted 0x2c. */
  if (hid_usage == 0x2cu && !altgr) {
    return ' ';
  }

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
