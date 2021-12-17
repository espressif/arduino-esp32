/*
  TwoWire.h - TWI/I2C library for Arduino & Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

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

  Modified 2012 by Todd Krein (todd@krein.org) to implement repeated starts
  Modified December 2014 by Ivan Grokhotkov (ivan@esp8266.com) - esp8266 support
  Modified April 2015 by Hrsto Gochkov (ficeto@ficeto.com) - alternative esp8266 support
  Modified November 2017 by Chuck Todd <stickbreaker on GitHub> to use ISR and increase stability.
  Modified Nov 2021 by Hristo Gochkov <Me-No-Dev> to support ESP-IDF API
*/

#ifndef TwoWire_h
#define TwoWire_h

#include <esp32-hal.h>
#if !CONFIG_DISABLE_HAL_LOCKS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif
#include "Stream.h"

#ifndef I2C_BUFFER_LENGTH
    #define I2C_BUFFER_LENGTH 128
#endif
typedef void(*user_onRequest)(void);
typedef void(*user_onReceive)(uint8_t*, int);

class TwoWire: public Stream
{
protected:
    uint8_t num;
    int8_t sda;
    int8_t scl;

    uint8_t rxBuffer[I2C_BUFFER_LENGTH];
    size_t rxIndex;
    size_t rxLength;

    uint8_t txBuffer[I2C_BUFFER_LENGTH];
    size_t txLength;
    uint16_t txAddress;

    uint32_t _timeOutMillis;
    bool nonStop;
#if !CONFIG_DISABLE_HAL_LOCKS
    TaskHandle_t nonStopTask;
    SemaphoreHandle_t lock;
#endif
private:
    bool is_slave;
    void (*user_onRequest)(void);
    void (*user_onReceive)(int);
    static void onRequestService(uint8_t, void *);
    static void onReceiveService(uint8_t, uint8_t*, size_t, bool, void *);
    bool initPins(int sdaPin, int sclPin);

public:
    TwoWire(uint8_t bus_num);
    ~TwoWire();
    
    //call setPins() first, so that begin() can be called without arguments from libraries
    bool setPins(int sda, int scl);
    
    bool begin(int sda=-1, int scl=-1, uint32_t frequency=0); // returns true, if successful init of i2c bus
    bool begin(uint8_t slaveAddr, int sda=-1, int scl=-1, uint32_t frequency=0);
    bool end();

    void setTimeOut(uint16_t timeOutMillis); // default timeout of i2c transactions is 50ms
    uint16_t getTimeOut();

    bool setClock(uint32_t);
    uint32_t getClock();

    void beginTransmission(uint16_t address);
    void beginTransmission(uint8_t address);
    void beginTransmission(int address);

    uint8_t endTransmission(bool sendStop);
    uint8_t endTransmission(void);

    size_t requestFrom(uint16_t address, size_t size, bool sendStop);
    uint8_t requestFrom(uint16_t address, uint8_t size, bool sendStop);
    uint8_t requestFrom(uint16_t address, uint8_t size, uint8_t sendStop);
    size_t requestFrom(uint8_t address, size_t len, bool stopBit);
    uint8_t requestFrom(uint16_t address, uint8_t size);
    uint8_t requestFrom(uint8_t address, uint8_t size, uint8_t sendStop);
    uint8_t requestFrom(uint8_t address, uint8_t size);
    uint8_t requestFrom(int address, int size, int sendStop);
    uint8_t requestFrom(int address, int size);

    size_t write(uint8_t);
    size_t write(const uint8_t *, size_t);
    int available(void);
    int read(void);
    int peek(void);
    void flush(void);

    inline size_t write(const char * s)
    {
        return write((uint8_t*) s, strlen(s));
    }
    inline size_t write(unsigned long n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(long n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(unsigned int n)
    {
        return write((uint8_t)n);
    }
    inline size_t write(int n)
    {
        return write((uint8_t)n);
    }

    void onReceive( void (*)(int) );
    void onRequest( void (*)(void) );
    size_t slaveWrite(const uint8_t *, size_t);
};

extern TwoWire Wire;
extern TwoWire Wire1;

#endif
