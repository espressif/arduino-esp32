/*
 * Pins_Arduino.h
 *
 * ftSwarmControlUSBC hardware definitions
 *
 * (C) 2021-26 Christian Bergschneider & Stefan Fuss
 *
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <driver/gpio.h>
#include <driver/adc.h>

// my hardware features
#define FTSWARM_BOARD_CONTROL_USBC
#define FTSWARM_HAL_INPUTS     11
#define FTSWARM_HAL_AX_INPUTS  6
#define FTSWARM_HAL_MOTORS     2
#define FTSWARM_HAL_BUTTONS    8
#define FTSWARM_HAL_JOYSTICKS  2
#define FTSWARM_HAL_FIRSTJPOTI 7
#define FTSWARM_HAL_OLEDS      1
#define FTSWARM_HAL_HC165      1

#define FTSWARM_JOY1LR FTSWARM_HAL_FIRSTJPOTI
#define FTSWARM_JOY1FB FTSWARM_HAL_FIRSTJPOTI + 1
#define FTSWARM_JOY2LR FTSWARM_HAL_FIRSTJPOTI + 2
#define FTSWARM_JOY2FB FTSWARM_HAL_FIRSTJPOTI + 3

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
#define PIN_SDA GPIO_NUM_4
#define PIN_SDC GPIO_NUM_5
static const gpio_num_t SDA = PIN_SDA;
static const gpio_num_t SCL = PIN_SDC;

// I2C (internal)
static const gpio_num_t SDA_INTERNAL = GPIO_NUM_NC;
static const gpio_num_t SCL_INTERNAL = GPIO_NUM_NC;

// SPI
static const gpio_num_t SS = GPIO_NUM_3;
static const gpio_num_t MOSI = GPIO_NUM_38;
static const gpio_num_t MISO = GPIO_NUM_39;
static const gpio_num_t SCK = GPIO_NUM_40;

// HC165
static const gpio_num_t HC165_CS = GPIO_NUM_10;
static const gpio_num_t HC165_LD = GPIO_NUM_47;
static const gpio_num_t HC165_CLK = GPIO_NUM_48;
static const gpio_num_t HC165_MISO = GPIO_NUM_15;

// Serial (UART0)
static const gpio_num_t TX = GPIO_NUM_43;
static const gpio_num_t RX = GPIO_NUM_44;

// RGB LED
#define RGB_BUILTIN GPIO_NUM_48

static const int8_t FTSWARM_HAL_GYRO = GYRO_LSM6;
static const int8_t FTSWARM_HAL_GYRO_PORT = GYRO_SPI;

// Inputs
static const gpio_num_t A1 = GPIO_NUM_1;
static const gpio_num_t A2 = GPIO_NUM_2;
static const gpio_num_t A3 = GPIO_NUM_19;
static const gpio_num_t A4 = GPIO_NUM_20;
static const gpio_num_t A5 = GPIO_NUM_13;
static const gpio_num_t A6 = GPIO_NUM_11;
static const gpio_num_t PWRCTL = GPIO_NUM_12;
static const gpio_num_t JOY1LR = GPIO_NUM_6;
static const gpio_num_t JOY1FB = GPIO_NUM_7;
static const gpio_num_t JOY2LR = GPIO_NUM_17;
static const gpio_num_t JOY2FB = GPIO_NUM_18;

static const gpio_num_t USTX = GPIO_NUM_42;
static const gpio_num_t PUA2 = GPIO_NUM_41;

// Named ports
#define FACTORYSETTINGS "S1"

// array based
static const char INPUT_NAME[][7] = {"A1", "A2", "A3", "A4", "A5", "A6", "PWRCTL", "JOY1LR", "JOY1FB", "JOY2LR", "JOY2FB"};
static const gpio_num_t INPUT_GPIO[] = {A1, A2, A3, A4, A5, A6, PWRCTL, JOY1LR, JOY1FB, JOY2LR, JOY2FB};
static const uint8_t INPUT_FLAGS[] = {FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE,
                                      FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE,
                                      FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE};
static const int8_t INPUT_ADC_UNIT[] = {ADC_UNIT_1, ADC_UNIT_1, ADC_UNIT_2, ADC_UNIT_2, ADC_UNIT_2, ADC_UNIT_2,
                                        ADC_UNIT_2, ADC_UNIT_1, ADC_UNIT_1, ADC_UNIT_2, ADC_UNIT_2};
static const int8_t INPUT_ADC_CHANNEL[] = {ADC1_CHANNEL_0, ADC1_CHANNEL_1, ADC2_CHANNEL_8, ADC2_CHANNEL_9, ADC2_CHANNEL_0, ADC2_CHANNEL_2,
                                           ADC2_CHANNEL_1, ADC1_CHANNEL_5, ADC1_CHANNEL_6, ADC2_CHANNEL_6, ADC2_CHANNEL_7};
static const adc_atten_t INPUT_ATTENUATION[] = {ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12,
                                                ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12};
static const int8_t INPUT_IOTYPE[] = {FTSWARM_HAL_IO_DIGITAL,      FTSWARM_HAL_IO_DIGITAL,      FTSWARM_HAL_IO_DIGITAL,     FTSWARM_HAL_IO_DIGITAL,
                                      FTSWARM_HAL_IO_DIGITAL,      FTSWARM_HAL_IO_DIGITAL,      FTSWARM_HAL_IO_PWRCTL,      FTSWARM_HAL_IO_JOYSTICKPOTI,
                                      FTSWARM_HAL_IO_JOYSTICKPOTI, FTSWARM_HAL_IO_JOYSTICKPOTI, FTSWARM_HAL_IO_JOYSTICKPOTI};

static const char JOYSTICK_NAME[][5] = {"JOY1", "JOY2"};
static const char JOYSTICK_BUTTON[][3] = {"J1", "J2"};
static const char JOYSTICK_LR[][7] = {"JOY1LR", "JOY2LR"};
static const char JOYSTICK_FB[][7] = {"JOY1FB", "JOY2FB"};

// Motor
static const gpio_num_t M1A = GPIO_NUM_45;
static const gpio_num_t M1B = GPIO_NUM_46;
static const gpio_num_t M2A = GPIO_NUM_14;
static const gpio_num_t M2B = GPIO_NUM_21;

static const char MOTOR_NAME[][6] = {"M1", "M2"};
static const gpio_num_t MOTOR_GPIO[][6] = {{M1A, M1B}, {M2A, M2B}};
static const int8_t MOTOR_IOTYPE[] = {FTSWARM_HAL_IO_MOTOR, FTSWARM_HAL_IO_MOTOR};

#endif /* Pins_Arduino_h */
