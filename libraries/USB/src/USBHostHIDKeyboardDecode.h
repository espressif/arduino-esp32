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
