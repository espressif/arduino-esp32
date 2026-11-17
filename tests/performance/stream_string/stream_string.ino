/*
  StreamString performance benchmark for Arduino and ESP32.

  Measures the throughput of the different StreamString read paths:
  - read() one byte at a time
  - readBytes() in a single call
  - readBytes() in small chunks
  - readString()

  Every benchmark verifies the returned data and that the stream is fully
  consumed.
*/

#include <Arduino.h>
#include <StreamString.h>
#include <esp_task_wdt.h>

// Number of runs to average
#define N_RUNS 3

// Number of bytes read one by one
#define READ_SIZE (4 * 1024)

// Number of bytes used for the readBytes() benchmarks
#define BUFFER_SIZE (16 * 1024)

// Chunk size used by the chunked readBytes() benchmark
#define CHUNK_SIZE (1024)

// Number of bytes read by readString()
#define STRING_SIZE (4 * 1024)

// Keep the task watchdog from firing on the slow (unoptimized) paths
#define TWDT_TIMEOUT_S 30

static volatile uint32_t g_sink = 0;

static inline uint8_t pattern(size_t i) {
  return (uint8_t)((i * 31u + 7u) & 0xFF);
}

static inline uint8_t untilPattern(size_t i) {
  // Never zero, so a zero byte is an unambiguous terminator.
  return (uint8_t)((i % 255u) + 1u);
}

static void wdtRelax() {
  esp_task_wdt_config_t config = {
    .timeout_ms = TWDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = false,
  };
  esp_task_wdt_reconfigure(&config);
}

static void fill(StreamString &s, size_t n) {
  s.reserve(n + 1);
  uint8_t chunk[256];
  for (size_t offset = 0; offset < n;) {
    size_t count = n - offset;
    if (count > sizeof(chunk)) {
      count = sizeof(chunk);
    }
    for (size_t i = 0; i < count; i++) {
      chunk[i] = pattern(offset + i);
    }
    if (s.write(chunk, count) != count) {
      Serial.printf("Error: fill failed at %u\n", (unsigned)offset);
      return;
    }
    offset += count;
  }
}

static void fillUntil(StreamString &s, size_t n) {
  s.reserve(n + 2);
  uint8_t chunk[256];
  for (size_t offset = 0; offset < n;) {
    size_t count = n - offset;
    if (count > sizeof(chunk)) {
      count = sizeof(chunk);
    }
    for (size_t i = 0; i < count; i++) {
      chunk[i] = untilPattern(offset + i);
    }
    if (s.write(chunk, count) != count) {
      Serial.printf("Error: fill failed at %u\n", (unsigned)offset);
      return;
    }
    offset += count;
  }
  const uint8_t terminator = 0;
  s.write(&terminator, 1);
}

static bool checkBuffer(const uint8_t *buffer, size_t n, size_t offset) {
  for (size_t i = 0; i < n; i++) {
    if (buffer[i] != pattern(offset + i)) {
      return false;
    }
  }
  return true;
}

static bool checkUntilBuffer(const uint8_t *buffer, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (buffer[i] != untilPattern(i)) {
      return false;
    }
  }
  return true;
}

static void benchRead() {
  StreamString s;
  fill(s, READ_SIZE);

  uint32_t start = micros();
  uint64_t sum = 0;
  bool ok = true;
  for (size_t i = 0; i < READ_SIZE; i++) {
    int c = s.read();
    if (c < 0 || (uint8_t)c != pattern(i)) {
      ok = false;
      break;
    }
    sum += (uint8_t)c;
  }
  uint32_t elapsed = micros() - start;
  g_sink += (uint32_t)sum;

  Serial.printf("READ n=%u ok=%d us=%u avail=%d\n", (unsigned)READ_SIZE, (int)ok, (unsigned)elapsed, s.available());
}

static void benchReadBytes() {
  StreamString s;
  uint8_t *buffer = (uint8_t *)malloc(BUFFER_SIZE);
  if (!buffer) {
    Serial.println("Error: out of memory");
    return;
  }
  fill(s, BUFFER_SIZE);

  uint32_t start = micros();
  size_t got = s.readBytes((char *)buffer, BUFFER_SIZE);
  uint32_t elapsed = micros() - start;

  bool ok = (got == BUFFER_SIZE) && checkBuffer(buffer, BUFFER_SIZE, 0);
  Serial.printf("READB n=%u ok=%d us=%u avail=%d\n", (unsigned)BUFFER_SIZE, (int)ok, (unsigned)elapsed, s.available());
  free(buffer);
}

static void benchReadBytesChunked() {
  StreamString s;
  uint8_t *buffer = (uint8_t *)malloc(CHUNK_SIZE);
  if (!buffer) {
    Serial.println("Error: out of memory");
    return;
  }
  fill(s, BUFFER_SIZE);

  uint32_t start = micros();
  size_t total = 0;
  bool ok = true;
  while (total < BUFFER_SIZE) {
    size_t wanted = BUFFER_SIZE - total;
    if (wanted > CHUNK_SIZE) {
      wanted = CHUNK_SIZE;
    }
    size_t got = s.readBytes((char *)buffer, wanted);
    if (got != wanted || !checkBuffer(buffer, got, total)) {
      ok = false;
      break;
    }
    total += got;
  }
  uint32_t elapsed = micros() - start;

  Serial.printf("READBC n=%u ok=%d us=%u avail=%d\n", (unsigned)BUFFER_SIZE, (int)ok, (unsigned)elapsed, s.available());
  free(buffer);
}

static void benchReadString() {
  StreamString s;
  fill(s, STRING_SIZE);
  // Measure the read path, not the idle timeout after the data is consumed.
  s.setTimeout(0);

  uint32_t start = micros();
  String result = s.readString();
  uint32_t elapsed = micros() - start;

  bool ok = result.length() == STRING_SIZE;
  for (size_t i = 0; ok && i < STRING_SIZE; i++) {
    if ((uint8_t)result[i] != pattern(i)) {
      ok = false;
    }
  }
  g_sink += result.length();

  Serial.printf("READS n=%u ok=%d us=%u avail=%d\n", (unsigned)STRING_SIZE, (int)ok, (unsigned)elapsed, s.available());
}

static void benchReadBytesUntil() {
  StreamString s;
  fillUntil(s, BUFFER_SIZE);
  // Measure the read path, not the idle timeout after the data is consumed.
  s.setTimeout(0);

  uint8_t *buffer = (uint8_t *)malloc(BUFFER_SIZE + 1);
  if (!buffer) {
    Serial.println("Error: out of memory");
    return;
  }

  uint32_t start = micros();
  // Read one extra byte so the terminator itself is consumed.
  size_t got = s.readBytesUntil(0, (char *)buffer, BUFFER_SIZE + 1);
  uint32_t elapsed = micros() - start;

  bool ok = (got == BUFFER_SIZE) && checkUntilBuffer(buffer, got);
  Serial.printf("READBU n=%u ok=%d us=%u avail=%d\n", (unsigned)got, (int)ok, (unsigned)elapsed, s.available());
  free(buffer);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  wdtRelax();

  log_d("Starting StreamString benchmark");
  Serial.printf("Runs: %u\n", N_RUNS);
  Serial.printf("Read size: %u\n", READ_SIZE);
  Serial.printf("Buffer size: %u\n", BUFFER_SIZE);
  Serial.printf("Chunk size: %u\n", CHUNK_SIZE);
  Serial.printf("String size: %u\n", STRING_SIZE);
  Serial.flush();

  for (int i = 0; i < N_RUNS; i++) {
    Serial.printf("Run %d\n", i);
    Serial.flush();

    benchRead();
    benchReadBytes();
    benchReadBytesChunked();
    benchReadString();
    benchReadBytesUntil();

    Serial.flush();
  }

  Serial.printf("Sink: %u\n", (unsigned)g_sink);
  log_d("StreamString benchmark done");
}

void loop() {
  vTaskDelete(NULL);
}
