/*
  KeyboardLayout.h

  This file is not part of the public API. It is meant to be included
  only in Keyboard.cpp and the keyboard layout files. Layout files map
  ASCII character codes to keyboard scan codes (technically, to USB HID
  Usage codes), possibly altered by the SHIFT or ALT_GR modifiers.
  Non-ACSII characters (anything outside the 7-bit range NUL..DEL) are
  not supported.

  == Creating your own layout ==

  In order to create your own layout file, copy an existing layout that
  is similar to yours, then modify it to use the correct keys. The
  layout is an array in ASCII order. Each entry contains a scan code,
  possibly modified by "|SHIFT" or "|ALT_GR", as in this excerpt from
  the Italian layout:

      0x35,          // bslash
      0x30|ALT_GR,   // ]
      0x2e|SHIFT,    // ^

  Do not change the control characters (those before scan code 0x2c,
  corresponding to space). Do not attempt to grow the table past DEL. Do
  not use both SHIFT and ALT_GR on the same character: this is not
  supported. Unsupported characters should have 0x00 as scan code.

  For a keyboard with an ISO physical layout, use the scan codes below:

      +---+---+---+---+---+---+---+---+---+---+---+---+---+-------+
      |35 |1e |1f |20 |21 |22 |23 |24 |25 |26 |27 |2d |2e |BackSp |
      +---+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-----+
      | Tab |14 |1a |08 |15 |17 |1c |18 |0c |12 |13 |2f |30 | Ret |
      +-----++--++--++--++--++--++--++--++--++--++--++--++--++    |
      |CapsL |04 |16 |07 |09 |0a |0b |0d |0e |0f |33 |34 |31 |    |
      +----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+---+----+
      |Shi.|32 |1d |1b |06 |19 |05 |11 |10 |36 |37 |38 |  Shift   |
      +----+---++--+-+-+---+---+---+---+---+--++---+---++----+----+
      |Ctrl|Win |Alt |                        |AlGr|Win |Menu|Ctrl|
      +----+----+----+------------------------+----+----+----+----+

  The ANSI layout is identical except that key 0x31 is above (rather
  than next to) Return, and there is not key 0x32.

  Give a unique name to the layout array, then declare it in Keyboard.h
  with a line of the form:

    extern const uint8_t KeyboardLayout_xx_YY[];

  == Encoding details ==

  All scan codes are less than 0x80, which makes bit 7 available to
  signal that a modifier (Shift or AltGr) is needed to generate the
  character. With only one exception, keys that are used with modifiers
  have scan codes that are less than 0x40. This makes bit 6 available
  to signal whether the modifier is Shift or AltGr. The exception is
  0x64, the key next next to Left Shift on the ISO layout (and absent
  from the ANSI layout). We handle it by replacing its value by 0x32 in
  the layout arrays.
*/

#include <Arduino.h>

// Modifier keys for _asciimap[] table (not to be used directly)
#define SHIFT           0x80
#define ALT_GR          0x40
#define ISO_KEY         0x64
#define ISO_REPLACEMENT 0x32
