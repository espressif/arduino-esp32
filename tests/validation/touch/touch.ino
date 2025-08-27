#include <unity.h>
#include "soc/soc_caps.h"
#include "hal/touch_sensor_ll.h"

#if CONFIG_IDF_TARGET_ESP32


#define TEST_TOUCH_CHANNEL (7)
static touch_pad_t touch_list[TEST_TOUCH_CHANNEL] = {
  TOUCH_PAD_NUM0,
  //TOUCH_PAD_NUM1 is GPIO0, for download.
  TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM5, TOUCH_PAD_NUM6, TOUCH_PAD_NUM7 /*,TOUCH_PAD_NUM8, TOUCH_PAD_NUM9*/
};

uint8_t TOUCH_GPIOS[] = {4,/* 0,*/ 2, 15, 13, 12, 14, 27/*, 33, 32*/};

#define NO_TOUCH_GPIO 25

#elif (CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)

#define TEST_TOUCH_CHANNEL (8)  //14
static touch_pad_t touch_list[TEST_TOUCH_CHANNEL] = {
  TOUCH_PAD_NUM1, TOUCH_PAD_NUM2, TOUCH_PAD_NUM3,  TOUCH_PAD_NUM4,  TOUCH_PAD_NUM5, TOUCH_PAD_NUM6, TOUCH_PAD_NUM7, TOUCH_PAD_NUM8 
  /*TOUCH_PAD_NUM9, TOUCH_PAD_NUM10, TOUCH_PAD_NUM11, TOUCH_PAD_NUM12, TOUCH_PAD_NUM13, TOUCH_PAD_NUM14*/
};

uint8_t TOUCH_GPIOS[] = {1, 2, 3, 4, 5, 6, 7, 8 /*, 9, 10, 11, 12, 13, 14*/};

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

#define INTERRUPT_THRESHOLD      0    // Use benchmarked threshold

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32P4
#define PRESSED_VALUE_DIFFERENCE 200  //-200 for ESP32 and +200 for ESP32P4 read value difference against the unpressed value
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
#define PRESSED_VALUE_DIFFERENCE 2000  //2000+ read value difference against the unpressed value
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
  touch_ll_set_charge_speed(pad_num,TOUCH_CHARGE_SPEED_4);
#else
  touch_ll_set_internal_capacitor(0x7f);
#endif
}

/*
 * Change the slope to get smaller value from touch sensor. (Capacitor for ESP32P4)
 */
static void test_release_fake(touch_pad_t pad_num) {
#if SOC_TOUCH_SENSOR_VERSION <= 2
  touch_ll_set_charge_speed(pad_num, TOUCH_CHARGE_SPEED_7); 
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

  // COMPARE PRESSED <-> UNPRESSED
  for (int l = 0; l < sizeof(TOUCH_GPIOS); l++) {
    //log_i("Touch %d: %d -> %d", TOUCH_GPIOS[l], touch_unpressed[l], touch_pressed[l]);
    Serial.printf("Touch %d: %lu -> %lu\n", TOUCH_GPIOS[l], touch_unpressed[l], touch_pressed[l]);
  }
  for (int l = 0; l < sizeof(TOUCH_GPIOS); l++) {
#if CONFIG_IDF_TARGET_ESP32
    TEST_ASSERT_LESS_THAN((touch_unpressed[l] - PRESSED_VALUE_DIFFERENCE), touch_pressed[l]);
#else
    TEST_ASSERT_GREATER_THAN((touch_unpressed[l] + PRESSED_VALUE_DIFFERENCE), touch_pressed[l]);
#endif
  }
}

void test_touch_interrtupt(void) {

  touchAttachInterrupt(TOUCH_GPIOS[0], gotTouch1, INTERRUPT_THRESHOLD);
  touchAttachInterrupt(TOUCH_GPIOS[1], gotTouch2, INTERRUPT_THRESHOLD);

  test_press_fake(touch_list[0]);
  test_press_fake(touch_list[1]);

  delay(100);
  
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

#if SOC_TOUCH_SENSOR_VERSION == 3
  touch_ll_enable_internal_capacitor(true);
#endif

  UNITY_BEGIN();
  RUN_TEST(test_touch_read);
  RUN_TEST(test_touch_interrtupt);
  RUN_TEST(test_touch_errors);
  UNITY_END();
}

void loop() {
  delay(10);
}
