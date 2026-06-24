/*
  Comprehensive SPI Validation Test

  Uses a Wokwi custom SPI echo chip. The chip echoes back in the CURRENT
  CS transaction the exact bytes it received during the PREVIOUS CS transaction.
  All tests therefore follow a two-step pattern:
    1. Prime  – send payload; chip stores it (master ignores received bytes).
    2. Echo   – send dummy zeros; chip returns the stored payload for verification.

  API surface exercised:
    begin / end
    transfer(uint8_t), transfer16, transfer32
    transfer(void*, size)          – in-place buffer
    transferBytes(in, out, size)   – separate in/out buffers
    write, write16, write32
    writeBytes, writePixels, writePattern
    beginTransaction / endTransaction (SPISettings)
    setFrequency / getClockDivider
    pinSS
    Stress: 256-byte transfer
*/

#include <Arduino.h>
#include <SPI.h>
#include <unity.h>

// ── helpers ────────────────────────────────────────────────────────────────

static void spi_cs_transfer(const uint8_t *tx, uint8_t *rx, size_t len) {
  digitalWrite(SS, LOW);
  SPI.transferBytes(tx, rx, len);
  digitalWrite(SS, HIGH);
}

// Send payload → chip stores it; discard what chip sent back.
static void spi_prime(const uint8_t *data, size_t len) {
  static uint8_t dummy[256];
  spi_cs_transfer(data, dummy, len);
}

// Send zeros → receive the echo of the previously primed payload.
static void spi_echo(uint8_t *recv, size_t len) {
  static const uint8_t zeros[256] = {};
  spi_cs_transfer(zeros, recv, len);
}

// ── setUp / tearDown ────────────────────────────────────────────────────────

void setUp(void) {
  SPI.begin();
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
}

void tearDown(void) {
  digitalWrite(SS, HIGH);
  SPI.end();
}

// ── tests ──────────────────────────────────────────────────────────────────

void test_begin_end(void) {
  // begin() was already called in setUp; call end() + begin() to exercise both.
  SPI.end();
  TEST_ASSERT_TRUE(SPI.begin());
  // tearDown() will call SPI.end() again, which is a safe no-op here.
}

void test_transfer_byte(void) {
  const uint8_t tx = 0xA5;
  uint8_t rx;

  // Prime
  digitalWrite(SS, LOW);
  SPI.transfer(tx);
  digitalWrite(SS, HIGH);

  // Echo
  digitalWrite(SS, LOW);
  rx = SPI.transfer(0x00);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_HEX8(tx, rx);
}

void test_transfer16(void) {
  const uint16_t tx = 0x1234;
  uint16_t rx;

  digitalWrite(SS, LOW);
  SPI.transfer16(tx);
  digitalWrite(SS, HIGH);

  digitalWrite(SS, LOW);
  rx = SPI.transfer16(0x0000);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_HEX16(tx, rx);
}

void test_transfer32(void) {
  const uint32_t tx = 0xDEADBEEF;
  uint32_t rx;

  digitalWrite(SS, LOW);
  SPI.transfer32(tx);
  digitalWrite(SS, HIGH);

  digitalWrite(SS, LOW);
  rx = SPI.transfer32(0x00000000);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_HEX32(tx, rx);
}

void test_transfer_buffer(void) {
  // transfer(void*, size) does an in-place exchange.
  const uint8_t original[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  const size_t len = sizeof(original);
  uint8_t buf[sizeof(original)];

  // Prime: buf holds original; after transfer buf contains chip's reply (zeros).
  memcpy(buf, original, len);
  digitalWrite(SS, LOW);
  SPI.transfer(buf, len);
  digitalWrite(SS, HIGH);

  // Echo: zero buf, then transfer again so buf receives the original data back.
  memset(buf, 0, len);
  digitalWrite(SS, LOW);
  SPI.transfer(buf, len);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_MEMORY(original, buf, len);
}

void test_transfer_bytes(void) {
  const uint8_t tx[] = {0xAA, 0xBB, 0xCC, 0xDD};
  const size_t len = sizeof(tx);
  uint8_t rx[sizeof(tx)];

  spi_prime(tx, len);
  spi_echo(rx, len);

  TEST_ASSERT_EQUAL_MEMORY(tx, rx, len);
}

void test_write_byte(void) {
  const uint8_t data = 0x7E;
  uint8_t rx;

  digitalWrite(SS, LOW);
  SPI.write(data);
  digitalWrite(SS, HIGH);

  digitalWrite(SS, LOW);
  rx = SPI.transfer(0x00);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_HEX8(data, rx);
}

void test_write16(void) {
  const uint16_t data = 0xBEEF;
  uint16_t rx;

  digitalWrite(SS, LOW);
  SPI.write16(data);
  digitalWrite(SS, HIGH);

  digitalWrite(SS, LOW);
  rx = SPI.transfer16(0x0000);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_HEX16(data, rx);
}

void test_write32(void) {
  const uint32_t data = 0xCAFEBABE;
  uint32_t rx;

  digitalWrite(SS, LOW);
  SPI.write32(data);
  digitalWrite(SS, HIGH);

  digitalWrite(SS, LOW);
  rx = SPI.transfer32(0x00000000);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_HEX32(data, rx);
}

void test_write_bytes(void) {
  const uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  const size_t len = sizeof(data);
  uint8_t rx[sizeof(data)];

  digitalWrite(SS, LOW);
  SPI.writeBytes(data, len);
  digitalWrite(SS, HIGH);

  spi_echo(rx, len);

  TEST_ASSERT_EQUAL_MEMORY(data, rx, len);
}

void test_write_pixels(void) {
  // writePixels swaps bytes within each 16-bit pixel (ili9341-style).
  // Use a 4-byte (2-pixel) buffer: {0x12, 0x34, 0x56, 0x78}.
  // After the per-pixel byte-swap the wire order is {0x34, 0x12, 0x78, 0x56}.
  const uint8_t pixels[] = {0x12, 0x34, 0x56, 0x78};
  const uint8_t expected_wire[] = {0x34, 0x12, 0x78, 0x56};
  const size_t len = sizeof(pixels);
  uint8_t rx[sizeof(pixels)];

  digitalWrite(SS, LOW);
  SPI.writePixels(pixels, len);
  digitalWrite(SS, HIGH);

  spi_echo(rx, len);

  TEST_ASSERT_EQUAL_MEMORY(expected_wire, rx, len);
}

void test_write_pattern(void) {
  const uint8_t pattern[] = {0xAA, 0x55};
  const uint32_t repeat = 3;
  const size_t total = sizeof(pattern) * repeat;  // 6 bytes
  const uint8_t expected[] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
  uint8_t rx[6];

  digitalWrite(SS, LOW);
  SPI.writePattern(pattern, sizeof(pattern), repeat);
  digitalWrite(SS, HIGH);

  spi_echo(rx, total);

  TEST_ASSERT_EQUAL_MEMORY(expected, rx, total);
}

void test_begin_transaction(void) {
  const uint8_t tx[] = {0xDE, 0xAD, 0xBE, 0xEF};
  const size_t len = sizeof(tx);
  uint8_t rx[sizeof(tx)];
  static const uint8_t zeros[sizeof(tx)] = {};

  // Prime inside a transaction.
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  spi_prime(tx, len);
  SPI.endTransaction();

  // Echo inside another transaction.
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  spi_cs_transfer(zeros, rx, len);
  SPI.endTransaction();

  TEST_ASSERT_EQUAL_MEMORY(tx, rx, len);
}

void test_set_frequency(void) {
  const uint8_t tx = 0x42;
  uint8_t rx;

  // Lower the clock and grab the divider.
  SPI.setFrequency(500000);
  uint32_t div_slow = SPI.getClockDivider();
  TEST_ASSERT_NOT_EQUAL(0, div_slow);

  // Prime at 500 kHz.
  digitalWrite(SS, LOW);
  SPI.transfer(tx);
  digitalWrite(SS, HIGH);

  // Raise the clock and verify the divider changed.
  SPI.setFrequency(4000000);
  uint32_t div_fast = SPI.getClockDivider();
  TEST_ASSERT_NOT_EQUAL(div_slow, div_fast);

  // Echo at 4 MHz – data integrity must be preserved regardless of speed.
  digitalWrite(SS, LOW);
  rx = SPI.transfer(0x00);
  digitalWrite(SS, HIGH);

  TEST_ASSERT_EQUAL_HEX8(tx, rx);
}

void test_pin_ss(void) {
  TEST_ASSERT_EQUAL(SS, SPI.pinSS());
}

void test_stress(void) {
  // 256-byte transfer: send an incrementing pattern and verify the echo.
  static uint8_t tx[256], rx[256];
  static const uint8_t zeros[256] = {};

  for (int i = 0; i < 256; i++) {
    tx[i] = (uint8_t)i;
  }

  spi_prime(tx, 256);
  spi_cs_transfer(zeros, rx, 256);

  for (int i = 0; i < 256; i++) {
    TEST_ASSERT_EQUAL_HEX8(tx[i], rx[i]);
  }
}

// ── entry points ────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);

  UNITY_BEGIN();
  RUN_TEST(test_begin_end);
  RUN_TEST(test_transfer_byte);
  RUN_TEST(test_transfer16);
  RUN_TEST(test_transfer32);
  RUN_TEST(test_transfer_buffer);
  RUN_TEST(test_transfer_bytes);
  RUN_TEST(test_write_byte);
  RUN_TEST(test_write16);
  RUN_TEST(test_write32);
  RUN_TEST(test_write_bytes);
  RUN_TEST(test_write_pixels);
  RUN_TEST(test_write_pattern);
  RUN_TEST(test_begin_transaction);
  RUN_TEST(test_set_frequency);
  RUN_TEST(test_pin_ss);
  RUN_TEST(test_stress);
  UNITY_END();
}

void loop() {
  vTaskDelete(NULL);
}

