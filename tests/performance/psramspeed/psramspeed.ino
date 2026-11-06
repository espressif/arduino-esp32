/*
  Based on the ramspeed test from NuttX.
  https://github.com/apache/nuttx-apps/blob/master/benchmarks/ramspeed/ramspeed_main.c
  Modified for Arduino and ESP32 by Lucas Saavedra Vaz, 2024
*/

#include <Arduino.h>

#if __has_include("esp_cache.h")
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#endif

// Test settings

// Number of runs to average
#define N_RUNS 3

// Value to fill the memory with
#define FILL_VALUE 0x00

// Number of copies to be performed in each test
#define N_COPIES 400

// Start size for the tests. Value must be a power of 2.
// Using 64KiB to avoid cache effects.
#define START_SIZE (64 * 1024)  // 64KiB

// Max size to be copied. Must be bigger than 32 and it will be floored to the nearest power of 2
#define MAX_TEST_SIZE (512 * 1024)  // 512KiB

// Implementation macros

#if defined(UINTPTR_MAX) && UINTPTR_MAX > 0xFFFFFFFF
#define MEM_UNIT   uint64_t
#define ALIGN_MASK 0x7
#else
#define MEM_UNIT   uint32_t
#define ALIGN_MASK 0x3
#endif

#define COPY32 \
  *d32 = *s32; \
  d32++;       \
  s32++;
#define COPY8 \
  *d8 = *s8;  \
  d8++;       \
  s8++;
#define SET32(x) \
  *d32 = x;      \
  d32++;
#define SET8(x) \
  *d8 = x;      \
  d8++;
#define REPEAT8(expr) expr expr expr expr expr expr expr expr

static size_t s_cache_line_size = 0;

/* Invalidate data cache for region so the next timed access hits actual PSRAM, not cache.
   addr must be cache-line aligned (guaranteed by heap_caps_aligned_alloc). */
static inline void invalidate_cache_region(void *addr, size_t size) {
#if __has_include("esp_cache.h")
  if (size == 0 || s_cache_line_size == 0) {
    return;
  }
  size_t aligned_size = (size + s_cache_line_size - 1) & ~(s_cache_line_size - 1);
  esp_cache_msync(addr, aligned_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
#else
  (void)addr;
  (void)size;
#endif
}

/* Functions */

static void *mock_memcpy(void *dst, const void *src, size_t len) {
  uint8_t *d8 = (uint8_t *)dst;
  const uint8_t *s8 = (uint8_t *)src;

  uintptr_t d_align = (uintptr_t)d8 & ALIGN_MASK;
  uintptr_t s_align = (uintptr_t)s8 & ALIGN_MASK;
  uint32_t *d32;
  const uint32_t *s32;

  /* Byte copy for unaligned memories */

  if (s_align != d_align) {
    while (len > 32) {
      REPEAT8(COPY8);
      REPEAT8(COPY8);
      REPEAT8(COPY8);
      REPEAT8(COPY8);
      len -= 32;
    }

    while (len) {
      COPY8;
      len--;
    }

    return dst;
  }

  /* Make the memories aligned */

  if (d_align) {
    d_align = ALIGN_MASK + 1 - d_align;
    while (d_align && len) {
      COPY8;
      d_align--;
      len--;
    }
  }

  d32 = (uint32_t *)d8;
  s32 = (uint32_t *)s8;
  while (len > 32) {
    REPEAT8(COPY32);
    len -= 32;
  }

  while (len > 4) {
    COPY32;
    len -= 4;
  }

  d8 = (uint8_t *)d32;
  s8 = (const uint8_t *)s32;
  while (len) {
    COPY8;
    len--;
  }

  return dst;
}

static void mock_memset(void *dst, uint8_t v, size_t len) {
  uint8_t *d8 = (uint8_t *)dst;
  uintptr_t d_align = (uintptr_t)d8 & ALIGN_MASK;
  uint32_t v32;
  uint32_t *d32;

  /* Make the address aligned */

  if (d_align) {
    d_align = ALIGN_MASK + 1 - d_align;
    while (d_align && len) {
      SET8(v);
      len--;
      d_align--;
    }
  }

  v32 = (uint32_t)v + ((uint32_t)v << 8) + ((uint32_t)v << 16) + ((uint32_t)v << 24);

  d32 = (uint32_t *)d8;

  while (len > 32) {
    REPEAT8(SET32(v32));
    len -= 32;
  }

  while (len > 4) {
    SET32(v32);
    len -= 4;
  }

  d8 = (uint8_t *)d32;
  while (len) {
    SET8(v);
    len--;
  }
}

static void print_rate(const char *name, uint64_t bytes, unsigned long cost_time_us) {
  uint32_t rate;
  if (cost_time_us == 0) {
    Serial.println("Error: Too little time taken, please increase N_COPIES");
    return;
  }

  rate = (bytes / 1024ULL) * 1000000ULL / cost_time_us;
  Serial.printf("%s Rate = %" PRIu32 " KiB/s Time: %lu us\n", name, rate, cost_time_us);
}

static void memcpy_speed_test(void *dest, const void *src, size_t size, uint32_t repeat_cnt) {
  unsigned long start_time;
  unsigned long cost_time_system;
  unsigned long cost_time_mock;
  uint32_t cnt;
  uint32_t step;
  uint64_t total_size;

  for (step = START_SIZE; step <= size; step <<= 1) {
    total_size = (uint64_t)step * (uint64_t)repeat_cnt;

    Serial.printf("Memcpy %" PRIu32 " Bytes test\n", step);

    invalidate_cache_region((void *)src, step);
    invalidate_cache_region(dest, step);
    start_time = micros();

    for (cnt = 0; cnt < repeat_cnt; cnt++) {
      memcpy(dest, src, step);
    }

    cost_time_system = micros() - start_time;

    invalidate_cache_region((void *)src, step);
    invalidate_cache_region(dest, step);
    start_time = micros();

    for (cnt = 0; cnt < repeat_cnt; cnt++) {
      mock_memcpy(dest, src, step);
    }

    cost_time_mock = micros() - start_time;

    print_rate("System memcpy():", total_size, cost_time_system);
    print_rate("Mock memcpy():", total_size, cost_time_mock);
  }
}

static void memset_speed_test(void *dest, uint8_t value, size_t size, uint32_t repeat_num) {
  unsigned long start_time;
  unsigned long cost_time_system;
  unsigned long cost_time_mock;
  uint32_t cnt;
  uint32_t step;
  uint64_t total_size;

  for (step = START_SIZE; step <= size; step <<= 1) {
    total_size = (uint64_t)step * (uint64_t)repeat_num;

    Serial.printf("Memset %" PRIu32 " Bytes test\n", step);

    invalidate_cache_region(dest, step);
    start_time = micros();

    for (cnt = 0; cnt < repeat_num; cnt++) {
      memset(dest, value, step);
    }

    cost_time_system = micros() - start_time;

    invalidate_cache_region(dest, step);
    start_time = micros();

    for (cnt = 0; cnt < repeat_num; cnt++) {
      mock_memset(dest, value, step);
    }

    cost_time_mock = micros() - start_time;

    print_rate("System memset():", total_size, cost_time_system);
    print_rate("Mock memset():", total_size, cost_time_mock);
  }
}

/* Main */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  size_t alloc_align = sizeof(uint32_t);
#if __has_include("esp_cache.h")
  size_t cache_align = 0;
  if (esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &cache_align) == ESP_OK && cache_align > 0) {
    alloc_align = cache_align;
    // Real cache lines are >= 16 bytes (16, 32, 64, 128).
    // ESP32 reports 4 (bus width) but doesn't support per-address
    // invalidation (cache_hal_invalidate_addr calls abort()).
    if (cache_align >= 16) {
      s_cache_line_size = cache_align;
    }
  }
#endif
  void *dest = heap_caps_aligned_alloc(alloc_align, MAX_TEST_SIZE, MALLOC_CAP_SPIRAM);
  const void *src = heap_caps_aligned_alloc(alloc_align, MAX_TEST_SIZE, MALLOC_CAP_SPIRAM);

  if (!dest || !src) {
    Serial.println("Memory allocation failed");
    return;
  }

  if (s_cache_line_size == 0) {
    log_w("Cache invalidation not available on this SoC. Results may be influenced by CPU cache.");
  }

  log_d("Starting PSRAM speed test");
  Serial.printf("Runs: %u\n", N_RUNS);
  Serial.printf("Copies: %u\n", N_COPIES);
  Serial.printf("Max test size: %u\n", MAX_TEST_SIZE);
  Serial.flush();
  for (int i = 0; i < N_RUNS; i++) {
    Serial.printf("Run %u\n", i);
    memcpy_speed_test(dest, src, MAX_TEST_SIZE, N_COPIES);
    Serial.flush();
    memset_speed_test(dest, FILL_VALUE, MAX_TEST_SIZE, N_COPIES);
    Serial.flush();
  }
  log_d("PSRAM speed test done");
}

void loop() {
  vTaskDelete(NULL);
}
