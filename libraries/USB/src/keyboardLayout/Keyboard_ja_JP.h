/*
  Keyboard_ja_JP.h
*/

#ifndef KEYBOARD_JA_JP_h
#define KEYBOARD_JA_JP_h

//================================================================================
//================================================================================
//  Keyboard

// ja_JP key - 半角/全角 (Hankaku/Zenkaku) IME toggle key.
// Same physical position as the grave/tilde key on US keyboards.
#define KEY_HANKAKU_ZENKAKU (0x88 + 0x35)

// The following JIS-specific keys have USB HID scan codes in the International
// range (0x85-0x89) and cannot be used with press() / release(). Use
// pressRaw() / releaseRaw() with the raw HID scan code instead.
//
//   ろ (ro) key        pressRaw(0x85)  // HID International1
//   カタカナ/ひらがな   pressRaw(0x86)  // HID International2
//   ¥ (yen) key        pressRaw(0x87)  // HID International3
//   変換 (henkan)      pressRaw(0x88)  // HID International4
//   無変換 (muhenkan)  pressRaw(0x89)  // HID International5

#endif
