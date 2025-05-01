/*
  I2C Master Test for
*/

#include <Arduino.h>
#include <unity.h>
#include <Wire.h>
#include <vector>
#include <algorithm>
#include <WiFi.h>

#include "sdkconfig.h"

/* DS1307 functions */

const uint8_t DS1307_ADDR = 0x68;
const uint8_t start_sec = 1;
const uint8_t start_min = 2;
const uint8_t start_hour = 3;
const uint8_t start_day = 4;
const uint8_t start_month = 5;
const uint16_t start_year = 2020;

static uint8_t read_sec = 0;
static uint8_t read_min = 0;
static uint8_t read_hour = 0;
static uint8_t read_day = 0;
static uint8_t read_month = 0;
static uint16_t read_year = 0;
static int peek_data = -1;

const char *ssid = "Wokwi-GUEST";
const char *password = "";

const auto BCD2DEC = [](uint8_t num) -> uint8_t {
  return ((num / 16 * 10) + (num % 16));
};

const auto DEC2BCD = [](uint8_t num) -> uint8_t {
  return ((num / 10 * 16) + (num % 10));
};

void reset_read_values() {
  read_sec = 0;
  read_min = 0;
  read_hour = 0;
  read_day = 0;
  read_month = 0;
  read_year = 0;
}

void ds1307_start(void) {
  uint8_t sec;

  //Get seconds
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDR, 1);
  sec = Wire.read() & 0x7F;  //Seconds without halt bit

  //Set seconds and start clock
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(0x00);
  Wire.write(sec);
  Wire.endTransmission();
}

void ds1307_stop(void) {
  uint8_t sec;

  //Get seconds
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDR, 1);
  sec = Wire.read() | 0x80;  //Seconds with halt bit

  //Set seconds and halt clock
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(0x00);
  Wire.write(sec);
  Wire.endTransmission();
}

void ds1307_get_time(uint8_t *sec, uint8_t *min, uint8_t *hour, uint8_t *day, uint8_t *month, uint16_t *year) {
  //Get time
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDR, 7);

  TEST_ASSERT_EQUAL(7, Wire.available());

  if (peek_data == -1 && Wire.peek() != -1) {
    peek_data = Wire.peek();
  }

  *sec = BCD2DEC(Wire.read() & 0x7F);  //Seconds without halt bit
  *min = BCD2DEC(Wire.read());
  *hour = BCD2DEC(Wire.read() & 0x3F);
  Wire.read();  //Ignore day of week
  *day = BCD2DEC(Wire.read());
  *month = BCD2DEC(Wire.read());
  *year = BCD2DEC(Wire.read()) + 2000;
}

void ds1307_set_time(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t month, uint16_t year) {
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(0x00);
  Wire.write(DEC2BCD(sec));
  Wire.write(DEC2BCD(min));
  Wire.write(DEC2BCD(hour));
  Wire.write(DEC2BCD(0));  //Ignore day of week
  Wire.write(DEC2BCD(day));
  Wire.write(DEC2BCD(month));
  Wire.write(DEC2BCD(year - 2000));
  Wire.endTransmission();
}

/* Unity functions */

// This function is automatically called by unity before each test is run
void setUp(void) {
  reset_read_values();
  Wire.begin();
}

// This function is automatically called by unity after each test is run
void tearDown(void) {
  //Reset time
  ds1307_set_time(start_sec, start_min, start_hour, start_day, start_month, start_year);

  Wire.end();
}

void rtc_set_time() {
  //Set time
  ds1307_set_time(start_sec, start_min, start_hour, start_day, start_month, start_year);

  //Get time
  ds1307_get_time(&read_sec, &read_min, &read_hour, &read_day, &read_month, &read_year);

  //Check time
  TEST_ASSERT_EQUAL(start_sec, read_sec);
  TEST_ASSERT_EQUAL(start_min, read_min);
  TEST_ASSERT_EQUAL(start_hour, read_hour);
  TEST_ASSERT_EQUAL(start_day, read_day);
  TEST_ASSERT_EQUAL(start_month, read_month);
  TEST_ASSERT_EQUAL(start_year, read_year);
}

void rtc_run_clock() {
  uint8_t old_sec = 0;

  //Run clock for 5 seconds
  ds1307_start();
  delay(5000);
  ds1307_stop();

  //Get time
  ds1307_get_time(&read_sec, &read_min, &read_hour, &read_day, &read_month, &read_year);

  //Check time
  TEST_ASSERT_NOT_EQUAL(start_sec, read_sec);  //Seconds should have changed
  TEST_ASSERT_EQUAL(start_min, read_min);
  TEST_ASSERT_EQUAL(start_hour, read_hour);
  TEST_ASSERT_EQUAL(start_day, read_day);
  TEST_ASSERT_EQUAL(start_month, read_month);
  TEST_ASSERT_EQUAL(start_year, read_year);

  old_sec = read_sec;
  reset_read_values();

  //Get time again to check that clock is stopped
  delay(2000);
  ds1307_get_time(&read_sec, &read_min, &read_hour, &read_day, &read_month, &read_year);

  //Check time
  TEST_ASSERT_EQUAL(old_sec, read_sec);
  TEST_ASSERT_EQUAL(start_min, read_min);
  TEST_ASSERT_EQUAL(start_hour, read_hour);
  TEST_ASSERT_EQUAL(start_day, read_day);
  TEST_ASSERT_EQUAL(start_month, read_month);
  TEST_ASSERT_EQUAL(start_year, read_year);
}

void change_clock() {
  //Get time
  ds1307_get_time(&read_sec, &read_min, &read_hour, &read_day, &read_month, &read_year);

  //Check time
  TEST_ASSERT_EQUAL(start_sec, read_sec);
  TEST_ASSERT_EQUAL(start_min, read_min);
  TEST_ASSERT_EQUAL(start_hour, read_hour);
  TEST_ASSERT_EQUAL(start_day, read_day);
  TEST_ASSERT_EQUAL(start_month, read_month);
  TEST_ASSERT_EQUAL(start_year, read_year);

  Wire.setClock(400000);
  reset_read_values();

  TEST_ASSERT_EQUAL(400000, Wire.getClock());

  //Get time
  ds1307_get_time(&read_sec, &read_min, &read_hour, &read_day, &read_month, &read_year);

  //Check time
  TEST_ASSERT_EQUAL(start_sec, read_sec);
  TEST_ASSERT_EQUAL(start_min, read_min);
  TEST_ASSERT_EQUAL(start_hour, read_hour);
  TEST_ASSERT_EQUAL(start_day, read_day);
  TEST_ASSERT_EQUAL(start_month, read_month);
  TEST_ASSERT_EQUAL(start_year, read_year);
}

void swap_pins() {
  Wire.setPins(SCL, SDA);
  Wire.begin();
  //Set time
  ds1307_set_time(start_sec, start_min, start_hour, start_day, start_month, start_year);

  //Get time
  ds1307_get_time(&read_sec, &read_min, &read_hour, &read_day, &read_month, &read_year);

  //Check time
  TEST_ASSERT_EQUAL(start_sec, read_sec);
  TEST_ASSERT_EQUAL(start_min, read_min);
  TEST_ASSERT_EQUAL(start_hour, read_hour);
  TEST_ASSERT_EQUAL(start_day, read_day);
  TEST_ASSERT_EQUAL(start_month, read_month);
  TEST_ASSERT_EQUAL(start_year, read_year);

  Wire.setPins(SDA, SCL);
}

void test_api() {
  int integer_ret;

  // Set Buffer Size
  integer_ret = Wire.setBufferSize(32);
  TEST_ASSERT_EQUAL(32, integer_ret);
  integer_ret = Wire.setBufferSize(I2C_BUFFER_LENGTH);
  TEST_ASSERT_EQUAL(I2C_BUFFER_LENGTH, integer_ret);

  // Set TimeOut
  Wire.setTimeOut(100);
  TEST_ASSERT_EQUAL(100, Wire.getTimeOut());

  // Check if buffer can be peeked
  TEST_ASSERT_GREATER_THAN(-1, peek_data);

  Wire.flush();
}

bool device_found() {
  uint8_t err;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    err = Wire.endTransmission();
    log_d("Address: 0x%02X, Error: %d", address, err);
    if (err == 0) {
      log_i("Found device at address: 0x%02X", address);
    } else if (address == DS1307_ADDR) {
      log_e("Failed to find DS1307");
      return false;
    }
  }

  return true;
}

void scan_bus() {
  TEST_ASSERT_TRUE(device_found());
}

#if SOC_WIFI_SUPPORTED
void scan_bus_with_wifi() {
  // delete old config
  WiFi.disconnect(true, true, 1000);
  delay(1000);
  WiFi.begin(ssid, password);
  delay(5000);
  bool found = device_found();
  WiFi.disconnect(true, true, 1000);

  TEST_ASSERT_TRUE(found);
}
#endif

/* Main */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  log_d("Starting I2C Master");
  Wire.begin();

  log_d("Starting tests");
  UNITY_BEGIN();
  RUN_TEST(scan_bus);
#if SOC_WIFI_SUPPORTED
  RUN_TEST(scan_bus_with_wifi);
#endif
  RUN_TEST(rtc_set_time);
  RUN_TEST(rtc_run_clock);
  RUN_TEST(change_clock);
  RUN_TEST(swap_pins);
  RUN_TEST(test_api);
  UNITY_END();
}

void loop() {
  vTaskDelete(NULL);
}
