/*
 Arduino.h - Main include file for the Arduino SDK
 Copyright (c) 2005-2013 Arduino Team.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef Arduino_h
#define Arduino_h

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "esp_arduino_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp32-hal.h"
#include "esp8266-compat.h"
#include "soc/gpio_reg.h"

#include "stdlib_noniso.h"
#include "binary.h"

#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER 2.718281828459045235360287471352

#define SERIAL  0x0
#define DISPLAY 0x1

#define LSBFIRST 0
#define MSBFIRST 1

//Interrupt Modes
#define RISING    0x01
#define FALLING   0x02
#define CHANGE    0x03
#define ONLOW     0x04
#define ONHIGH    0x05
#define ONLOW_WE  0x0C
#define ONHIGH_WE 0x0D

#define DEFAULT 1
#define EXTERNAL 0

#ifndef __STRINGIFY
#define __STRINGIFY(a) #a
#endif

// can't define max() / min() because of conflicts with C++
#define _min(a,b) ((a)<(b)?(a):(b))  
#define _max(a,b) ((a)>(b)?(a):(b))
#define _abs(x) ((x)>0?(x):-(x))  // abs() comes from STL
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define _round(x)     ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))  // round() comes from STL
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define sq(x) ((x)*(x))

// ESP32xx runs FreeRTOS... disabling interrupts can lead to issues, such as Watchdog Timeout
#define sei() portENABLE_INTERRUPTS()
#define cli() portDISABLE_INTERRUPTS()
#define interrupts() sei()
#define noInterrupts() cli()

#define clockCyclesPerMicrosecond() ( (long int)getCpuFrequencyMhz() )
#define clockCyclesToMicroseconds(a) ( (a) / clockCyclesPerMicrosecond() )
#define microsecondsToClockCycles(a) ( (a) * clockCyclesPerMicrosecond() )

#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

// avr-libc defines _NOP() since 1.6.2
#ifndef _NOP
#define _NOP() do { __asm__ volatile ("nop"); } while (0)
#endif

#define bit(b) (1UL << (b))
#define _BV(b) (1UL << (b))

#define digitalPinToTimer(pin)      (0)
#define analogInPinToBit(P)         (P)
#if SOC_GPIO_PIN_COUNT <= 32
#define digitalPinToPort(pin)       (0)
#define digitalPinToBitMask(pin)    (1UL << (pin))
#define portOutputRegister(port)    ((volatile uint32_t*)GPIO_OUT_REG)
#define portInputRegister(port)     ((volatile uint32_t*)GPIO_IN_REG)
#define portModeRegister(port)      ((volatile uint32_t*)GPIO_ENABLE_REG)
#elif SOC_GPIO_PIN_COUNT <= 64
#define digitalPinToPort(pin)       (((pin)>31)?1:0)
#define digitalPinToBitMask(pin)    (1UL << (((pin)>31)?((pin)-32):(pin)))
#define portOutputRegister(port)    ((volatile uint32_t*)((port)?GPIO_OUT1_REG:GPIO_OUT_REG))
#define portInputRegister(port)     ((volatile uint32_t*)((port)?GPIO_IN1_REG:GPIO_IN_REG))
#define portModeRegister(port)      ((volatile uint32_t*)((port)?GPIO_ENABLE1_REG:GPIO_ENABLE_REG))
#else
#error SOC_GPIO_PIN_COUNT > 64 not implemented
#endif

#define NOT_A_PIN -1
#define NOT_A_PORT -1
#define NOT_AN_INTERRUPT -1
#define NOT_ON_TIMER 0

typedef bool boolean;
typedef uint8_t byte;
typedef unsigned int word;

#ifdef __cplusplus
void setup(void);
void loop(void);

long random(long, long);
#endif
void randomSeed(unsigned long);
long map(long, long, long, long, long);

#ifdef __cplusplus
extern "C" {
#endif

void init(void);
void initVariant(void);
void initArduino(void);

unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout);
unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout);

uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder);
void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val);

#ifdef __cplusplus
}

#include <algorithm>
#include <cmath>

#include "WCharacter.h"
#include "WString.h"
#include "Stream.h"
#include "Printable.h"
#include "Print.h"
#include "IPAddress.h"
#include "Client.h"
#include "Server.h"
#include "Udp.h"
#include "HardwareSerial.h"
#include "Esp.h"
#include "esp32/spiram.h"

// Use float-compatible stl abs() and round(), we don't use Arduino macros to avoid issues with the C++ libraries
using std::abs;
using std::isinf;
using std::isnan;
using std::max;
using std::min;
using std::round;

uint16_t makeWord(uint16_t w);
uint16_t makeWord(uint8_t h, uint8_t l);

#define word(...) makeWord(__VA_ARGS__)

size_t getArduinoLoopTaskStackSize(void);
#define SET_LOOP_TASK_STACK_SIZE(sz) size_t getArduinoLoopTaskStackSize() { return sz;}

// allows user to bypass esp_spiram_test()
#define BYPASS_SPIRAM_TEST(bypass) bool testSPIRAM(void) { if (bypass) return true; else return esp_spiram_test(); }

unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout = 1000000L);
unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout = 1000000L);

extern "C" bool getLocalTime(struct tm * info, uint32_t ms = 5000);
extern "C" void configTime(long gmtOffset_sec, int daylightOffset_sec,
        const char* server1, const char* server2 = nullptr, const char* server3 = nullptr);
extern "C" void configTzTime(const char* tz,
        const char* server1, const char* server2 = nullptr, const char* server3 = nullptr);

void setToneChannel(uint8_t channel = 0);
void tone(uint8_t _pin, unsigned int frequency, unsigned long duration = 0);
void noTone(uint8_t _pin);

// WMath prototypes
long random(long);
#endif /* __cplusplus */

#include "pins_arduino.h"

#endif /* _ESP32_CORE_ARDUINO_H_ */
