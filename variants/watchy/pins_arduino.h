#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

static const uint8_t MENU_BTN_PIN = 26;
static const uint8_t BACK_BTN_PIN = 25;
static const uint8_t DOWN_BTN_PIN = 4;
static const uint8_t DISPLAY_CS = 5;
static const uint8_t DISPLAY_RES = 9;
static const uint8_t DISPLAY_DC = 10;
static const uint8_t DISPLAY_BUSY = 19;
static const uint8_t ACC_INT_1_PIN = 14;
static const uint8_t ACC_INT_2_PIN = 12;
static const uint8_t VIB_MOTOR_PIN = 13;
static const uint8_t RTC_INT_PIN = 27;

#if defined(ARDUINO_WATCHY_V10)
static const uint8_t UP_BTN_PIN = 32;
static const uint8_t BATT_ADC_PIN = 33;
#define UP_BTN_MASK GPIO_SEL_32
#define RTC_TYPE 1  //DS3231
#elif defined(ARDUINO_WATCHY_V15)
static const uint8_t UP_BTN_PIN = 32;
static const uint8_t BATT_ADC_PIN = 35;
#define UP_BTN_MASK GPIO_SEL_32
#define RTC_TYPE 2  //PCF8563
#elif defined(ARDUINO_WATCHY_V20)
static const uint8_t UP_BTN_PIN = 35;
static const uint8_t BATT_ADC_PIN = 34;
#define UP_BTN_MASK GPIO_SEL_35
#define RTC_TYPE 2  //PCF8563
#endif

#define MENU_BTN_MASK GPIO_SEL_26
#define BACK_BTN_MASK GPIO_SEL_25
#define DOWN_BTN_MASK GPIO_SEL_4
#define ACC_INT_MASK GPIO_SEL_14
#define BTN_PIN_MASK MENU_BTN_MASK | BACK_BTN_MASK | UP_BTN_MASK | DOWN_BTN_MASK

#endif /* Pins_Arduino_h */
