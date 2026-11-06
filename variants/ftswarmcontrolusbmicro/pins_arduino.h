/*
 * Pins_Arduino.h
 *
 * ftSwarmControl USB Micro hardware definitions
 *
 * (C) 2021-26 Christian Bergschneider & Stefan Fuss
 *
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <driver/gpio.h>

// my hardware features
#define FTSWARM_BOARD_CONTROL_USBMICRO
#define FTSWARM_HAL_INPUTS     8
#define FTSWARM_HAL_AX_INPUTS  4
#define FTSWARM_HAL_MOTORS     2
#define FTSWARM_HAL_BUTTONS    8
#define FTSWARM_HAL_JOYSTICKS  2
#define FTSWARM_HAL_FIRSTJPOTI 4
#define FTSWARM_HAL_OLEDS      1
#define FTSWARM_HAL_HC165      1

#define FTSWARM_JOY1LR FTSWARM_HAL_FIRSTJPOTI
#define FTSWARM_JOY1FB FTSWARM_HAL_FIRSTJPOTI + 1
#define FTSWARM_JOY2LR FTSWARM_HAL_FIRSTJPOTI + 2
#define FTSWARM_JOY2FB FTSWARM_HAL_FIRSTJPOTI + 3

// due to some hen&egg problemns with including adc_types.h using esp32 cpu
// #include <hal/adc_hal.h>
#define xADC_UNIT_1       1
#define xADC1_CHANNEL_0   0
#define xADC1_CHANNEL_3   3
#define xADC1_CHANNEL_4   4
#define xADC1_CHANNEL_5   5
#define xADC1_CHANNEL_6   6
#define xADC1_CHANNEL_MAX 8

#define xADC_ATTEN_DB_12 3

// to solve some hen & egg problems
static const int8_t FTSWARM_HAL_IO_DIGITAL = 1;
static const int8_t FTSWARM_HAL_IO_ANALOG = 2;
static const int8_t FTSWARM_HAL_IO_PWRCTL = 3;
static const int8_t FTSWARM_HAL_IO_MOTOR = 4;
static const int8_t FTSWARM_HAL_IO_STEPPER = 5;
static const int8_t FTSWARM_HAL_IO_JOYSTICKPOTI = 6;
static const int8_t FTSWARM_HAL_IO_RCSERVO = 7;
static const int8_t FTSWARM_HAL_IO_WHEELDRIVE = 8;
static const int8_t FTSWARM_HAL_IO_RCP = 9;

static const int8_t FTSWARM_HAL_FLAG_NONE = 0x00;
static const int8_t FTSWARM_HAL_FLAG_HIDDEN = 0x01;

static const int8_t GYRO_NONE = 0;
static const int8_t GYRO_6050 = 1;
static const int8_t GYRO_LSM6 = 2;

static const int8_t GYRO_SPI = 1;
static const int8_t GYRO_INTERNAL_I2C = 2;
static const int8_t GYRO_EXTERNAL_I2C = 3;

// I2C (Standard-Bus)
#define PIN_SDA GPIO_NUM_21
#define PIN_SDC GPIO_NUM_22
static const gpio_num_t SDA = PIN_SDA;
static const gpio_num_t SCL = PIN_SDC;

// I2C (internal)
static const gpio_num_t SDA_INTERNAL = GPIO_NUM_NC;
static const gpio_num_t SCL_INTERNAL = GPIO_NUM_NC;

// SPI
static const gpio_num_t SS = GPIO_NUM_5;
static const gpio_num_t MOSI = GPIO_NUM_23;
static const gpio_num_t MISO = GPIO_NUM_19;
static const gpio_num_t SCK = GPIO_NUM_18;

// HC165
static const gpio_num_t HC165_CS = GPIO_NUM_14;
static const gpio_num_t HC165_LD = GPIO_NUM_15;
static const gpio_num_t HC165_CLK = GPIO_NUM_12;
static const gpio_num_t HC165_MISO = GPIO_NUM_35;

// Serial (UART0)
static const gpio_num_t TX = GPIO_NUM_1;
static const gpio_num_t RX = GPIO_NUM_3;

// RGB LED
#define RGB_BUILTIN GPIO_NUM_26

static const int8_t FTSWARM_HAL_GYRO = GYRO_6050;
static const int8_t FTSWARM_HAL_GYRO_PORT = GYRO_EXTERNAL_I2C;

// Inputs
static const gpio_num_t A1 = GPIO_NUM_39;
static const gpio_num_t A2 = GPIO_NUM_25;
static const gpio_num_t A3 = GPIO_NUM_26;
static const gpio_num_t A4 = GPIO_NUM_27;
static const gpio_num_t JOY1LR = GPIO_NUM_33;
static const gpio_num_t JOY1FB = GPIO_NUM_36;
static const gpio_num_t JOY2LR = GPIO_NUM_32;
static const gpio_num_t JOY2FB = GPIO_NUM_34;

static const gpio_num_t USTX = GPIO_NUM_15;
static const gpio_num_t PUA2 = GPIO_NUM_14;

// Named ports
#define FACTORYSETTINGS "S1"

// array based
static const char INPUT_NAME[][7] = {"A1", "A2", "A3", "A4", "JOY1LR", "JOY1FB", "JOY2LR", "JOY2FB"};
static const gpio_num_t INPUT_GPIO[] = {A1, A2, A3, A4, JOY1LR, JOY1FB, JOY2LR, JOY2FB};
static const uint8_t INPUT_FLAGS[] = {FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE,
                                      FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE};
static const int8_t INPUT_ADC_UNIT[] = {xADC_UNIT_1, xADC_UNIT_1, xADC_UNIT_1, xADC_UNIT_1, xADC_UNIT_1, xADC_UNIT_1, xADC_UNIT_1, xADC_UNIT_1};
static const int8_t INPUT_ADC_CHANNEL[] = {xADC1_CHANNEL_3, xADC1_CHANNEL_MAX, xADC1_CHANNEL_MAX, xADC1_CHANNEL_MAX,
                                           xADC1_CHANNEL_5, xADC1_CHANNEL_0,   xADC1_CHANNEL_4,   xADC1_CHANNEL_6};
static const int8_t INPUT_ATTENUATION[] = {xADC_ATTEN_DB_12, xADC_ATTEN_DB_12, xADC_ATTEN_DB_12, xADC_ATTEN_DB_12,
                                           xADC_ATTEN_DB_12, xADC_ATTEN_DB_12, xADC_ATTEN_DB_12, xADC_ATTEN_DB_12};
static const int8_t INPUT_IOTYPE[] = {FTSWARM_HAL_IO_DIGITAL,      FTSWARM_HAL_IO_DIGITAL,      FTSWARM_HAL_IO_DIGITAL,      FTSWARM_HAL_IO_DIGITAL,
                                      FTSWARM_HAL_IO_JOYSTICKPOTI, FTSWARM_HAL_IO_JOYSTICKPOTI, FTSWARM_HAL_IO_JOYSTICKPOTI, FTSWARM_HAL_IO_JOYSTICKPOTI};

static const char JOYSTICK_NAME[][5] = {"JOY1", "JOY2"};
static const char JOYSTICK_BUTTON[][3] = {"J1", "J2"};
static const char JOYSTICK_LR[][7] = {"JOY1LR", "JOY2LR"};
static const char JOYSTICK_FB[][7] = {"JOY1FB", "JOY2FB"};

// Motor
static const gpio_num_t M1A = GPIO_NUM_13;
static const gpio_num_t M1B = GPIO_NUM_4;
static const gpio_num_t M2A = GPIO_NUM_2;
static const gpio_num_t M2B = GPIO_NUM_0;

static const char MOTOR_NAME[][6] = {"M1", "M2"};
static const gpio_num_t MOTOR_GPIO[][2] = {{M1A, M1B}, {M2A, M2B}};
static const int8_t MOTOR_IOTYPE[] = {FTSWARM_HAL_IO_MOTOR, FTSWARM_HAL_IO_MOTOR};

#endif /* Pins_Arduino_h */
