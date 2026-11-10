/*
 * String concatenation regressions for the existing Unity validation suite.
 * This example code is in the Public Domain (or CC0 licensed, at your option).
 */

#pragma once

#include <Arduino.h>
#include <unity.h>

static void assertString(const char *expected, const String &actual) {
  TEST_ASSERT_EQUAL_UINT(strlen(expected), actual.length());
  TEST_ASSERT_EQUAL_MEMORY(expected, actual.c_str(), strlen(expected) + 1);
}

void test_string_concat_cstr_inline_growth(void) {
  String value("0123456789abc");
  TEST_ASSERT_TRUE(value.concat(value.c_str()));
  assertString("0123456789abc0123456789abc", value);
}

void test_string_concat_cstr_inline_substring(void) {
  String value("0123456789abc");
  TEST_ASSERT_TRUE(value.concat(value.c_str() + 3));
  assertString("0123456789abc3456789abc", value);
}

void test_string_concat_cstr_heap_growth(void) {
  String value("0123456789abcdefghijklmnopqrstuvwxyz");
  TEST_ASSERT_TRUE(value.concat(value.c_str()));
  assertString("0123456789abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz", value);
}

void test_string_concat_buffer_heap_substring(void) {
  String value("0123456789abcdefghijklmnopqrstuvwxyz");
  const uint8_t *source = reinterpret_cast<const uint8_t *>(value.c_str() + 10);
  TEST_ASSERT_TRUE(value.concat(source, 24));
  assertString("0123456789abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwx", value);
}

void test_string_concat_cstr_reserved(void) {
  String value("0123456789abc");
  TEST_ASSERT_TRUE(value.reserve(64));
  TEST_ASSERT_TRUE(value.concat(value.c_str()));
  assertString("0123456789abc0123456789abc", value);

  value = "abcdef";
  TEST_ASSERT_TRUE(value.concat(value.c_str() + 2, 3));
  assertString("abcdefcde", value);

  String small("ab");
  TEST_ASSERT_TRUE(small.concat(small.c_str()));
  assertString("abab", small);
}

void test_string_concat_self_overloads(void) {
  String value("0123456789abc");
  TEST_ASSERT_TRUE(value.concat(value));
  assertString("0123456789abc0123456789abc", value);

  String viaOperator("0123456789abc");
  viaOperator += viaOperator.c_str();
  assertString("0123456789abc0123456789abc", viaOperator);
}

void test_string_concat_external_and_empty(void) {
  String value("prefix");
  char suffix[] = "-ram";
  TEST_ASSERT_TRUE(value.concat(suffix));
  TEST_ASSERT_TRUE(value.concat(F("-flash")));
  assertString("prefix-ram-flash", value);

  TEST_ASSERT_TRUE(value.concat("", 0));
  TEST_ASSERT_FALSE(value.concat(static_cast<const char *>(nullptr)));
  assertString("prefix-ram-flash", value);
}

void test_string_concat_binary_alias(void) {
  const uint8_t bytes[] = {'a', 0, 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l'};
  String value(bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(value.concat(reinterpret_cast<const uint8_t *>(value.c_str()), sizeof(bytes)));
  TEST_ASSERT_EQUAL_UINT(2 * sizeof(bytes), value.length());
  TEST_ASSERT_EQUAL_MEMORY(bytes, value.c_str(), sizeof(bytes));
  TEST_ASSERT_EQUAL_MEMORY(bytes, value.c_str() + sizeof(bytes), sizeof(bytes));
  TEST_ASSERT_EQUAL_CHAR('\0', value.c_str()[value.length()]);
}
