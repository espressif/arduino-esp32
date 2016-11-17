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
*/

#ifndef TwoWire_h
#define TwoWire_h

#include <esp32-hal.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "Stream.h"

#define I2C_BUFFER_LENGTH 128

class TwoWire: public Stream
{
protected:
    uint8_t num;
    int8_t sda;
    int8_t scl;
    i2c_t * i2c;

    uint8_t rxBuffer[I2C_BUFFER_LENGTH];
    uint16_t rxIndex;
    uint16_t rxLength;

    uint8_t txBuffer[I2C_BUFFER_LENGTH];
    uint16_t txIndex;
    uint16_t txLength;
    uint8_t txAddress;

    uint8_t transmitting;

public:
    TwoWire(uint8_t bus_num);
    void begin(int sda=-1, int scl=-1, uint32_t frequency=100000);
    void setClock(uint32_t);
    void beginTransmission(uint8_t);
    void beginTransmission(int);
    uint8_t endTransmission(void);
    uint8_t endTransmission(uint8_t);
    size_t requestFrom(uint8_t address, size_t size, bool sendStop);

    uint8_t requestFrom(uint8_t, uint8_t);
    uint8_t requestFrom(uint8_t, uint8_t, uint8_t);
    uint8_t requestFrom(int, int);
    uint8_t requestFrom(int, int, int);

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
};

extern TwoWire Wire;

#endif
