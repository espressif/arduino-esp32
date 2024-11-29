/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Keyboard_pt_BR.h
 * Portuguese Brazilian keyboard layout.
*/

#ifndef KEYBOARD_PT_BR_h
#define KEYBOARD_PT_BR_h

//================================================================================
//================================================================================
//  Keyboard

// pt_BR keys
#define KEY_C_CEDILLA (0x88 + 0x33)
#define KEY_ACUTE     (0x88 + 0x2f)
// use the pressRaw() to press the modification key and then press the key you want to modify
#define KEY_MASCULINE_ORDINAL (0x88 + 0x32)  // first pressRaw(HID_KEY_ALT_RIGHT), then press(KEY_MASCULINE_ORDINAL)
#define KEY_FEMININE_ORDINAL  (0x88 + 0x30)  // first pressRaw(HID_KEY_ALT_RIGHT), then press(KEY_FEMININE_ORDINAL)
#define KEY_PARAGRAPH         (0x88 + 0x2e)  // first pressRaw(HID_KEY_ALT_RIGHT), then press(KEY_PARAGRAPH)
#define KEY_UMLAUT            (0x88 + 0x23)  // first pressRaw(HID_KEY_SHIFT_RIGHT), then press(KEY_UMLAUT)

#endif
