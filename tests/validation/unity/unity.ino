#include <unity.h>

/* These functions are intended to be called before and after each test. */
void setUp(void) {}

void tearDown(void) {}

void test_pass(void) {
  TEST_ASSERT_EQUAL(1, 1);
}

void test_fail(void) {
  TEST_ASSERT_EQUAL(1, 1);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  UNITY_BEGIN();
  RUN_TEST(test_pass);
  RUN_TEST(test_fail);
  UNITY_END();
}

void loop() {}
