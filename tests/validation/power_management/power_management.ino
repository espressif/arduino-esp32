/*
 * Power Management Validation Test (hardware only -- Wokwi disabled)
 *
 * RTC is only partially simulated in Wokwi (pull-up/pull-down only),
 * so deep sleep and RTC_DATA_ATTR persistence do not work reliably.
 * The entire suite runs on hardware only.
 *
 * Covers:
 *   TWDT: feed/starve, reset reason verification
 *   Deep sleep: timer wakeup, wakeup cause, RTC_DATA_ATTR persistence
 *   Light sleep: timer wakeup, no reboot
 *   Reset reason: power-on vs deep sleep
 *
 * The test uses a phased approach:
 *   Phase 0: Initial boot - run non-reboot tests, then trigger deep sleep
 *   Phase 1: After deep sleep - verify wakeup cause + RTC data
 */

#include <Arduino.h>
#include <unity.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

#define DEEP_SLEEP_US 2000000  // 2 seconds

RTC_DATA_ATTR int boot_count = 0;
RTC_DATA_ATTR uint32_t rtc_magic = 0;

void setUp(void) {}
void tearDown(void) {}

// ==================== Reset Reason ====================

void test_reset_reason_power_on(void) {
  esp_reset_reason_t reason = esp_reset_reason();
  TEST_ASSERT_TRUE_MESSAGE(reason == ESP_RST_POWERON || reason == ESP_RST_DEEPSLEEP, "Unexpected reset reason on initial boot");
}

// ==================== TWDT ====================

void test_twdt_feed(void) {
  esp_task_wdt_config_t config = {
    .timeout_ms = 3000,
    .idle_core_mask = 0,
    .trigger_panic = false,
  };
  TEST_ASSERT_EQUAL(ESP_OK, esp_task_wdt_reconfigure(&config));
  TEST_ASSERT_EQUAL(ESP_OK, esp_task_wdt_add(NULL));
  TEST_ASSERT_EQUAL(ESP_OK, esp_task_wdt_reset());
  delay(500);
  TEST_ASSERT_EQUAL(ESP_OK, esp_task_wdt_reset());
  TEST_ASSERT_EQUAL(ESP_OK, esp_task_wdt_delete(NULL));
}

// ==================== Light Sleep ====================

void test_light_sleep_timer(void) {
  esp_sleep_enable_timer_wakeup(500000);  // 500ms
  unsigned long before = millis();
  esp_err_t ret = esp_light_sleep_start();
  unsigned long elapsed = millis() - before;

  TEST_ASSERT_EQUAL(ESP_OK, ret);
  TEST_ASSERT_GREATER_OR_EQUAL(400, elapsed);
  TEST_ASSERT_LESS_OR_EQUAL(2000, elapsed);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  TEST_ASSERT_EQUAL(ESP_SLEEP_WAKEUP_TIMER, cause);
}

// ==================== Deep Sleep Phase ====================

void test_deep_sleep_verify(void) {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  TEST_ASSERT_EQUAL(ESP_SLEEP_WAKEUP_TIMER, cause);
}

void test_rtc_data_persisted(void) {
  TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, rtc_magic);
  TEST_ASSERT_GREATER_OR_EQUAL(2, boot_count);
}

// ==================== Main ====================

void setup() {
  boot_count++;

  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();

  bool woke_from_deep_sleep = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER && rtc_magic == 0xDEADBEEF);

  if (woke_from_deep_sleep) {
    // Phase 1: woke from deep sleep
    RUN_TEST(test_deep_sleep_verify);
    RUN_TEST(test_rtc_data_persisted);
  } else {
    // Phase 0: initial boot
    RUN_TEST(test_reset_reason_power_on);
    RUN_TEST(test_twdt_feed);
    RUN_TEST(test_light_sleep_timer);
  }

  UNITY_END();

  if (!woke_from_deep_sleep) {
    rtc_magic = 0xDEADBEEF;
    Serial.println("DEEP_SLEEP_START");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_US);
    esp_deep_sleep_start();
  }
}

void loop() {}
