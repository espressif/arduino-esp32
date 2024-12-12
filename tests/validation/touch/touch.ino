#include <unity.h>
#include "soc/soc_caps.h"
#include "driver/touch_pad.h"

#if SOC_TOUCH_SENSOR_VERSION == 3
#include "hal/touch_sensor_ll.h"
#endif

#if CONFIG_IDF_TARGET_ESP32

#define TEST_TOUCH_CHANNEL (9)
static touch_pad_t touch_list[TEST_TOUCH_CHANNEL] = {
  TOUCH_PAD_NUM0,
  //TOUCH_PAD_NUM1 is GPIO0, for download.
  TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM5, TOUCH_PAD_NUM6, TOUCH_PAD_NUM7, TOUCH_PAD_NUM8, TOUCH_PAD_NUM9
};

uint8_t TOUCH_GPIOS[] = {4, 2, 15, 13, 12, 14, 27, 33, 32};

#define NO_TOUCH_GPIO 25

#elif (CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)

#define TEST_TOUCH_CHANNEL (12)  //14
static touch_pad_t touch_list[TEST_TOUCH_CHANNEL] = {
  TOUCH_PAD_NUM1, TOUCH_PAD_NUM2, TOUCH_PAD_NUM3,  TOUCH_PAD_NUM4,  TOUCH_PAD_NUM5, TOUCH_PAD_NUM6, TOUCH_PAD_NUM7,
  TOUCH_PAD_NUM8, TOUCH_PAD_NUM9, TOUCH_PAD_NUM10, TOUCH_PAD_NUM11, TOUCH_PAD_NUM12
  //TOUCH_PAD_NUM13, //Wrong reading
  //TOUCH_PAD_NUM14
};

uint8_t TOUCH_GPIOS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 /*,13,14*/};

#define NO_TOUCH_GPIO 17

#else  //ESP32P4

#define TEST_TOUCH_CHANNEL (5)  //14
static touch_pad_t touch_list[TEST_TOUCH_CHANNEL] = {
  TOUCH_PAD_NUM0, TOUCH_PAD_NUM1, TOUCH_PAD_NUM2,
  TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, /* TOUCH_PAD_NUM5, TOUCH_PAD_NUM6,
  TOUCH_PAD_NUM7, TOUCH_PAD_NUM8, TOUCH_PAD_NUM9, TOUCH_PAD_NUM10, TOUCH_PAD_NUM11, TOUCH_PAD_NUM12, TOUCH_PAD_NUM13*/
};

uint8_t TOUCH_GPIOS[] = {2, 3, 4, 5, 6 /*, 7, 8, 9, 10, 11, 12 ,13, 14, 15*/};

#define NO_TOUCH_GPIO 17
#endif

#if CONFIG_IDF_TARGET_ESP32
#define RELEASED_VALUE      75  //75+ read value to pass test
#define PRESSED_VALUE       20  //20- read value to pass test
#define INTERRUPT_THRESHOLD 40
#elif CONFIG_IDF_TARGET_ESP32S2
#define RELEASED_VALUE      10000  //10000- read value to pass test
#define PRESSED_VALUE       42000  //40000+ read value to pass test
#define INTERRUPT_THRESHOLD 30000
#elif CONFIG_IDF_TARGET_ESP32S3
#define RELEASED_VALUE      25000  //25000- read value to pass test
#define PRESSED_VALUE       90000  //90000+ read value to pass test
#define INTERRUPT_THRESHOLD 80000
#elif CONFIG_IDF_TARGET_ESP32P4
#define PRESSED_VALUE_DIFFERENCE 200  //200+ read value difference against the unpressed value
#define INTERRUPT_THRESHOLD      0    // Use benchmarked threshold
#else
#error Test not currently supported on this chip. Please adjust and try again!
#endif

bool touch1detected = false;
bool touch2detected = false;

void gotTouch1() {
  touch1detected = true;
}

void gotTouch2() {
  touch2detected = true;
}

/*
 * Change the slope to get larger value from touch sensor. (Capacitor for ESP32P4)
 */
static void test_press_fake(touch_pad_t pad_num) {
#if SOC_TOUCH_SENSOR_VERSION <= 2
  touch_pad_set_cnt_mode(pad_num, TOUCH_PAD_SLOPE_1, TOUCH_PAD_TIE_OPT_DEFAULT);
#else
  touch_ll_set_internal_capacitor(0x7f);
#endif
}

/*
 * Change the slope to get smaller value from touch sensor. (Capacitor for ESP32P4)
 */
static void test_release_fake(touch_pad_t pad_num) {
#if SOC_TOUCH_SENSOR_VERSION <= 2
  touch_pad_set_cnt_mode(pad_num, TOUCH_PAD_SLOPE_7, TOUCH_PAD_TIE_OPT_DEFAULT);
#else
  touch_ll_set_internal_capacitor(0);
#endif
}

/* These functions are intended to be called before and after each test. */
void setUp(void) {}

void tearDown(void) {
  for (int i = 0; i < TEST_TOUCH_CHANNEL; i++) {
    test_release_fake(touch_list[i]);
  }
  delay(100);
}

/*
 * Test Touch read on all available channels - compare values if reading is right
 */
void test_touch_read(void) {

#if SOC_TOUCH_SENSOR_VERSION <= 2
  //TEST RELEASE STATE
  for (int i = 0; i < sizeof(TOUCH_GPIOS); i++) {
#ifdef CONFIG_IDF_TARGET_ESP32
    TEST_ASSERT_GREATER_THAN(RELEASED_VALUE, touchRead(TOUCH_GPIOS[i]));
#else
    TEST_ASSERT_LESS_THAN(RELEASED_VALUE, touchRead(TOUCH_GPIOS[i]));
#endif
  }

  // TEST PRESS STATE
  for (int j = 0; j < TEST_TOUCH_CHANNEL; j++) {
    test_press_fake(touch_list[j]);
  }
  delay(100);

  for (int k = 0; k < sizeof(TOUCH_GPIOS); k++) {
#ifdef CONFIG_IDF_TARGET_ESP32
    TEST_ASSERT_LESS_THAN(PRESSED_VALUE, touchRead(TOUCH_GPIOS[k]));
#else
    TEST_ASSERT_GREATER_THAN(PRESSED_VALUE, touchRead(TOUCH_GPIOS[k]));
#endif
  }
#else  //TOUCH V3
  //TEST RELEASE STATE
  touch_value_t touch_unpressed[sizeof(TOUCH_GPIOS)];
  for (int i = 0; i < sizeof(TOUCH_GPIOS); i++) {
    touch_unpressed[i] = touchRead(TOUCH_GPIOS[i]);
  }

  // TEST PRESS STATE
  for (int j = 0; j < TEST_TOUCH_CHANNEL; j++) {
    test_press_fake(touch_list[j]);
  }
  delay(100);

  touch_value_t touch_pressed[sizeof(TOUCH_GPIOS)];
  for (int k = 0; k < sizeof(TOUCH_GPIOS); k++) {
    touch_pressed[k] = touchRead(TOUCH_GPIOS[k]);
  }

  // COMPARE PRESSED > UNPRESSED
  for (int l = 0; l < sizeof(TOUCH_GPIOS); l++) {
    TEST_ASSERT_GREATER_THAN((touch_unpressed[l] + PRESSED_VALUE_DIFFERENCE), touch_pressed[l]);
  }
#endif
}

void test_touch_interrtupt(void) {

  touchAttachInterrupt(TOUCH_GPIOS[0], gotTouch1, INTERRUPT_THRESHOLD);
  touchAttachInterrupt(TOUCH_GPIOS[1], gotTouch2, INTERRUPT_THRESHOLD);

  test_press_fake(touch_list[0]);
  test_press_fake(touch_list[1]);

  delay(300);

  touchDetachInterrupt(TOUCH_GPIOS[0]);
  touchDetachInterrupt(TOUCH_GPIOS[1]);

  TEST_ASSERT_TRUE(touch1detected);
  TEST_ASSERT_TRUE(touch2detected);
}

void test_touch_errors(void) {

  TEST_ASSERT_FALSE(touchRead(NO_TOUCH_GPIO));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  UNITY_BEGIN();
  RUN_TEST(test_touch_read);
  RUN_TEST(test_touch_interrtupt);
  RUN_TEST(test_touch_errors);
  UNITY_END();
}

void loop() {
  delay(10);
}
