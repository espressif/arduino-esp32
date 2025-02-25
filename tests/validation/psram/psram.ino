#include <Arduino.h>
#include <unity.h>

#define MAX_TEST_SIZE 512 * 1024  // 512KB

void *buf = NULL;
uint32_t psram_size = 0;

void psram_found(void) {
  psram_size = ESP.getPsramSize();
  TEST_ASSERT_TRUE(psram_size > 0);
}

void test_malloc_success(void) {
  buf = ps_malloc(MAX_TEST_SIZE);
  TEST_ASSERT_NOT_NULL(buf);
  free(buf);
  buf = NULL;
}

void test_calloc_success(void) {
  buf = ps_calloc(MAX_TEST_SIZE, 1);
  TEST_ASSERT_NOT_NULL(buf);
  free(buf);
  buf = NULL;
}

void test_realloc_success(void) {
  buf = ps_malloc(MAX_TEST_SIZE);
  TEST_ASSERT_NOT_NULL(buf);
  buf = ps_realloc(buf, MAX_TEST_SIZE + 1024);
  TEST_ASSERT_NOT_NULL(buf);
  free(buf);
  buf = NULL;
}

void test_malloc_fail(void) {
  buf = ps_malloc(0xFFFFFFFF);
  TEST_ASSERT_NULL(buf);
}

void test_memset_all_zeroes(void) {
  memset(buf, 0, MAX_TEST_SIZE);
  for (size_t i = 0; i < MAX_TEST_SIZE; i++) {
    TEST_ASSERT_EQUAL(0, ((uint8_t *)buf)[i]);
  }
}

void test_memset_all_ones(void) {
  memset(buf, 0xFF, MAX_TEST_SIZE);
  for (size_t i = 0; i < MAX_TEST_SIZE; i++) {
    TEST_ASSERT_EQUAL(0xFF, ((uint8_t *)buf)[i]);
  }
}

void test_memset_alternating(void) {
  for (size_t i = 0; i < MAX_TEST_SIZE; i++) {
    ((uint8_t *)buf)[i] = i % 2 == 0 ? 0x00 : 0xFF;
  }
  memset(buf, 0xAA, MAX_TEST_SIZE);
  for (size_t i = 0; i < MAX_TEST_SIZE; i++) {
    TEST_ASSERT_EQUAL(0xAA, ((uint8_t *)buf)[i]);
  }
}

void test_memset_random(void) {
  for (size_t i = 0; i < MAX_TEST_SIZE; i++) {
    ((uint8_t *)buf)[i] = random(0, 256);
  }
  memset(buf, 0x55, MAX_TEST_SIZE);
  for (size_t i = 0; i < MAX_TEST_SIZE; i++) {
    TEST_ASSERT_EQUAL(0x55, ((uint8_t *)buf)[i]);
  }
}

void test_memcpy(void) {
  void *buf2 = malloc(1024);  // 1KB
  TEST_ASSERT_NOT_NULL(buf2);
  memset(buf, 0x55, MAX_TEST_SIZE);
  memset(buf2, 0xAA, 1024);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"

  for (size_t i = 0; i < MAX_TEST_SIZE; i += 1024) {
    memcpy(buf + i, buf2, 1024);
  }

  for (size_t i = 0; i < MAX_TEST_SIZE; i += 1024) {
    TEST_ASSERT_NULL(memcmp(buf + i, buf2, 1024));
  }

#pragma GCC diagnostic pop

  free(buf2);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();
  RUN_TEST(psram_found);

  if (psram_size == 0) {
    UNITY_END();
    return;
  }

  RUN_TEST(test_malloc_success);
  RUN_TEST(test_malloc_fail);
  RUN_TEST(test_calloc_success);
  RUN_TEST(test_realloc_success);
  buf = ps_malloc(MAX_TEST_SIZE);
  RUN_TEST(test_memset_all_zeroes);
  RUN_TEST(test_memset_all_ones);
  RUN_TEST(test_memset_alternating);
#ifndef CONFIG_IDF_TARGET_ESP32P4
  // These tests are taking too long on ESP32-P4 in Wokwi
  RUN_TEST(test_memset_random);
  RUN_TEST(test_memcpy);
#endif
  UNITY_END();
}

void loop() {}
