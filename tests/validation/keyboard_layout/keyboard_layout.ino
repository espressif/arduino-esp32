/*
  keyboard_layout.ino - Validation tests for USB HID keyboard layouts.

  Tests structural properties that must hold for all keyboard layout tables:
    - Control character mappings
    - Modifier flag constraints (SHIFT and ALT_GR mutual exclusivity)
    - Valid scancode ranges
    - Letter case consistency
    - Printable character coverage

  To add a new layout, simply add an entry to the layouts[] array below.
  All tests will automatically run against it.
*/

#include <Arduino.h>
#include <unity.h>

// Include USB library header to ensure layout tables are compiled
#include "USBHIDKeyboard.h"

// Modifier flags (from keyboardLayout/KeyboardLayout.h)
#ifndef SHIFT
#define SHIFT 0x80
#endif
#ifndef ALT_GR
#define ALT_GR 0x40
#endif

// Layout table forward declarations
extern const uint8_t KeyboardLayout_da_DK[];
extern const uint8_t KeyboardLayout_de_DE[];
extern const uint8_t KeyboardLayout_en_US[];
extern const uint8_t KeyboardLayout_es_ES[];
extern const uint8_t KeyboardLayout_fr_CH[];
extern const uint8_t KeyboardLayout_fr_FR[];
extern const uint8_t KeyboardLayout_hu_HU[];
extern const uint8_t KeyboardLayout_it_IT[];
extern const uint8_t KeyboardLayout_pt_BR[];
extern const uint8_t KeyboardLayout_pt_PT[];
extern const uint8_t KeyboardLayout_sv_SE[];

typedef struct {
  const uint8_t *table;
  const char *name;
} LayoutEntry;

// Add new layouts here - all tests run automatically for each entry.
static const LayoutEntry layouts[] = {
  {KeyboardLayout_da_DK, "da_DK"}, {KeyboardLayout_de_DE, "de_DE"}, {KeyboardLayout_en_US, "en_US"}, {KeyboardLayout_es_ES, "es_ES"},
  {KeyboardLayout_fr_CH, "fr_CH"}, {KeyboardLayout_fr_FR, "fr_FR"}, {KeyboardLayout_hu_HU, "hu_HU"}, {KeyboardLayout_it_IT, "it_IT"},
  {KeyboardLayout_pt_BR, "pt_BR"}, {KeyboardLayout_pt_PT, "pt_PT"}, {KeyboardLayout_sv_SE, "sv_SE"},
};

#define NUM_LAYOUTS (sizeof(layouts) / sizeof(layouts[0]))

static char msg_buf[80];

/* These functions are intended to be called before and after each test. */
void setUp(void) {}

void tearDown(void) {}

// Unused control characters (NUL-BEL, VT, FF, CR, SO-US) must map to 0x00
void test_unused_control_chars(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    for (int c = 0; c <= 7; c++) {
      snprintf(msg_buf, sizeof(msg_buf), "%s: ASCII %d", n, c);
      TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x00, pgm_read_byte(t + c), msg_buf);
    }
    for (int c = 11; c <= 31; c++) {
      snprintf(msg_buf, sizeof(msg_buf), "%s: ASCII %d", n, c);
      TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x00, pgm_read_byte(t + c), msg_buf);
    }
  }
}

// BS, TAB, and LF must map to their standard USB HID scancodes
void test_standard_control_keys(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    snprintf(msg_buf, sizeof(msg_buf), "%s: BS", n);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x2a, pgm_read_byte(t + 8), msg_buf);
    snprintf(msg_buf, sizeof(msg_buf), "%s: TAB", n);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x2b, pgm_read_byte(t + 9), msg_buf);
    snprintf(msg_buf, sizeof(msg_buf), "%s: LF", n);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x28, pgm_read_byte(t + 10), msg_buf);
  }
}

// Space (ASCII 32) must map to USB HID scancode 0x2c
void test_space_mapping(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x2c, pgm_read_byte(layouts[i].table + ' '), layouts[i].name);
  }
}

// No entry may have both SHIFT (0x80) and ALT_GR (0x40) set simultaneously
void test_no_combined_shift_altgr(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    for (int c = 0; c < 128; c++) {
      uint8_t entry = pgm_read_byte(t + c);
      if ((entry & SHIFT) && (entry & ALT_GR)) {
        snprintf(msg_buf, sizeof(msg_buf), "%s: ASCII %d has SHIFT+ALT_GR (0x%02x)", n, c, entry);
        TEST_FAIL_MESSAGE(msg_buf);
      }
    }
  }
}

// Non-zero entries must have a valid base scancode in range [0x04, 0x38]
void test_valid_scancode_range(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    for (int c = 0; c < 128; c++) {
      uint8_t entry = pgm_read_byte(t + c);
      if (entry == 0x00) {
        continue;
      }
      uint8_t scancode = entry & 0x3F;
      snprintf(msg_buf, sizeof(msg_buf), "%s: ASCII %d scancode 0x%02x", n, c, scancode);
      TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(0x04, scancode, msg_buf);
      TEST_ASSERT_LESS_OR_EQUAL_UINT8_MESSAGE(0x38, scancode, msg_buf);
    }
  }
}

// All 26 lowercase letters must be mapped without modifiers
void test_lowercase_letters(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    for (char c = 'a'; c <= 'z'; c++) {
      uint8_t entry = pgm_read_byte(t + c);
      snprintf(msg_buf, sizeof(msg_buf), "%s: '%c' unmapped", n, c);
      TEST_ASSERT_NOT_EQUAL_MESSAGE(0x00, entry, msg_buf);
      snprintf(msg_buf, sizeof(msg_buf), "%s: '%c' has modifier (0x%02x)", n, c, entry);
      TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, entry & 0xC0, msg_buf);
    }
  }
}

// All 26 uppercase letters must be mapped with SHIFT modifier only
void test_uppercase_letters(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    for (char c = 'A'; c <= 'Z'; c++) {
      uint8_t entry = pgm_read_byte(t + c);
      snprintf(msg_buf, sizeof(msg_buf), "%s: '%c' unmapped", n, c);
      TEST_ASSERT_NOT_EQUAL_MESSAGE(0x00, entry, msg_buf);
      snprintf(msg_buf, sizeof(msg_buf), "%s: '%c' missing SHIFT (0x%02x)", n, c, entry);
      TEST_ASSERT_TRUE_MESSAGE((entry & SHIFT) != 0, msg_buf);
      snprintf(msg_buf, sizeof(msg_buf), "%s: '%c' has ALT_GR (0x%02x)", n, c, entry);
      TEST_ASSERT_TRUE_MESSAGE((entry & ALT_GR) == 0, msg_buf);
    }
  }
}

// Upper and lowercase letters must share the same base scancode
void test_letter_case_consistency(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    for (int c = 0; c < 26; c++) {
      uint8_t lower = pgm_read_byte(t + 'a' + c) & 0x3F;
      uint8_t upper = pgm_read_byte(t + 'A' + c) & 0x3F;
      snprintf(msg_buf, sizeof(msg_buf), "%s: '%c'/'%c' scancode mismatch", n, 'a' + c, 'A' + c);
      TEST_ASSERT_EQUAL_UINT8_MESSAGE(lower, upper, msg_buf);
    }
  }
}

// All 10 digits must be mapped (non-zero)
void test_digits_mapped(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    for (char c = '0'; c <= '9'; c++) {
      snprintf(msg_buf, sizeof(msg_buf), "%s: '%c' unmapped", n, c);
      TEST_ASSERT_NOT_EQUAL_MESSAGE(0x00, pgm_read_byte(t + c), msg_buf);
    }
  }
}

// Each layout must use unique scancodes for distinct letters
void test_unique_letter_scancodes(void) {
  for (size_t i = 0; i < NUM_LAYOUTS; i++) {
    const uint8_t *t = layouts[i].table;
    const char *n = layouts[i].name;
    uint8_t scancodes[26];
    for (int c = 0; c < 26; c++) {
      scancodes[c] = pgm_read_byte(t + 'a' + c) & 0x3F;
    }
    for (int a = 0; a < 25; a++) {
      for (int b = a + 1; b < 26; b++) {
        if (scancodes[a] == scancodes[b]) {
          snprintf(msg_buf, sizeof(msg_buf), "%s: '%c' and '%c' share scancode 0x%02x", n, 'a' + a, 'a' + b, scancodes[a]);
          TEST_FAIL_MESSAGE(msg_buf);
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  UNITY_BEGIN();
  RUN_TEST(test_unused_control_chars);
  RUN_TEST(test_standard_control_keys);
  RUN_TEST(test_space_mapping);
  RUN_TEST(test_no_combined_shift_altgr);
  RUN_TEST(test_valid_scancode_range);
  RUN_TEST(test_lowercase_letters);
  RUN_TEST(test_uppercase_letters);
  RUN_TEST(test_letter_case_consistency);
  RUN_TEST(test_digits_mapped);
  RUN_TEST(test_unique_letter_scancodes);
  UNITY_END();
}

void loop() {}
