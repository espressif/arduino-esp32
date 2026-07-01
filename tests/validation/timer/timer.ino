/* HW Timer test */
#include <Arduino.h>
#include <atomic>
#include <unity.h>

#define TIMER_FREQUENCY          4000000
#define TIMER_FREQUENCY_XTAL_CLK 1000

/*
 * ESP32 - APB clk only (1kHz not possible)
 * C3 - APB + XTAL clk
 * S2 - APB + XTAL clk
 * S3 - APB + XTAL clk
 */

static hw_timer_t *timer = NULL;
static volatile bool alarm_flag;

/* setUp / tearDown functions are intended to be called before / after each test. */
void setUp(void) {
  timer = timerBegin(TIMER_FREQUENCY);
  if (timer == NULL) {
    TEST_FAIL_MESSAGE("Timer init failed in setUp()");
  }
  timerStop(timer);
  timerRestart(timer);
}

void tearDown(void) {
  timerEnd(timer);
}

void ARDUINO_ISR_ATTR onTimer() {
  alarm_flag = true;
}

void timer_interrupt_test(void) {

  alarm_flag = false;
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, (1.2 * TIMER_FREQUENCY), true, 0);
  timerStart(timer);

  delay(2000);

  TEST_ASSERT_EQUAL(true, alarm_flag);

  timerStop(timer);
  timerRestart(timer);
  alarm_flag = false;
  timerDetachInterrupt(timer);
  timerStart(timer);

  delay(2000);
  TEST_ASSERT_EQUAL(false, alarm_flag);
}

void timer_divider_test(void) {

  uint64_t time_val;
  uint64_t comp_time_val;

  timerStart(timer);

  delay(1000);
  time_val = timerRead(timer);

  // compare divider  16 and 8, value should be double
  timerEnd(timer);

  timer = timerBegin(2 * TIMER_FREQUENCY);
  if (timer == NULL) {
    TEST_FAIL_MESSAGE("Timer init failed!");
  }
  timerRestart(timer);
  delay(1000);
  comp_time_val = timerRead(timer);

  TEST_ASSERT_INT_WITHIN(4000, 4000000, time_val);
  TEST_ASSERT_INT_WITHIN(8000, 8000000, comp_time_val);

  // divider is 256, value should be 2^4
  timerEnd(timer);

  timer = timerBegin(TIMER_FREQUENCY / 16);
  if (timer == NULL) {
    TEST_FAIL_MESSAGE("Timer init failed!");
  }
  timerRestart(timer);
  delay(1000);
  comp_time_val = timerRead(timer);

  TEST_ASSERT_INT_WITHIN(4000, 4000000, time_val);
  TEST_ASSERT_INT_WITHIN(2500, 250000, comp_time_val);
}

void timer_read_test(void) {

  uint64_t set_timer_val = 0xFF;
  uint64_t get_timer_val = 0;

  timerWrite(timer, set_timer_val);
  get_timer_val = timerRead(timer);

  TEST_ASSERT_EQUAL(set_timer_val, get_timer_val);
}

void timer_clock_select_test(void) {
  // Set timer frequency that can be achieved using XTAL clock source (autoselected)
  timer = timerBegin(TIMER_FREQUENCY_XTAL_CLK);

  uint32_t resolution = timerGetFrequency(timer);
  TEST_ASSERT_EQUAL(TIMER_FREQUENCY_XTAL_CLK, resolution);
}

// ==================== One-Shot Alarm ====================

static std::atomic<int> oneshot_count{0};

void ARDUINO_ISR_ATTR onOneShotTimer() {
  oneshot_count.fetch_add(1, std::memory_order_relaxed);
}

void timer_oneshot_test(void) {
  oneshot_count = 0;
  timerAttachInterrupt(timer, &onOneShotTimer);
  // One-shot: count=1 means fire once then stop
  timerAlarm(timer, TIMER_FREQUENCY, false, 1);
  timerStart(timer);

  delay(2000);

  TEST_ASSERT_EQUAL(1, oneshot_count.load(std::memory_order_relaxed));
  timerDetachInterrupt(timer);
}

// ==================== Auto-Reload Count ====================

static std::atomic<int> reload_count{0};

void ARDUINO_ISR_ATTR onReloadTimer() {
  reload_count.fetch_add(1, std::memory_order_relaxed);
}

void timer_auto_reload_test(void) {
  reload_count = 0;
  timerAttachInterrupt(timer, &onReloadTimer);
  // Auto-reload (true), unlimited (0) -- fires every 0.25s at 4MHz frequency
  timerAlarm(timer, TIMER_FREQUENCY / 4, true, 0);
  timerStart(timer);

  delay(1100);
  timerStop(timer);

  // In ~1.1s with 0.25s period, expect ~4 fires
  TEST_ASSERT_GREATER_OR_EQUAL(3, reload_count.load(std::memory_order_relaxed));
  TEST_ASSERT_LESS_OR_EQUAL(6, reload_count.load(std::memory_order_relaxed));
  timerDetachInterrupt(timer);
}

// ==================== Stop / Start ====================

void timer_stop_start_test(void) {
  timerStart(timer);
  delay(100);
  uint64_t val1 = timerRead(timer);
  TEST_ASSERT_GREATER_THAN(0, val1);

  timerStop(timer);
  uint64_t val_stopped = timerRead(timer);
  delay(100);
  uint64_t val_after = timerRead(timer);
  // Timer should not advance while stopped
  TEST_ASSERT_EQUAL(val_stopped, val_after);

  timerStart(timer);
  delay(100);
  uint64_t val_resumed = timerRead(timer);
  TEST_ASSERT_GREATER_THAN(val_after, val_resumed);
}

// ==================== Multiple Timers ====================

void timer_multiple_test(void) {
  // End the default timer from setUp
  timerEnd(timer);

  hw_timer_t *t1 = timerBegin(TIMER_FREQUENCY);
  hw_timer_t *t2 = timerBegin(TIMER_FREQUENCY / 2);
  TEST_ASSERT_NOT_NULL(t1);
  TEST_ASSERT_NOT_NULL(t2);

  timerRestart(t1);
  timerRestart(t2);
  timerStart(t1);
  timerStart(t2);

  delay(500);

  uint64_t v1 = timerRead(t1);
  uint64_t v2 = timerRead(t2);

  // t1 at 4MHz for 500ms ~ 2000000, t2 at 2MHz for 500ms ~ 1000000
  TEST_ASSERT_INT_WITHIN(200000, 2000000, v1);
  TEST_ASSERT_INT_WITHIN(100000, 1000000, v2);

  timerEnd(t1);
  timerEnd(t2);

  // Re-init the default timer for tearDown
  timer = timerBegin(TIMER_FREQUENCY);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();
  RUN_TEST(timer_read_test);
  RUN_TEST(timer_interrupt_test);
  RUN_TEST(timer_divider_test);
  RUN_TEST(timer_oneshot_test);
  RUN_TEST(timer_auto_reload_test);
  RUN_TEST(timer_stop_start_test);
  RUN_TEST(timer_multiple_test);
#if !CONFIG_IDF_TARGET_ESP32
  RUN_TEST(timer_clock_select_test);
#endif
  UNITY_END();
}

void loop() {}
