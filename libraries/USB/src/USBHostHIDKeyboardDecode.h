/*
 * Host-side inverse of USBHIDKeyboard: boot-protocol HID usages + layout -> Arduino KEY_* / ASCII.
 *
 * On the device, USBHIDKeyboard::press() maps ASCII and KEY_* (>= 0x88) to HID usages; modifiers
 * in KeyReport.modifiers are the same bits on the wire in boot protocol. On the host, use these
 * helpers to recover the same logical view.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Boot HID usage → Arduino KEY_* value → identifier (same names as USBHIDKeyboard.h).
 *
 * Sketch:
 *   #define ON_KEY(hid, vk, name) case vk: do_##name(); break;
 *   switch (USBHostKeyboard.toVirtualKey(keys[0])) { USBHOST_VK_MAP(ON_KEY) }
 *   #undef ON_KEY
 */
#define USBHOST_VK_MAP(X)                      \
  X(0x29, 0xB1, KEY_ESC)                       \
  X(0x2a, 0xB2, KEY_BACKSPACE)                 \
  X(0x2b, 0xB3, KEY_TAB)                       \
  X(0x2c, 0x20, KEY_SPACE)                     \
  X(0x28, 0xB0, KEY_RETURN)                    \
  X(0x39, 0xC1, KEY_CAPS_LOCK)                 \
  X(0x3a, 0xC2, KEY_F1)                        \
  X(0x3b, 0xC3, KEY_F2)                        \
  X(0x3c, 0xC4, KEY_F3)                        \
  X(0x3d, 0xC5, KEY_F4)                        \
  X(0x3e, 0xC6, KEY_F5)                        \
  X(0x3f, 0xC7, KEY_F6)                        \
  X(0x40, 0xC8, KEY_F7)                        \
  X(0x41, 0xC9, KEY_F8)                        \
  X(0x42, 0xCA, KEY_F9)                        \
  X(0x43, 0xCB, KEY_F10)                       \
  X(0x44, 0xCC, KEY_F11)                       \
  X(0x45, 0xCD, KEY_F12)                       \
  X(0x68, 0xF0, KEY_F13)                       \
  X(0x69, 0xF1, KEY_F14)                       \
  X(0x6a, 0xF2, KEY_F15)                       \
  X(0x6b, 0xF3, KEY_F16)                       \
  X(0x6c, 0xF4, KEY_F17)                       \
  X(0x6d, 0xF5, KEY_F18)                       \
  X(0x6e, 0xF6, KEY_F19)                       \
  X(0x6f, 0xF7, KEY_F20)                       \
  X(0x70, 0xF8, KEY_F21)                       \
  X(0x71, 0xF9, KEY_F22)                       \
  X(0x72, 0xFA, KEY_F23)                       \
  X(0x73, 0xFB, KEY_F24)                       \
  X(0x46, 0xCE, KEY_PRINT_SCREEN)              \
  X(0x47, 0xCF, KEY_SCROLL_LOCK)               \
  X(0x48, 0xD0, KEY_PAUSE)                     \
  X(0x49, 0xD1, KEY_INSERT)                    \
  X(0x4a, 0xD2, KEY_HOME)                      \
  X(0x4b, 0xD3, KEY_PAGE_UP)                   \
  X(0x4c, 0xD4, KEY_DELETE)                    \
  X(0x4d, 0xD5, KEY_END)                       \
  X(0x4e, 0xD6, KEY_PAGE_DOWN)                 \
  X(0x4f, 0xD7, KEY_RIGHT_ARROW)               \
  X(0x50, 0xD8, KEY_LEFT_ARROW)                \
  X(0x51, 0xD9, KEY_DOWN_ARROW)                \
  X(0x52, 0xDA, KEY_UP_ARROW)                  \
  X(0x53, 0xDB, KEY_NUM_LOCK)                  \
  X(0x54, 0xDC, KEY_KP_SLASH)                  \
  X(0x55, 0xDD, KEY_KP_ASTERISK)               \
  X(0x56, 0xDE, KEY_KP_MINUS)                  \
  X(0x57, 0xDF, KEY_KP_PLUS)                   \
  X(0x58, 0xE0, KEY_KP_ENTER)                  \
  X(0x59, 0xE1, KEY_KP_1)                      \
  X(0x5a, 0xE2, KEY_KP_2)                      \
  X(0x5b, 0xE3, KEY_KP_3)                      \
  X(0x5c, 0xE4, KEY_KP_4)                      \
  X(0x5d, 0xE5, KEY_KP_5)                      \
  X(0x5e, 0xE6, KEY_KP_6)                      \
  X(0x5f, 0xE7, KEY_KP_7)                      \
  X(0x60, 0xE8, KEY_KP_8)                      \
  X(0x61, 0xE9, KEY_KP_9)                      \
  X(0x62, 0xEA, KEY_KP_0)                      \
  X(0x63, 0xEB, KEY_KP_DOT)                    \
  X(0x65, 0xED, KEY_MENU)

/* Host-only sketches: KEY_* from the map. Skip if USBHIDKeyboard.h already defined them. */
#ifndef KEY_ESC
enum {
#define USBHOST_VK_ENUM(hid, vk, name) name = vk,
  USBHOST_VK_MAP(USBHOST_VK_ENUM)
#undef USBHOST_VK_ENUM
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Linked with the core USB library (same table as USBHIDKeyboard::begin default). */
extern const uint8_t KeyboardLayout_en_US[];

/**
 * @brief Map a keyboard-page HID usage from the boot report key array to an Arduino virtual key.
 *
 * Returns the same numeric values as KEY_* in USBHIDKeyboard.h for non-printing keys (>= 0x88),
 * e.g. KEY_F1 (0xC2) for usage 0x3A. Returns 0 if the usage is not one of those specials
 * (e.g. letter/digit usages) — use usbHostHidBootReportUsageToAscii() for those with a layout.
 */
uint8_t usbHostHidKeyboardUsageToArduinoVirtualKey(uint8_t hid_usage);

/** Arduino KEY_* identifier for a virtual-key value from toVirtualKey(), or NULL. */
const char *usbHostHidArduinoVirtualKeyName(uint8_t vk);

/** Log name for HID usages that are not KEY_* (ERR_OVF, NON_US_BS), or NULL. */
const char *usbHostHidBootUsageLogName(uint8_t hid_usage);

/**
 * @brief Map one key usage + modifier byte to a 7-bit ASCII character using the same layout table
 * as USBHIDKeyboard::begin(layout) (e.g. KeyboardLayout_en_US).
 *
 * Only matches when modifier state matches the layout entry (SHIFT / ALT_GR flags). Returns 0 if
 * unknown or ambiguous.
 */
char usbHostHidBootReportUsageToAscii(uint8_t modifiers, uint8_t hid_usage, const uint8_t *layout);

/**
 * @brief Append NUL-terminated ASCII for currently held keys in a boot report (up to 6 slots).
 * Skips usages that do not map through the layout. @p layout same as USBHIDKeyboard.
 * @return number of characters written (excluding terminator); 0 if none.
 */
size_t usbHostHidBootReportAppendAscii(char *buf, size_t buf_cap, uint8_t modifiers, const uint8_t keys[6],
                                       const uint8_t *layout);

#ifdef __cplusplus
}
#endif
