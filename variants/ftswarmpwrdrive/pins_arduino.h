/*
 * Pins_Arduino.h
 *
 * ftSwarmPwrDrive hardware definitions
 *
 * (C) 2021-26 Christian Bergschneider & Stefan Fuss
 *
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <driver/gpio.h>
#include <driver/adc.h>

// my hardware features
#define FTSWARM_BOARD_PWRDRIVE
#define FTSWARM_HAL_RS485     1
#define FTSWARM_HAL_INPUTS    5
#define FTSWARM_HAL_AX_INPUTS 5
#define FTSWARM_HAL_MOTORS    4
#define FTSWARM_HAL_PIXELS    2

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
#define PIN_SDA GPIO_NUM_5
#define PIN_SDC GPIO_NUM_4
static const gpio_num_t SDA = PIN_SDA;
static const gpio_num_t SCL = PIN_SDC;

// I2C (internal)
static const gpio_num_t SDA_INTERNAL = GPIO_NUM_NC;
static const gpio_num_t SCL_INTERNAL = GPIO_NUM_NC;

// SPI
static const gpio_num_t SS = GPIO_NUM_NC;
static const gpio_num_t MOSI = GPIO_NUM_NC;
static const gpio_num_t MISO = GPIO_NUM_NC;
static const gpio_num_t SCK = GPIO_NUM_NC;

// Serial (UART0)
static const gpio_num_t TX = GPIO_NUM_43;
static const gpio_num_t RX = GPIO_NUM_44;

// RGB LED
#define RGB_BUILTIN GPIO_NUM_48

// RS485
static const gpio_num_t RS485_R = GPIO_NUM_6;
static const gpio_num_t RS485_REB = GPIO_NUM_7;
static const gpio_num_t RS485_DE = GPIO_NUM_17;
static const gpio_num_t RS485_D = GPIO_NUM_18;

// Inputs
static const gpio_num_t ES1 = GPIO_NUM_NC;
static const gpio_num_t ES2 = GPIO_NUM_NC;
static const gpio_num_t ES3 = GPIO_NUM_NC;
static const gpio_num_t ES4 = GPIO_NUM_NC;
static const gpio_num_t EM = GPIO_NUM_NC;

static const gpio_num_t USTX = GPIO_NUM_NC;
static const gpio_num_t PUA2 = GPIO_NUM_NC;

// special input ports
#define FACTORYSETTINGS "EM"

// array based
static const char INPUT_NAME[][7] = {"ES1", "ES2", "ES3", "ES4", "EM"};
static const gpio_num_t INPUT_GPIO[] = {ES1, ES2, ES3, ES4, EM};
static const uint8_t INPUT_FLAGS[] = {FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE};
static const int8_t INPUT_ADC_UNIT[] = {ADC_UNIT_1, ADC_UNIT_1, ADC_UNIT_1, ADC_UNIT_1, ADC_UNIT_1};
static const int8_t INPUT_ADC_CHANNEL[] = {ADC1_CHANNEL_0, ADC1_CHANNEL_0, ADC1_CHANNEL_0, ADC1_CHANNEL_0, ADC1_CHANNEL_0};
static const adc_atten_t INPUT_ATTENUATION[] = {ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12};
static const int8_t INPUT_IOTYPE[] = {FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL};

// Motor
static const gpio_num_t M1A = GPIO_NUM_NC;
static const gpio_num_t M1B = GPIO_NUM_NC;
static const gpio_num_t M2A = GPIO_NUM_NC;
static const gpio_num_t M2B = GPIO_NUM_NC;
static const gpio_num_t M3A = GPIO_NUM_NC;
static const gpio_num_t M3B = GPIO_NUM_NC;
static const gpio_num_t M4A = GPIO_NUM_NC;
static const gpio_num_t M4B = GPIO_NUM_NC;

static const char MOTOR_NAME[][6] = {"STEP1", "STEP2", "STEP3", "STEP4"};
static const gpio_num_t MOTOR_GPIO[][2] = {{M1A, M1B}, {M2A, M2B}, {M3A, M3B}, {M4A, M4B}};
static const int8_t MOTOR_IOTYPE[] = {FTSWARM_HAL_IO_STEPPER, FTSWARM_HAL_IO_STEPPER, FTSWARM_HAL_IO_STEPPER, FTSWARM_HAL_IO_STEPPER};

#endif /* Pins_Arduino_h */
