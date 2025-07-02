/* HW Timer test */
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

void setup() {

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  UNITY_BEGIN();
  RUN_TEST(timer_read_test);
  RUN_TEST(timer_interrupt_test);
  RUN_TEST(timer_divider_test);
#if !CONFIG_IDF_TARGET_ESP32
  RUN_TEST(timer_clock_select_test);
#endif
  UNITY_END();
}

void loop() {}
