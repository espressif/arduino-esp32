/*
 * Clock Validation Test (hardware only)
 *
 * Neither Wokwi nor QEMU model the clock tree, so the whole suite runs on hardware.
 *
 * Covers:
 *   getSupportedCpuFrequencyMhz(): shape of the list, and that it agrees with setCpuFrequencyMhz()
 *   setCpuFrequencyMhz(): every advertised frequency is applied exactly, everything else refused
 *   getClockSourceName(): names every clock source the chip can select, rejects invalid input
 *   getApbFrequency(): matches the frequency the HAL derives from the dividers in hardware
 *   SPI: the configured bit rate does not drift when the CPU frequency changes
 *
 * The console runs at every frequency under test, so a frequency that breaks the UART shows up
 * as garbled or missing output rather than as a passing test.
 */

#include <Arduino.h>
#include <unity.h>
#include <SPI.h>
#include "soc/rtc.h"
#include "hal/clk_tree_hal.h"

// Enough room for the longest list any target can report, plus a couple of spare slots
#define MAX_SUPPORTED_FREQS 32

#define SPI_TEST_FREQ_HZ 1000000
#define SPI_TOLERANCE_HZ (SPI_TEST_FREQ_HZ / 10)
#define SPI_TEST_BYTES   2048  // 16 ms at 1 MHz, long enough for the software overhead to vanish

static uint32_t supported[MAX_SUPPORTED_FREQS];
static size_t supported_count = 0;
static uint32_t initial_freq_mhz = 0;

void setUp(void) {}

void tearDown(void) {
  setCpuFrequencyMhz(initial_freq_mhz);
}

static bool isSupported(uint32_t freq_mhz) {
  for (size_t i = 0; i < supported_count; i++) {
    if (supported[i] == freq_mhz) {
      return true;
    }
  }
  return false;
}

// Turns "240, 160, 80 MHz" into the supported[] array
static void parseSupportedFrequencies(void) {
  const char *list = getSupportedCpuFrequencyMhz();
  TEST_ASSERT_NOT_NULL(list);

  supported_count = 0;
  for (const char *p = list; *p != '\0'; p++) {
    if (*p < '0' || *p > '9') {
      continue;
    }
    TEST_ASSERT_LESS_THAN_MESSAGE(MAX_SUPPORTED_FREQS, supported_count, "More frequencies reported than this test can hold");
    char *end = NULL;
    supported[supported_count++] = strtoul(p, &end, 10);
    p = end - 1;  // end is the first character strtoul did not consume, which the loop steps over
  }
}

// ==================== Supported frequency list ====================

void test_supported_frequency_list(void) {
  Serial.printf("Supported: %s\n", getSupportedCpuFrequencyMhz());
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, supported_count, "No supported CPU frequency reported");

  for (size_t i = 0; i < supported_count; i++) {
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, supported[i], "Reported a frequency of 0 MHz");
    if (i > 0) {
      TEST_ASSERT_GREATER_THAN_MESSAGE(supported[i], supported[i - 1], "List is not in descending order");
    }
  }

  // The frequency the chip is running at has to be one the list offers
  TEST_ASSERT_TRUE_MESSAGE(isSupported(initial_freq_mhz), "The current CPU frequency is missing from the list");
}

// ==================== setCpuFrequencyMhz ====================

void test_every_supported_frequency_applies(void) {
  for (size_t i = 0; i < supported_count; i++) {
    TEST_ASSERT_TRUE_MESSAGE(setCpuFrequencyMhz(supported[i]), "A frequency from the list was refused");
    TEST_ASSERT_EQUAL_MESSAGE(supported[i], getCpuFrequencyMhz(), "The applied frequency differs from the requested one");
    Serial.printf("  %3" PRIu32 " MHz ok\n", supported[i]);
    Serial.flush();
  }
}

void test_unsupported_frequencies_are_refused(void) {
  TEST_ASSERT_FALSE_MESSAGE(setCpuFrequencyMhz(0), "0 MHz was accepted");

  // Well above the maximum of every target, including the 400 MHz ESP32-P4
  TEST_ASSERT_FALSE_MESSAGE(setCpuFrequencyMhz(1000), "1000 MHz was accepted");

  // Anything the list leaves out has to be refused, otherwise the list is not the contract
  size_t checked = 0;
  for (uint32_t freq_mhz = supported[0]; freq_mhz > 0 && checked < 8; freq_mhz--) {
    if (isSupported(freq_mhz)) {
      continue;
    }
    TEST_ASSERT_FALSE_MESSAGE(setCpuFrequencyMhz(freq_mhz), "A frequency missing from the list was accepted");
    TEST_ASSERT_EQUAL_MESSAGE(initial_freq_mhz, getCpuFrequencyMhz(), "A refused frequency still changed the CPU frequency");
    checked++;
  }
}

// ==================== getClockSourceName ====================

void test_clock_source_names(void) {
  rtc_cpu_freq_config_t conf;

  // Every frequency in the list, so that each selectable clock source gets named at least once
  for (size_t i = 0; i < supported_count; i++) {
    TEST_ASSERT_TRUE(setCpuFrequencyMhz(supported[i]));
    rtc_clk_cpu_freq_get_config(&conf);

    const char *name = getClockSourceName(conf.source);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, strlen(name), "Clock source name is empty");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, strcmp(name, "Invalid"), "Clock source in use has no name");
    Serial.printf("  %3" PRIu32 " MHz from %s\n", supported[i], name);
    Serial.flush();
  }

  // Out of range input must not be indexed into whatever table backs the names
  TEST_ASSERT_EQUAL_STRING("Invalid", getClockSourceName(SOC_CPU_CLK_SRC_INVALID));
  TEST_ASSERT_EQUAL_STRING("Invalid", getClockSourceName(UINT8_MAX));
}

// ==================== getApbFrequency ====================

void test_apb_frequency_matches_hardware(void) {
  for (size_t i = 0; i < supported_count; i++) {
    TEST_ASSERT_TRUE(setCpuFrequencyMhz(supported[i]));

    // clk_hal_apb_get_freq_hz() derives APB_CLK from the dividers the IDF programmed, so it is
    // an independent view of what the bus is really running at
    const uint32_t expected = clk_hal_apb_get_freq_hz();
    Serial.printf("  %3" PRIu32 " MHz cpu -> %" PRIu32 " Hz apb\n", supported[i], expected);
    Serial.flush();
    TEST_ASSERT_EQUAL_MESSAGE(expected, getApbFrequency(), "getApbFrequency() disagrees with the hardware dividers");
  }
}

// ==================== SPI ====================

// A divider is only as good as the source frequency it was computed from, and a wrong source is
// invisible to the core's own arithmetic, so time a transfer and compare it with the request. Only
// at the frequency the chip booted at: at low CPU frequencies the software cost of refilling the
// FIFO dominates the measurement.
void test_spi_bitrate_matches_request(void) {
  static uint8_t data[SPI_TEST_BYTES];
  memset(data, 0xA5, sizeof(data));

  SPI.begin();
  SPI.setFrequency(SPI_TEST_FREQ_HZ);

  const uint32_t start = micros();
  SPI.writeBytes(data, sizeof(data));
  const uint32_t elapsed_us = micros() - start;
  SPI.end();

  TEST_ASSERT_GREATER_THAN_MESSAGE(0, elapsed_us, "Transfer took no measurable time");
  const uint32_t measured = (uint32_t)((uint64_t)sizeof(data) * 8 * 1000000 / elapsed_us);
  Serial.printf("  requested %u Hz, measured %" PRIu32 " bit/s over %" PRIu32 " us\n", SPI_TEST_FREQ_HZ, measured, elapsed_us);
  Serial.flush();
  TEST_ASSERT_UINT32_WITHIN_MESSAGE(SPI_TOLERANCE_HZ, SPI_TEST_FREQ_HZ, measured, "Measured SPI bit rate does not match the requested frequency");
}

void test_spi_frequency_survives_cpu_change(void) {
  SPI.begin();
  SPI.setFrequency(SPI_TEST_FREQ_HZ);

  const uint32_t configured = spiClockDivToFrequency(SPI.bus(), spiGetClockDiv(SPI.bus()));
  TEST_ASSERT_UINT32_WITHIN_MESSAGE(SPI_TOLERANCE_HZ, SPI_TEST_FREQ_HZ, configured, "SPI did not start at the requested frequency");

  for (size_t i = 0; i < supported_count; i++) {
    TEST_ASSERT_TRUE(setCpuFrequencyMhz(supported[i]));

    const uint32_t freq = spiClockDivToFrequency(SPI.bus(), spiGetClockDiv(SPI.bus()));
    Serial.printf("  %3" PRIu32 " MHz cpu -> %" PRIu32 " Hz spi\n", supported[i], freq);
    Serial.flush();
    TEST_ASSERT_UINT32_WITHIN_MESSAGE(SPI_TOLERANCE_HZ, SPI_TEST_FREQ_HZ, freq, "SPI frequency drifted with the CPU frequency");
  }

  SPI.end();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  initial_freq_mhz = getCpuFrequencyMhz();
  Serial.printf("Booted at %" PRIu32 " MHz, XTAL %" PRIu32 " MHz\n", initial_freq_mhz, getXtalFrequencyMhz());

  UNITY_BEGIN();
  parseSupportedFrequencies();
  RUN_TEST(test_supported_frequency_list);
  RUN_TEST(test_every_supported_frequency_applies);
  RUN_TEST(test_unsupported_frequencies_are_refused);
  RUN_TEST(test_clock_source_names);
  RUN_TEST(test_apb_frequency_matches_hardware);
  RUN_TEST(test_spi_bitrate_matches_request);
  RUN_TEST(test_spi_frequency_survives_cpu_change);
  UNITY_END();
}

void loop() {}
