#include <Arduino.h>
#include <unity.h>
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"

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

// ==================== heap_caps Verification ====================

void test_heap_caps_spiram(void) {
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  TEST_ASSERT_GREATER_THAN(0, free_psram);

  size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  TEST_ASSERT_GREATER_THAN(0, largest_block);
  TEST_ASSERT_LESS_OR_EQUAL(free_psram, largest_block);
}

void test_heap_caps_alloc(void) {
  void *p = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
  TEST_ASSERT_NOT_NULL(p);

  // Verify the pointer is actually in SPIRAM range
  TEST_ASSERT_TRUE(esp_ptr_external_ram(p));

  heap_caps_free(p);
}

// ==================== Large Allocation ====================

void test_large_allocation(void) {
  size_t alloc_size = psram_size / 2;
  if (alloc_size > 2 * 1024 * 1024) {
    alloc_size = 2 * 1024 * 1024;
  }

  void *large = ps_malloc(alloc_size);
  TEST_ASSERT_NOT_NULL_MESSAGE(large, "Large PSRAM allocation failed");

  // Write pattern to first and last pages to verify full range is accessible
  memset(large, 0xDE, 4096);
  memset((uint8_t *)large + alloc_size - 4096, 0xAD, 4096);

  TEST_ASSERT_EQUAL(0xDE, ((uint8_t *)large)[0]);
  TEST_ASSERT_EQUAL(0xDE, ((uint8_t *)large)[4095]);
  TEST_ASSERT_EQUAL(0xAD, ((uint8_t *)large)[alloc_size - 1]);
  TEST_ASSERT_EQUAL(0xAD, ((uint8_t *)large)[alloc_size - 4096]);

  free(large);
}

// ==================== Concurrent Access ====================

#define CONCURRENT_SIZE 1024
static volatile bool writer_done = false;
static volatile bool reader_done = false;
static volatile bool reader_ok = true;
static uint8_t *shared_psram_buf = NULL;

static void writerTask(void *param) {
  (void)param;
  for (int round = 0; round < 10; round++) {
    memset(shared_psram_buf, round & 0xFF, CONCURRENT_SIZE);
    delay(5);
  }
  writer_done = true;
  vTaskDelete(NULL);
}

static void readerTask(void *param) {
  (void)param;
  delay(2);
  for (int round = 0; round < 20; round++) {
    uint8_t first = shared_psram_buf[0];
    // Verify at least some consistency within a single memset run
    bool consistent = true;
    for (int i = 1; i < CONCURRENT_SIZE; i++) {
      if (shared_psram_buf[i] != first) {
        consistent = false;
        break;
      }
    }
    if (!consistent) {
      reader_ok = false;
    }
    delay(3);
  }
  reader_done = true;
  vTaskDelete(NULL);
}

void test_concurrent_access(void) {
  shared_psram_buf = (uint8_t *)ps_malloc(CONCURRENT_SIZE);
  TEST_ASSERT_NOT_NULL(shared_psram_buf);

  writer_done = false;
  reader_done = false;
  reader_ok = true;

  xTaskCreate(writerTask, "writer", 4096, NULL, 1, NULL);
  xTaskCreate(readerTask, "reader", 4096, NULL, 1, NULL);

  unsigned long start = millis();
  while ((!writer_done || !reader_done) && millis() - start < 5000) {
    delay(10);
  }

  TEST_ASSERT_TRUE(writer_done);
  TEST_ASSERT_TRUE(reader_done);
  TEST_ASSERT_TRUE_MESSAGE(reader_ok, "PSRAM concurrent read saw inconsistent data");

  free(shared_psram_buf);
  shared_psram_buf = NULL;
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
  RUN_TEST(test_heap_caps_spiram);
  RUN_TEST(test_heap_caps_alloc);
  RUN_TEST(test_large_allocation);
  buf = ps_malloc(MAX_TEST_SIZE);
  RUN_TEST(test_memset_all_zeroes);
  RUN_TEST(test_memset_all_ones);
  RUN_TEST(test_memset_alternating);
#ifndef CONFIG_IDF_TARGET_ESP32P4
  // These tests are taking too long on ESP32-P4 in Wokwi
  RUN_TEST(test_memset_random);
  RUN_TEST(test_memcpy);
  RUN_TEST(test_concurrent_access);
#endif
  UNITY_END();
}

void loop() {}
