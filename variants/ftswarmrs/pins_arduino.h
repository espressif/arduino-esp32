/*
 * Pins_Arduino.h
 *
 * ftSwarmRS hardware definitions
 *
 * (C) 2021-26 Christian Bergschneider & Stefan Fuss
 *
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <driver/gpio.h>
#include <driver/adc.h>

// my hardware features
#define FTSWARM_BOARD_RS
#define FTSWARM_HAL_RS485     1
#define FTSWARM_HAL_EXT_PORT  1
#define FTSWARM_HAL_INPUTS    7
#define FTSWARM_HAL_AX_INPUTS 6
#define FTSWARM_HAL_MOTORS    2
#define FTSWARM_HAL_SERVOS    2
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
#define PIN_SDA GPIO_NUM_8
#define PIN_SDC GPIO_NUM_9
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
static const gpio_num_t HC165_CS = GPIO_NUM_NC;
static const gpio_num_t HC165_LD = GPIO_NUM_NC;
static const gpio_num_t HC165_CLK = GPIO_NUM_NC;
static const gpio_num_t HC165_MISO = GPIO_NUM_NC;

// Serial (UART0)
static const gpio_num_t TX = GPIO_NUM_43;
static const gpio_num_t RX = GPIO_NUM_44;

// RGB LED
#define RGB_BUILTIN GPIO_NUM_48

// ftSwarm definitions
static const int8_t FTSWARM_HAL_GYRO = GYRO_LSM6;
static const int8_t FTSWARM_HAL_GYRO_PORT = GYRO_SPI;

// RS485
static const gpio_num_t RS485_R = GPIO_NUM_6;
static const gpio_num_t RS485_REB = GPIO_NUM_7;
static const gpio_num_t RS485_DE = GPIO_NUM_17;
static const gpio_num_t RS485_D = GPIO_NUM_18;

// Inputs
static const gpio_num_t A1 = GPIO_NUM_1;
static const gpio_num_t A2 = GPIO_NUM_2;
static const gpio_num_t A3 = GPIO_NUM_19;
static const gpio_num_t A4 = GPIO_NUM_20;
static const gpio_num_t A5 = GPIO_NUM_13;
static const gpio_num_t A6 = GPIO_NUM_11;
static const gpio_num_t PWRCTL = GPIO_NUM_12;

static const gpio_num_t USTX = GPIO_NUM_42;
static const gpio_num_t PUA2 = GPIO_NUM_41;

// Named ports
#define FACTORYSETTINGS "A6"

// array based
static const char INPUT_NAME[][7] = {"A1", "A2", "A3", "A4", "A5", "A6", "PWRCTL"};
static const gpio_num_t INPUT_GPIO[] = {A1, A2, A3, A4, A5, A6, PWRCTL};
static const uint8_t INPUT_FLAGS[] = {FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE,
                                      FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE, FTSWARM_HAL_FLAG_NONE};
static const int8_t INPUT_ADC_UNIT[] = {ADC_UNIT_1, ADC_UNIT_1, ADC_UNIT_2, ADC_UNIT_2, ADC_UNIT_2, ADC_UNIT_2, ADC_UNIT_2};
static const int8_t INPUT_ADC_CHANNEL[] = {ADC1_CHANNEL_0, ADC1_CHANNEL_1, ADC2_CHANNEL_8, ADC2_CHANNEL_9, ADC2_CHANNEL_0, ADC2_CHANNEL_2, ADC2_CHANNEL_1};
static const adc_atten_t INPUT_ATTENUATION[] = {ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12,
                                                ADC_ATTEN_DB_12, ADC_ATTEN_DB_12, ADC_ATTEN_DB_12};
static const int8_t INPUT_IOTYPE[] = {FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL,
                                      FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_DIGITAL, FTSWARM_HAL_IO_PWRCTL};

// Motor
static const gpio_num_t M1A = GPIO_NUM_14;
static const gpio_num_t M1B = GPIO_NUM_21;
static const gpio_num_t M2A = GPIO_NUM_45;
static const gpio_num_t M2B = GPIO_NUM_46;

static const char MOTOR_NAME[][6] = {"M1", "M2"};
static const gpio_num_t MOTOR_GPIO[][2] = {{M1A, M1B}, {M2A, M2B}};
static const int8_t MOTOR_IOTYPE[] = {FTSWARM_HAL_IO_MOTOR, FTSWARM_HAL_IO_MOTOR};

// SERVO
static const gpio_num_t SERVO1 = GPIO_NUM_47;
static const gpio_num_t SERVO2 = GPIO_NUM_15;
static const gpio_num_t SERVO3 = GPIO_NUM_8;  // optional via extension port
static const gpio_num_t SERVO4 = GPIO_NUM_9;  // optional via extension port

static const char SERVO_NAME[][7] = {"SERVO1", "SERVO2", "SEROV3", "SERVO4"};
static const gpio_num_t SERVO_GPIO[] = {SERVO1, SERVO2, SERVO3, SERVO4};

#endif /* Pins_Arduino_h */
