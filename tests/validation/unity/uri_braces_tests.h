/*
 * UriBraces regressions for the existing Unity validation suite.
 * This example code is in the Public Domain (or CC0 licensed, at your option).
 */

#pragma once

#include <Arduino.h>
#include <uri/UriBraces.h>
#include <unity.h>

void test_uri_braces_suffixed_argument(void) {
  UriBraces route("/files/{}.json");
  std::vector<String> args;
  route.initPathArgs(args);
  TEST_ASSERT_EQUAL_UINT(1, args.size());
  TEST_ASSERT_TRUE(route.canHandle("/files/report.json", args));
  TEST_ASSERT_EQUAL_STRING("report", args[0].c_str());
}

void test_uri_braces_suffix_rejects_nested_paths(void) {
  UriBraces route("/files/{}.json");
  std::vector<String> args;
  route.initPathArgs(args);
  TEST_ASSERT_FALSE(route.canHandle("/files/a/b.json", args));
  TEST_ASSERT_FALSE(route.canHandle("/files//report.json", args));
  TEST_ASSERT_FALSE(route.canHandle("/files/a/b/c.json", args));
}

void test_uri_braces_multiple_arguments(void) {
  UriBraces route("/teams/{}/files/{}.json");
  std::vector<String> args;
  route.initPathArgs(args);
  TEST_ASSERT_EQUAL_UINT(2, args.size());
  TEST_ASSERT_TRUE(route.canHandle("/teams/blue/files/report.json", args));
  TEST_ASSERT_EQUAL_STRING("blue", args[0].c_str());
  TEST_ASSERT_EQUAL_STRING("report", args[1].c_str());
  TEST_ASSERT_FALSE(route.canHandle("/teams/blue/files/a/b.json", args));
}

void test_uri_braces_arguments_in_one_segment(void) {
  UriBraces route("/files/{}-{}.json");
  std::vector<String> args;
  route.initPathArgs(args);
  TEST_ASSERT_EQUAL_UINT(2, args.size());
  TEST_ASSERT_TRUE(route.canHandle("/files/report-2026.json", args));
  TEST_ASSERT_EQUAL_STRING("report", args[0].c_str());
  TEST_ASSERT_EQUAL_STRING("2026", args[1].c_str());
  TEST_ASSERT_FALSE(route.canHandle("/files/a/b-2026.json", args));
  TEST_ASSERT_FALSE(route.canHandle("/files/report-a/b.json", args));
}

void test_uri_braces_trailing_argument(void) {
  UriBraces route("/files/{}");
  std::vector<String> args;
  route.initPathArgs(args);
  TEST_ASSERT_TRUE(route.canHandle("/files/report", args));
  TEST_ASSERT_EQUAL_STRING("report", args[0].c_str());
  TEST_ASSERT_FALSE(route.canHandle("/files/a/b", args));
}

void test_uri_braces_rejects_mismatched_literals(void) {
  UriBraces route("/files/{}.json");
  std::vector<String> args;
  route.initPathArgs(args);
  TEST_ASSERT_FALSE(route.canHandle("/files/report.txt", args));
  TEST_ASSERT_FALSE(route.canHandle("/files/report.json/extra", args));
  TEST_ASSERT_FALSE(route.canHandle("/other/report.json", args));
  TEST_ASSERT_FALSE(route.canHandle("/files/report", args));
}

void test_uri_braces_empty_argument(void) {
  UriBraces route("/files/{}.json");
  std::vector<String> args;
  route.initPathArgs(args);
  // Preserve the existing acceptance of an empty path argument.
  TEST_ASSERT_TRUE(route.canHandle("/files/.json", args));
  TEST_ASSERT_EQUAL_STRING("", args[0].c_str());
}

void test_uri_braces_valid_after_rejection(void) {
  UriBraces route("/files/{}.json");
  std::vector<String> args;
  route.initPathArgs(args);
  TEST_ASSERT_FALSE(route.canHandle("/files/a/b.json", args));
  TEST_ASSERT_TRUE(route.canHandle("/files/recovered.json", args));
  TEST_ASSERT_EQUAL_STRING("recovered", args[0].c_str());
}
