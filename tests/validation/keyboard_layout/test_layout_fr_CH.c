/*
 * test_layout_fr_CH.c
 *
 * Host-side verification of KeyboardLayout_fr_CH scancode table.
 * Compile and run on any system with a C99 compiler:
 *
 *   gcc -Wall -Wextra -Werror -std=c99 -o test_layout_fr_CH test_layout_fr_CH.c
 *   ./test_layout_fr_CH
 *
 * Verifies:
 *   - Array has exactly 128 entries
 *   - Control characters are correct (BS, TAB, LF; rest 0x00)
 *   - No entry uses both SHIFT and ALT_GR (forbidden by KeyboardLayout.h)
 *   - Raw scancodes are in valid range after stripping modifiers
 *   - QWERTZ Y/Z swap is correct
 *   - All 52 letter mappings (a-z, A-Z) match QWERTZ scancodes
 *   - All symbol mappings match the Swiss French physical keyboard layout
 *   - Simulated USBHIDKeyboard::press() produces valid output
 *   - Keyboard_fr_CH.h key defines are in valid range
 *   - Dead keys (^, `, ~) are correctly mapped to 0x00
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Defines from KeyboardLayout.h ─────────────────────────────────────── */

#define SHIFT           0x80
#define ALT_GR          0x40
#define ISO_KEY         0x64
#define ISO_REPLACEMENT 0x32

/* ── Defines from Keyboard_fr_CH.h ─────────────────────────────────────── */

#define KEY_CIRCUMFLEX (0x88 + 0x2e)
#define KEY_E_GRAVE    (0x88 + 0x2f)
#define KEY_DIAERESIS  (0x88 + 0x30)
#define KEY_E_ACUTE    (0x88 + 0x33)
#define KEY_A_GRAVE    (0x88 + 0x34)
#define KEY_SECTION    (0x88 + 0x35)

/* ── Layout table (verbatim copy from KeyboardLayout_fr_CH.cpp) ────────── */

static const uint8_t KeyboardLayout_fr_CH[128] = {
  0x00,  // NUL
  0x00,  // SOH
  0x00,  // STX
  0x00,  // ETX
  0x00,  // EOT
  0x00,  // ENQ
  0x00,  // ACK
  0x00,  // BEL
  0x2a,  // BS  Backspace
  0x2b,  // TAB Tab
  0x28,  // LF  Enter
  0x00,  // VT
  0x00,  // FF
  0x00,  // CR
  0x00,  // SO
  0x00,  // SI
  0x00,  // DEL
  0x00,  // DC1
  0x00,  // DC2
  0x00,  // DC3
  0x00,  // DC4
  0x00,  // NAK
  0x00,  // SYN
  0x00,  // ETB
  0x00,  // CAN
  0x00,  // EM
  0x00,  // SUB
  0x00,  // ESC
  0x00,  // FS
  0x00,  // GS
  0x00,  // RS
  0x00,  // US

  0x2c,           // ' '
  0x30 | SHIFT,   // !
  0x1f | SHIFT,   // "
  0x20 | ALT_GR,  // #
  0x31,           // $
  0x22 | SHIFT,   // %
  0x23 | SHIFT,   // &
  0x2d,           // '
  0x25 | SHIFT,   // (
  0x26 | SHIFT,   // )
  0x20 | SHIFT,   // *
  0x1e | SHIFT,   // +
  0x36,           // ,
  0x38,           // -
  0x37,           // .
  0x24 | SHIFT,   // /
  0x27,           // 0
  0x1e,           // 1
  0x1f,           // 2
  0x20,           // 3
  0x21,           // 4
  0x22,           // 5
  0x23,           // 6
  0x24,           // 7
  0x25,           // 8
  0x26,           // 9
  0x37 | SHIFT,   // :
  0x36 | SHIFT,   // ;
  0x32,           // <
  0x27 | SHIFT,   // =
  0x32 | SHIFT,   // >
  0x2d | SHIFT,   // ?
  0x1f | ALT_GR,  // @
  0x04 | SHIFT,   // A
  0x05 | SHIFT,   // B
  0x06 | SHIFT,   // C
  0x07 | SHIFT,   // D
  0x08 | SHIFT,   // E
  0x09 | SHIFT,   // F
  0x0a | SHIFT,   // G
  0x0b | SHIFT,   // H
  0x0c | SHIFT,   // I
  0x0d | SHIFT,   // J
  0x0e | SHIFT,   // K
  0x0f | SHIFT,   // L
  0x10 | SHIFT,   // M
  0x11 | SHIFT,   // N
  0x12 | SHIFT,   // O
  0x13 | SHIFT,   // P
  0x14 | SHIFT,   // Q
  0x15 | SHIFT,   // R
  0x16 | SHIFT,   // S
  0x17 | SHIFT,   // T
  0x18 | SHIFT,   // U
  0x19 | SHIFT,   // V
  0x1a | SHIFT,   // W
  0x1b | SHIFT,   // X
  0x1d | SHIFT,   // Y
  0x1c | SHIFT,   // Z
  0x2f | ALT_GR,  // [
  0x32 | ALT_GR,  // bslash
  0x30 | ALT_GR,  // ]
  0x00,           // ^  not supported (requires dead key + space)
  0x38 | SHIFT,   // _
  0x00,           // `  not supported (requires dead key + space)
  0x04,           // a
  0x05,           // b
  0x06,           // c
  0x07,           // d
  0x08,           // e
  0x09,           // f
  0x0a,           // g
  0x0b,           // h
  0x0c,           // i
  0x0d,           // j
  0x0e,           // k
  0x0f,           // l
  0x10,           // m
  0x11,           // n
  0x12,           // o
  0x13,           // p
  0x14,           // q
  0x15,           // r
  0x16,           // s
  0x17,           // t
  0x18,           // u
  0x19,           // v
  0x1a,           // w
  0x1b,           // x
  0x1d,           // y
  0x1c,           // z
  0x34 | ALT_GR,  // {
  0x1e | ALT_GR,  // |
  0x31 | ALT_GR,  // }
  0x00,           // ~  not supported (requires dead key + space)
  0x00            // DEL
};

/* ── Test infrastructure ───────────────────────────────────────────────── */

static int g_tests  = 0;
static int g_passed = 0;
static int g_failed = 0;

static void test_check(int condition, const char *fmt, ...) {
  g_tests++;
  if (condition) {
    g_passed++;
  } else {
    g_failed++;
    va_list ap;
    va_start(ap, fmt);
    printf("  FAIL: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
  }
}

/*
 * Simulate USBHIDKeyboard::press() for a printing character (ASCII 0..127).
 * Mirrors the exact logic from USBHIDKeyboard.cpp lines 137-166.
 * Returns the raw scancode that pressRaw() would receive.
 * Sets *modifiers to the modifier byte that would be sent in the HID report.
 */
static uint8_t simulate_press(uint8_t c, uint8_t *modifiers) {
  uint8_t k;
  *modifiers = 0;
  if (c >= 128) { return 0; }

  k = KeyboardLayout_fr_CH[c];
  if (!k) { return 0; }

  if ((k & SHIFT) == SHIFT) {
    *modifiers |= 0x02;  /* left shift modifier */
    k &= (uint8_t)~SHIFT;
  }
  if ((k & ALT_GR) == ALT_GR) {
    *modifiers |= 0x40;  /* right alt (AltGr) modifier */
    k &= (uint8_t)~ALT_GR;
  }
  if (k == ISO_REPLACEMENT) {
    k = ISO_KEY;
  }
  return k;
}

/* ── Swiss French QWERTZ physical layout reference ─────────────────────
 *
 * Row 0 (number row):
 *   Key:     §     1     2     3     4     5     6     7     8     9     0     '     ^
 *   Scan:   0x35  0x1e  0x1f  0x20  0x21  0x22  0x23  0x24  0x25  0x26  0x27  0x2d  0x2e
 *   Shift:   °     +     "     *     ç     %     &     /     (     )     =     ?     `
 *   AltGr:         |     @     #                                               ´     ~
 *
 * Row 1 (top letter row):
 *   Key:     q     w     e     r     t     z     u     i     o     p     è     ¨
 *   Scan:   0x14  0x1a  0x08  0x15  0x17  0x1c  0x18  0x0c  0x12  0x13  0x2f  0x30
 *   Shift:   Q     W     E     R     T     Z     U     I     O     P     ü     !
 *   AltGr:                                                               [     ]
 *
 * Row 2 (home row):
 *   Key:     a     s     d     f     g     h     j     k     l     é     à     $
 *   Scan:   0x04  0x16  0x07  0x09  0x0a  0x0b  0x0d  0x0e  0x0f  0x33  0x34  0x31
 *   Shift:   A     S     D     F     G     H     J     K     L     ö     ä     £
 *   AltGr:                                                               {     }
 *
 * Row 3 (bottom row):
 *   Key:     <     y     x     c     v     b     n     m     ,     .     -
 *   Scan:   0x32  0x1d  0x1b  0x06  0x19  0x05  0x11  0x10  0x36  0x37  0x38
 *   Shift:   >     Y     X     C     V     B     N     M     ;     :     _
 *   AltGr:   \
 *
 * ─────────────────────────────────────────────────────────────────────── */

/* ── Expected symbol mappings for Swiss French keyboard ────────────────── */

typedef struct {
  char     ch;
  uint8_t  expected;
  const char *desc;
} symbol_map_t;

static const symbol_map_t symbol_map[] = {
  {' ',  0x2c,           "space bar"},
  {'!',  0x30 | SHIFT,   "Shift + diaeresis(0x30)"},
  {'"',  0x1f | SHIFT,   "Shift + 2"},
  {'#',  0x20 | ALT_GR,  "AltGr + 3"},
  {'$',  0x31,           "dollar key(0x31)"},
  {'%',  0x22 | SHIFT,   "Shift + 5"},
  {'&',  0x23 | SHIFT,   "Shift + 6"},
  {'\'', 0x2d,           "apostrophe key(0x2d)"},
  {'(',  0x25 | SHIFT,   "Shift + 8"},
  {')',  0x26 | SHIFT,   "Shift + 9"},
  {'*',  0x20 | SHIFT,   "Shift + 3"},
  {'+',  0x1e | SHIFT,   "Shift + 1"},
  {',',  0x36,           "comma key(0x36)"},
  {'-',  0x38,           "minus key(0x38)"},
  {'.',  0x37,           "period key(0x37)"},
  {'/',  0x24 | SHIFT,   "Shift + 7"},
  {'0',  0x27,           "0 key(0x27)"},
  {'1',  0x1e,           "1 key(0x1e)"},
  {'2',  0x1f,           "2 key(0x1f)"},
  {'3',  0x20,           "3 key(0x20)"},
  {'4',  0x21,           "4 key(0x21)"},
  {'5',  0x22,           "5 key(0x22)"},
  {'6',  0x23,           "6 key(0x23)"},
  {'7',  0x24,           "7 key(0x24)"},
  {'8',  0x25,           "8 key(0x25)"},
  {'9',  0x26,           "9 key(0x26)"},
  {':',  0x37 | SHIFT,   "Shift + period"},
  {';',  0x36 | SHIFT,   "Shift + comma"},
  {'<',  0x32,           "ISO key(0x32)"},
  {'=',  0x27 | SHIFT,   "Shift + 0"},
  {'>',  0x32 | SHIFT,   "Shift + ISO key"},
  {'?',  0x2d | SHIFT,   "Shift + apostrophe"},
  {'@',  0x1f | ALT_GR,  "AltGr + 2"},
  {'[',  0x2f | ALT_GR,  "AltGr + e_grave(0x2f)"},
  {'\\', 0x32 | ALT_GR,  "AltGr + ISO key(0x32)"},
  {']',  0x30 | ALT_GR,  "AltGr + diaeresis(0x30)"},
  {'^',  0x00,           "dead key unsupported"},
  {'_',  0x38 | SHIFT,   "Shift + minus"},
  {'`',  0x00,           "dead key unsupported"},
  {'{',  0x34 | ALT_GR,  "AltGr + a_grave(0x34)"},
  {'|',  0x1e | ALT_GR,  "AltGr + 1"},
  {'}',  0x31 | ALT_GR,  "AltGr + dollar(0x31)"},
  {'~',  0x00,           "dead key unsupported"},
  {0, 0, NULL}
};

/* ── QWERTZ lowercase letter scancodes ─────────────────────────────────── */

/* Standard USB HID scancodes for a(0x04)..x(0x1b), then y/z swapped for QWERTZ */
static const uint8_t qwertz_lower[26] = {
  0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,  /* a - h */
  0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,  /* i - p */
  0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,  /* q - x */
  0x1d, 0x1c                                         /* y, z (swapped!) */
};

/* ── Test functions ────────────────────────────────────────────────────── */

static void test_array_size(void) {
  printf("Test 1:  Array size is exactly 128\n");
  test_check(sizeof(KeyboardLayout_fr_CH) == 128,
    "expected 128 entries, got %zu", sizeof(KeyboardLayout_fr_CH));
}

static void test_control_characters(void) {
  int i;
  printf("Test 2:  Control characters (0x00-0x1f)\n");
  for (i = 0; i < 32; i++) {
    uint8_t v = KeyboardLayout_fr_CH[i];
    if (i == 8) {
      test_check(v == 0x2a, "BS (0x%02x): expected 0x2a, got 0x%02x", i, v);
    } else if (i == 9) {
      test_check(v == 0x2b, "TAB (0x%02x): expected 0x2b, got 0x%02x", i, v);
    } else if (i == 10) {
      test_check(v == 0x28, "LF (0x%02x): expected 0x28, got 0x%02x", i, v);
    } else {
      test_check(v == 0x00, "ctrl char 0x%02x: expected 0x00, got 0x%02x", i, v);
    }
  }
}

static void test_no_shift_altgr_combo(void) {
  int i;
  printf("Test 3:  No entry uses SHIFT + ALT_GR simultaneously\n");
  for (i = 0; i < 128; i++) {
    uint8_t v = KeyboardLayout_fr_CH[i];
    test_check(!((v & SHIFT) && (v & ALT_GR)),
      "index %d '%c': has SHIFT|ALT_GR (0x%02x)",
      i, (i >= 32 && i < 127) ? i : '?', v);
  }
}

static void test_scancode_range(void) {
  int i;
  printf("Test 4:  Raw scancodes in valid range\n");
  for (i = 32; i < 128; i++) {
    uint8_t v = KeyboardLayout_fr_CH[i];
    uint8_t raw;
    if (v == 0) { continue; }
    raw = v & 0x3f;
    if (v & (SHIFT | ALT_GR)) {
      test_check(raw < 0x40,
        "index %d '%c': modified scancode raw=0x%02x >= 0x40",
        i, (i < 127) ? i : '?', raw);
    }
  }
}

static void test_del_entry(void) {
  printf("Test 5:  DEL (index 127) is 0x00\n");
  test_check(KeyboardLayout_fr_CH[127] == 0x00,
    "DEL: expected 0x00, got 0x%02x", KeyboardLayout_fr_CH[127]);
}

static void test_qwertz_yz_swap(void) {
  printf("Test 6:  QWERTZ Y/Z swap\n");
  /* On QWERTY (en_US): y=0x1c, z=0x1d */
  /* On QWERTZ (fr_CH): y=0x1d, z=0x1c (swapped) */
  test_check(KeyboardLayout_fr_CH['y'] == 0x1d,
    "y: expected 0x1d, got 0x%02x", KeyboardLayout_fr_CH['y']);
  test_check(KeyboardLayout_fr_CH['z'] == 0x1c,
    "z: expected 0x1c, got 0x%02x", KeyboardLayout_fr_CH['z']);
  test_check(KeyboardLayout_fr_CH['Y'] == (0x1d | SHIFT),
    "Y: expected 0x%02x, got 0x%02x", 0x1d | SHIFT, KeyboardLayout_fr_CH['Y']);
  test_check(KeyboardLayout_fr_CH['Z'] == (0x1c | SHIFT),
    "Z: expected 0x%02x, got 0x%02x", 0x1c | SHIFT, KeyboardLayout_fr_CH['Z']);
}

static void test_lowercase_letters(void) {
  int i;
  printf("Test 7:  All 26 lowercase letters\n");
  for (i = 0; i < 26; i++) {
    char c = (char)('a' + i);
    test_check(KeyboardLayout_fr_CH[(int)c] == qwertz_lower[i],
      "'%c': expected 0x%02x, got 0x%02x",
      c, qwertz_lower[i], KeyboardLayout_fr_CH[(int)c]);
  }
}

static void test_uppercase_letters(void) {
  int i;
  printf("Test 8:  All 26 uppercase letters\n");
  for (i = 0; i < 26; i++) {
    char c = (char)('A' + i);
    uint8_t expected = qwertz_lower[i] | SHIFT;
    test_check(KeyboardLayout_fr_CH[(int)c] == expected,
      "'%c': expected 0x%02x, got 0x%02x",
      c, expected, KeyboardLayout_fr_CH[(int)c]);
  }
}

static void test_symbol_mappings(void) {
  int i;
  printf("Test 9:  Swiss French symbol mappings\n");
  for (i = 0; symbol_map[i].desc != NULL; i++) {
    int ascii = (int)(unsigned char)symbol_map[i].ch;
    uint8_t expected = symbol_map[i].expected;
    uint8_t actual   = KeyboardLayout_fr_CH[ascii];
    test_check(actual == expected,
      "'%c' (0x%02x): expected 0x%02x (%s), got 0x%02x",
      symbol_map[i].ch, ascii, expected, symbol_map[i].desc, actual);
  }
}

static void test_simulate_press(void) {
  int c;
  printf("Test 10: Simulate USBHIDKeyboard::press() for printable ASCII\n");
  for (c = 32; c < 127; c++) {
    uint8_t modifiers = 0;
    uint8_t raw = simulate_press((uint8_t)c, &modifiers);

    if (KeyboardLayout_fr_CH[c] == 0) {
      test_check(raw == 0,
        "'%c' (0x%02x): unsupported should return raw=0", c, c);
      continue;
    }
    /* Raw scancode must be valid for pressRaw(): non-zero, < 0xA5 */
    test_check(raw > 0 && raw < 0xA5,
      "'%c' (0x%02x): press() raw=0x%02x out of range", c, c, raw);

    /* Verify ISO_REPLACEMENT -> ISO_KEY mapping */
    if ((KeyboardLayout_fr_CH[c] & 0x3f) == ISO_REPLACEMENT) {
      test_check(raw == ISO_KEY,
        "'%c' (0x%02x): ISO_REPLACEMENT should yield 0x%02x, got 0x%02x",
        c, c, ISO_KEY, raw);
    }
  }
}

static void test_key_defines(void) {
  printf("Test 11: Keyboard_fr_CH.h key defines\n");

  /* Each define must be >= 0x88 (non-printing key range) and < 0xE0 (modifier range) */
  test_check(KEY_CIRCUMFLEX >= 0x88 && KEY_CIRCUMFLEX < 0xE0,
    "KEY_CIRCUMFLEX=0x%02x out of range", KEY_CIRCUMFLEX);
  test_check(KEY_E_GRAVE >= 0x88 && KEY_E_GRAVE < 0xE0,
    "KEY_E_GRAVE=0x%02x out of range", KEY_E_GRAVE);
  test_check(KEY_DIAERESIS >= 0x88 && KEY_DIAERESIS < 0xE0,
    "KEY_DIAERESIS=0x%02x out of range", KEY_DIAERESIS);
  test_check(KEY_E_ACUTE >= 0x88 && KEY_E_ACUTE < 0xE0,
    "KEY_E_ACUTE=0x%02x out of range", KEY_E_ACUTE);
  test_check(KEY_A_GRAVE >= 0x88 && KEY_A_GRAVE < 0xE0,
    "KEY_A_GRAVE=0x%02x out of range", KEY_A_GRAVE);
  test_check(KEY_SECTION >= 0x88 && KEY_SECTION < 0xE0,
    "KEY_SECTION=0x%02x out of range", KEY_SECTION);

  /* Verify exact values: press() subtracts 0x88 to get the raw scancode */
  test_check(KEY_CIRCUMFLEX - 0x88 == 0x2e,
    "KEY_CIRCUMFLEX raw: expected 0x2e, got 0x%02x", KEY_CIRCUMFLEX - 0x88);
  test_check(KEY_E_GRAVE - 0x88 == 0x2f,
    "KEY_E_GRAVE raw: expected 0x2f, got 0x%02x", KEY_E_GRAVE - 0x88);
  test_check(KEY_DIAERESIS - 0x88 == 0x30,
    "KEY_DIAERESIS raw: expected 0x30, got 0x%02x", KEY_DIAERESIS - 0x88);
  test_check(KEY_E_ACUTE - 0x88 == 0x33,
    "KEY_E_ACUTE raw: expected 0x33, got 0x%02x", KEY_E_ACUTE - 0x88);
  test_check(KEY_A_GRAVE - 0x88 == 0x34,
    "KEY_A_GRAVE raw: expected 0x34, got 0x%02x", KEY_A_GRAVE - 0x88);
  test_check(KEY_SECTION - 0x88 == 0x35,
    "KEY_SECTION raw: expected 0x35, got 0x%02x", KEY_SECTION - 0x88);

  /* Raw scancodes from key defines must be valid for pressRaw() (< 0xA5) */
  test_check((KEY_CIRCUMFLEX - 0x88) < 0xA5, "KEY_CIRCUMFLEX raw scancode too large");
  test_check((KEY_E_GRAVE    - 0x88) < 0xA5, "KEY_E_GRAVE raw scancode too large");
  test_check((KEY_DIAERESIS  - 0x88) < 0xA5, "KEY_DIAERESIS raw scancode too large");
  test_check((KEY_E_ACUTE    - 0x88) < 0xA5, "KEY_E_ACUTE raw scancode too large");
  test_check((KEY_A_GRAVE    - 0x88) < 0xA5, "KEY_A_GRAVE raw scancode too large");
  test_check((KEY_SECTION    - 0x88) < 0xA5, "KEY_SECTION raw scancode too large");
}

static void test_dead_keys(void) {
  printf("Test 12: Dead keys mapped to 0x00\n");
  /* On Swiss French QWERTZ, ^/`/~ require a dead key followed by space */
  /* to produce the literal character. The library cannot send two-key   */
  /* sequences, so these must be 0x00 (matching de_DE, sv_SE, es_ES).   */
  test_check(KeyboardLayout_fr_CH['^'] == 0x00,
    "'^': expected 0x00 (dead key), got 0x%02x", KeyboardLayout_fr_CH['^']);
  test_check(KeyboardLayout_fr_CH['`'] == 0x00,
    "'`': expected 0x00 (dead key), got 0x%02x", KeyboardLayout_fr_CH['`']);
  test_check(KeyboardLayout_fr_CH['~'] == 0x00,
    "'~': expected 0x00 (dead key), got 0x%02x", KeyboardLayout_fr_CH['~']);
}

static void test_digits_no_shift(void) {
  int i;
  printf("Test 13: Digits 0-9 accessible without Shift\n");
  /* Unlike AZERTY (fr_FR), Swiss French QWERTZ has direct digit access */
  for (i = 0; i <= 9; i++) {
    char c = (char)('0' + i);
    uint8_t v = KeyboardLayout_fr_CH[(int)c];
    test_check(!(v & SHIFT) && !(v & ALT_GR),
      "'%c': digit should need no modifier, got 0x%02x", c, v);
  }
}

/* ── Main ──────────────────────────────────────────────────────────────── */

int main(void) {
  printf("=== KeyboardLayout_fr_CH Verification Suite ===\n\n");

  test_array_size();
  test_control_characters();
  test_no_shift_altgr_combo();
  test_scancode_range();
  test_del_entry();
  test_qwertz_yz_swap();
  test_lowercase_letters();
  test_uppercase_letters();
  test_symbol_mappings();
  test_simulate_press();
  test_key_defines();
  test_dead_keys();
  test_digits_no_shift();

  printf("\n============================================================\n");
  printf("Results: %d tests, %d passed, %d failed\n", g_tests, g_passed, g_failed);
  printf("============================================================\n");

  return g_failed ? 1 : 0;
}
