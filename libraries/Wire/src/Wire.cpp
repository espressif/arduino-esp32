/*
  TwoWire.cpp - TWI/I2C library for Arduino & Wiring
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

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
}

#include "esp32-hal-i2c.h"
#include "Wire.h"
#include "Arduino.h"

TwoWire::TwoWire(uint8_t bus_num)
    :num(bus_num & 1)
    ,sda(-1)
    ,scl(-1)
    ,i2c(NULL)
    ,rxIndex(0)
    ,rxLength(0)
    ,txIndex(0)
    ,txLength(0)
    ,txAddress(0)
    ,transmitting(0)
{}

void TwoWire::begin(int sdaPin, int sclPin, uint32_t frequency)
{
    if(sdaPin < 0) {
        if(num == 0) {
            sdaPin = SDA;
        } else {
            return;
        }
    }

    if(sclPin < 0) {
        if(num == 0) {
            sclPin = SCL;
        } else {
            return;
        }
    }

    if(i2c == NULL) {
        i2c = i2cInit(num, 0, false);
        if(i2c == NULL) {
            return;
        }
    }

    i2cSetFrequency(i2c, frequency);

    if(sda >= 0 && sda != sdaPin) {
        i2cDetachSDA(i2c, sda);
    }

    if(scl >= 0 && scl != sclPin) {
        i2cDetachSCL(i2c, scl);
    }

    sda = sdaPin;
    scl = sclPin;

    i2cAttachSDA(i2c, sda);
    i2cAttachSCL(i2c, scl);

    flush();

    i2cInitFix(i2c);
}

void TwoWire::setClock(uint32_t frequency)
{
    i2cSetFrequency(i2c, frequency);
}

size_t TwoWire::requestFrom(uint8_t address, size_t size, bool sendStop)
{
    if(size > I2C_BUFFER_LENGTH) {
        size = I2C_BUFFER_LENGTH;
    }
    size_t read = (i2cRead(i2c, address, false, rxBuffer, size, sendStop) == 0)?size:0;
    rxIndex = 0;
    rxLength = read;
    return read;
}

uint8_t TwoWire::endTransmission(uint8_t sendStop)
{
    int8_t ret = i2cWrite(i2c, txAddress, false, txBuffer, txLength, sendStop);
    txIndex = 0;
    txLength = 0;
    transmitting = 0;
    return ret;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop)
{
    return requestFrom(address, static_cast<size_t>(quantity), static_cast<bool>(sendStop));
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity)
{
    return requestFrom(address, static_cast<size_t>(quantity), true);
}

uint8_t TwoWire::requestFrom(int address, int quantity)
{
    return requestFrom(static_cast<uint8_t>(address), static_cast<size_t>(quantity), true);
}

uint8_t TwoWire::requestFrom(int address, int quantity, int sendStop)
{
    return requestFrom(static_cast<uint8_t>(address), static_cast<size_t>(quantity), static_cast<bool>(sendStop));
}

void TwoWire::beginTransmission(uint8_t address)
{
    transmitting = 1;
    txAddress = address;
    txIndex = 0;
    txLength = 0;
}

void TwoWire::beginTransmission(int address)
{
    beginTransmission((uint8_t)address);
}

uint8_t TwoWire::endTransmission(void)
{
    return endTransmission(true);
}

size_t TwoWire::write(uint8_t data)
{
    if(transmitting) {
        if(txLength >= I2C_BUFFER_LENGTH) {
            return 0;
        }
        txBuffer[txIndex] = data;
        ++txIndex;
        txLength = txIndex;
    }
    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    if(transmitting) {
        for(size_t i = 0; i < quantity; ++i) {
            if(!write(data[i])) {
                return i;
            }
        }
    }
    return quantity;
}

int TwoWire::available(void)
{
    int result = rxLength - rxIndex;
    return result;
}

int TwoWire::read(void)
{
    int value = -1;
    if(rxIndex < rxLength) {
        value = rxBuffer[rxIndex];
        ++rxIndex;
    }
    return value;
}

int TwoWire::peek(void)
{
    int value = -1;
    if(rxIndex < rxLength) {
        value = rxBuffer[rxIndex];
    }
    return value;
}

void TwoWire::flush(void)
{
    rxIndex = 0;
    rxLength = 0;
    txIndex = 0;
    txLength = 0;
}

TwoWire Wire = TwoWire(0);
