/*
 * Signed integer conversion regressions for the existing Unity validation suite.
 * This example code is in the Public Domain (or CC0 licensed, at your option).
 */

#pragma once

#include <errno.h>
#include <limits.h>
#include <limits>
#include <stdlib_noniso.h>

static constexpr long long kLongLongMin = std::numeric_limits<long long>::min();
static constexpr long long kLongLongMax = std::numeric_limits<long long>::max();

void test_ltoa_signed_limits(void) {
  char buffer[sizeof(long) * CHAR_BIT + 2];
  TEST_ASSERT_EQUAL_PTR(buffer, ltoa(LONG_MIN, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-2147483648", buffer);
  TEST_ASSERT_EQUAL_STRING("-2147483647", ltoa(LONG_MIN + 1, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("2147483647", ltoa(LONG_MAX, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-80000000", ltoa(LONG_MIN, buffer, 16));
  TEST_ASSERT_EQUAL_STRING("-10000000000000000000000000000000", ltoa(LONG_MIN, buffer, 2));
  TEST_ASSERT_EQUAL_STRING("0", ltoa(0, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("1", ltoa(1, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-1", ltoa(-1, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-3039", ltoa(-12345, buffer, 16));
}

void test_lltoa_signed_limits(void) {
  char buffer[sizeof(long long) * CHAR_BIT + 2];
  TEST_ASSERT_EQUAL_PTR(buffer, lltoa(kLongLongMin, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-9223372036854775808", buffer);
  TEST_ASSERT_EQUAL_STRING("-9223372036854775807", lltoa(kLongLongMin + 1, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("9223372036854775807", lltoa(kLongLongMax, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-8000000000000000", lltoa(kLongLongMin, buffer, 16));
  TEST_ASSERT_EQUAL_STRING("-1000000000000000000000000000000000000000000000000000000000000000", lltoa(kLongLongMin, buffer, 2));
  TEST_ASSERT_EQUAL_STRING("0", lltoa(0, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("1", lltoa(1, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-1", lltoa(-1, buffer, 10));
  TEST_ASSERT_EQUAL_STRING("-3039", lltoa(-12345, buffer, 16));
}

void test_signed_conversion_all_radices(void) {
  const long long values[] = {kLongLongMin, kLongLongMin + 1, LONG_MIN, LONG_MIN + 1, -12345, -1, 0, 1, 12345, LONG_MAX, kLongLongMax};
  char buffer[sizeof(long long) * CHAR_BIT + 2];

  for (int base = 2; base <= 16; base++) {
    for (long long value : values) {
      char *end = nullptr;
      errno = 0;
      long long parsed = strtoll(lltoa(value, buffer, base), &end, base);
      TEST_ASSERT_EQUAL_INT(0, errno);
      TEST_ASSERT_EQUAL_CHAR('\0', *end);
      TEST_ASSERT_TRUE(parsed == value);
      if (value >= LONG_MIN && value <= LONG_MAX) {
        errno = 0;
        long parsedLong = strtol(ltoa(static_cast<long>(value), buffer, base), &end, base);
        TEST_ASSERT_EQUAL_INT(0, errno);
        TEST_ASSERT_EQUAL_CHAR('\0', *end);
        TEST_ASSERT_TRUE(parsedLong == value);
      }
    }
  }
}

void test_signed_conversion_invalid_radices(void) {
  const int bases[] = {-1, 0, 1, 17, 36};
  char buffer[sizeof(long long) * CHAR_BIT + 2];
  for (int base : bases) {
    memset(buffer, 'x', sizeof(buffer));
    TEST_ASSERT_EQUAL_PTR(buffer, ltoa(LONG_MIN, buffer, base));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    TEST_ASSERT_EQUAL_CHAR('x', buffer[1]);
    memset(buffer, 'x', sizeof(buffer));
    TEST_ASSERT_EQUAL_PTR(buffer, lltoa(kLongLongMin, buffer, base));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    TEST_ASSERT_EQUAL_CHAR('x', buffer[1]);
  }
}

void test_string_signed_minimum_values(void) {
  TEST_ASSERT_EQUAL_STRING("-2147483648", String(LONG_MIN).c_str());
  TEST_ASSERT_EQUAL_STRING("-9223372036854775808", String(kLongLongMin).c_str());
  TEST_ASSERT_EQUAL_STRING("-80000000", String(LONG_MIN, HEX).c_str());
  TEST_ASSERT_EQUAL_STRING("-8000000000000000", String(kLongLongMin, HEX).c_str());

  String text("long=");
  TEST_ASSERT_TRUE(text.concat(LONG_MIN));
  TEST_ASSERT_EQUAL_STRING("long=-2147483648", text.c_str());
  text = "long long=";
  TEST_ASSERT_TRUE(text.concat(kLongLongMin));
  TEST_ASSERT_EQUAL_STRING("long long=-9223372036854775808", text.c_str());
  text = "";
  text += kLongLongMin;
  TEST_ASSERT_EQUAL_STRING("-9223372036854775808", text.c_str());
}
