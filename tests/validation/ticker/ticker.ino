/*
 * Ticker Validation Test
 *
 * Covers: initial inactive state, periodic attach (attach/attach_ms/attach_us),
 * one-shot once (once/once_ms/once_us), active() state transitions,
 * detach() stops callbacks, restart changes the period, re-attach while
 * running, callbacks with typed arguments (attach/once with TArg overload),
 * and large typed arguments (larger than sizeof(void*)).
 */

#include <Arduino.h>
#include <Ticker.h>
#include <unity.h>

static Ticker ticker;
static volatile int callCount = 0;
static volatile int argReceived = 0;

static void countCb(void) {
  callCount = callCount + 1;
}

static void argCb(int val) {
  argReceived = val;
}

void setUp(void) {
  ticker.detach();
  callCount = 0;
  argReceived = 0;
}

void tearDown(void) {
  ticker.detach();
}

// ==================== Initial state ====================

void test_initially_inactive(void) {
  TEST_ASSERT_FALSE(ticker.active());
}

// ==================== attach (periodic) ====================

void test_attach_ms_periodic(void) {
  ticker.attach_ms(100, countCb);
  TEST_ASSERT_TRUE(ticker.active());
  delay(550);
  ticker.detach();
  // Should have fired ~5 times in 550 ms; allow ±1 for simulation jitter
  TEST_ASSERT_INT_WITHIN(1, 5, callCount);
}

void test_attach_us_periodic(void) {
  ticker.attach_us(100000, countCb);  // 100 ms in µs
  TEST_ASSERT_TRUE(ticker.active());
  delay(550);
  ticker.detach();
  TEST_ASSERT_INT_WITHIN(1, 5, callCount);
}

void test_attach_seconds_periodic(void) {
  ticker.attach(0.1f, countCb);  // 100 ms
  TEST_ASSERT_TRUE(ticker.active());
  delay(550);
  ticker.detach();
  TEST_ASSERT_INT_WITHIN(1, 5, callCount);
}

// ==================== once (one-shot) ====================

void test_once_ms(void) {
  ticker.once_ms(100, countCb);
  TEST_ASSERT_TRUE(ticker.active());
  delay(300);
  TEST_ASSERT_EQUAL(1, callCount);
  TEST_ASSERT_FALSE(ticker.active());
}

void test_once_us(void) {
  ticker.once_us(100000, countCb);  // 100 ms in µs
  TEST_ASSERT_TRUE(ticker.active());
  delay(300);
  TEST_ASSERT_EQUAL(1, callCount);
  TEST_ASSERT_FALSE(ticker.active());
}

void test_once_seconds(void) {
  ticker.once(0.1f, countCb);  // 100 ms
  TEST_ASSERT_TRUE(ticker.active());
  delay(300);
  TEST_ASSERT_EQUAL(1, callCount);
  TEST_ASSERT_FALSE(ticker.active());
}

// ==================== active() state ====================

void test_active_while_running(void) {
  ticker.attach_ms(5000, countCb);  // Long period — will not fire during test
  TEST_ASSERT_TRUE(ticker.active());
  ticker.detach();
  TEST_ASSERT_FALSE(ticker.active());
}

// ==================== detach() ====================

void test_detach_stops_periodic(void) {
  ticker.attach_ms(100, countCb);
  delay(250);
  ticker.detach();
  int count_at_detach = callCount;
  delay(300);
  // No additional callbacks after detach
  TEST_ASSERT_EQUAL(count_at_detach, callCount);
}

void test_detach_on_inactive_is_safe(void) {
  // Double-detach must not crash
  ticker.detach();
  ticker.detach();
  TEST_ASSERT_FALSE(ticker.active());
}

// ==================== restart() ====================

void test_restart_ms(void) {
  // Start with a long period (5 s) so it does not fire during setup
  ticker.attach_ms(5000, countCb);
  TEST_ASSERT_EQUAL(0, callCount);
  // Shorten period to 100 ms
  ticker.restart_ms(100);
  delay(400);
  ticker.detach();
  // Should have fired at least once after restart
  TEST_ASSERT_GREATER_THAN(0, callCount);
}

void test_restart_us(void) {
  ticker.attach_ms(5000, countCb);
  ticker.restart_us(100000);  // 100 ms in µs
  delay(400);
  ticker.detach();
  TEST_ASSERT_GREATER_THAN(0, callCount);
}

void test_restart_seconds(void) {
  ticker.attach_ms(5000, countCb);
  ticker.restart(0.1f);  // 100 ms
  delay(400);
  ticker.detach();
  TEST_ASSERT_GREATER_THAN(0, callCount);
}

// ==================== Re-attach while running ====================

void test_reattach_while_running(void) {
  ticker.attach_ms(100, countCb);
  delay(250);
  TEST_ASSERT_GREATER_THAN(0, callCount);
  callCount = 0;
  // Re-attach without manual detach — must not crash or leak the old timer
  ticker.attach_ms(100, countCb);
  TEST_ASSERT_TRUE(ticker.active());
  delay(350);
  ticker.detach();
  TEST_ASSERT_INT_WITHIN(1, 3, callCount);
}

void test_reattach_different_callback(void) {
  ticker.attach_ms(100, countCb);
  delay(150);
  // Switch to a different callback while running
  ticker.attach_ms(100, argCb, (int)77);
  delay(200);
  ticker.detach();
  TEST_ASSERT_EQUAL(77, argReceived);
}

// ==================== Callbacks with arguments ====================

void test_attach_ms_with_arg(void) {
  ticker.attach_ms(100, argCb, (int)99);
  delay(200);
  ticker.detach();
  TEST_ASSERT_EQUAL(99, argReceived);
}

void test_once_ms_with_arg(void) {
  ticker.once_ms(100, argCb, (int)42);
  delay(300);
  TEST_ASSERT_EQUAL(42, argReceived);
  TEST_ASSERT_FALSE(ticker.active());
}

// ==================== Large typed arguments ====================

struct BigArg {
  int a;
  int b;
  int c;
};

static volatile int bigArgSum = 0;

static void bigArgCb(BigArg arg) {
  bigArgSum = arg.a + arg.b + arg.c;
}

void test_once_ms_with_large_arg(void) {
  bigArgSum = 0;
  BigArg val = {10, 20, 30};
  ticker.once_ms(100, bigArgCb, val);
  delay(300);
  TEST_ASSERT_EQUAL(60, bigArgSum);
  TEST_ASSERT_FALSE(ticker.active());
}

void test_attach_ms_with_large_arg(void) {
  bigArgSum = 0;
  BigArg val = {100, 200, 300};
  ticker.attach_ms(100, bigArgCb, val);
  delay(200);
  ticker.detach();
  TEST_ASSERT_EQUAL(600, bigArgSum);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();

  // Initial state
  RUN_TEST(test_initially_inactive);

  // Periodic attach variants
  RUN_TEST(test_attach_ms_periodic);
  RUN_TEST(test_attach_us_periodic);
  RUN_TEST(test_attach_seconds_periodic);

  // One-shot variants
  RUN_TEST(test_once_ms);
  RUN_TEST(test_once_us);
  RUN_TEST(test_once_seconds);

  // active() state
  RUN_TEST(test_active_while_running);

  // detach()
  RUN_TEST(test_detach_stops_periodic);
  RUN_TEST(test_detach_on_inactive_is_safe);

  // restart() variants
  RUN_TEST(test_restart_ms);
  RUN_TEST(test_restart_us);
  RUN_TEST(test_restart_seconds);

  // Re-attach while running
  RUN_TEST(test_reattach_while_running);
  RUN_TEST(test_reattach_different_callback);

  // Argument callbacks
  RUN_TEST(test_attach_ms_with_arg);
  RUN_TEST(test_once_ms_with_arg);

  // Large typed arguments (larger than sizeof(void*))
  RUN_TEST(test_once_ms_with_large_arg);
  RUN_TEST(test_attach_ms_with_large_arg);

  UNITY_END();
}

void loop() {}
