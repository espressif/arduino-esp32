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
  Modified Nov 2017 by Chuck Todd (ctodd@cableone.net) - ESP32 ISR Support
  Modified Sep 2021 by Rodrigo Garcia Corbera (rocorbera@gmail.com) - IDF refactoring effort
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
    ,rxQueued(0)
    ,transmitting(0)
    ,last_error(I2C_ERROR_OK)
    ,_timeOutMillis(50)
    ,_i2c_bus_clock(100000)
{}

TwoWire::~TwoWire()
{
    flush();
    if(i2c) {
        i2cRelease(i2c);
        i2c=NULL;
    }
}

bool TwoWire::setPins(int sdaPin, int sclPin)
{
    if(i2c) {
        log_e("can not set pins if begin was already called\n");
        return false;
    }
    if (sdaPin != -1) sda = sdaPin;
    if (sclPin != -1) scl = sclPin;
    return true;
}

bool TwoWire::setSDA(int sdaPin)
{
    return setPins(sdaPin, -1);
}

bool TwoWire::setSCL(int sclPin)
{
    return setPins(-1, sclPin);
}

bool TwoWire::begin(int sdaPin, int sclPin, uint32_t frequency)
{
    if(sdaPin < 0) { // default param passed
        if(num == 0) {
            if(sda==-1) {
                sdaPin = SDA;    //use Default Pin
            } else {
                sdaPin = sda;    // reuse prior pin
            }
        } else {
            if(sda==-1) {
                log_e("no Default SDA Pin for Second Peripheral\n");
                return false; //no Default pin for Second Peripheral
            } else {
                sdaPin = sda;    // reuse prior pin
            }
        }
    }

    if(sclPin < 0) { // default param passed
        if(num == 0) {
            if(scl == -1) {
                sclPin = SCL;    // use Default pin
            } else {
                sclPin = scl;    // reuse prior pin
            }
        } else {
            if(scl == -1) {
                log_e("no Default SCL Pin for Second Peripheral\n");
                return false; //no Default pin for Second Peripheral
            } else {
                sclPin = scl;    // reuse prior pin
            }
        }
    }	
    
    if(frequency == 0){            // Wire.begin() 
	    // in Arduino default is 100KHz
	    frequency = _i2c_bus_clock;
	}

    sda = sdaPin;
    scl = sclPin;
    i2c = i2cMasterInit(num, sda, scl, frequency);
    if(!i2c) {
        return false;
    }

    flush();
    return true;

}

void TwoWire::setTimeOut(uint16_t timeOutMillis)
{
    _timeOutMillis = timeOutMillis;
    i2cSetTimeOut(i2c, timeOutMillis);
}

uint16_t TwoWire::getTimeOut()
{
    return _timeOutMillis;
}


void TwoWire::setClock(uint32_t frequency)
{
    if(i2c) {
        log_e("can not set I2C bus clock if begin was already called\n");
        return;
    }

    _i2c_bus_clock = frequency;
}

size_t TwoWire::getClock()
{
    return _i2c_bus_clock;
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
    // resets RX buffer pointers
    rxIndex = 0;
    rxLength = 0;
    rxQueued = 0;
    i2cFlush(i2c); // cleanup
}

size_t TwoWire::write(uint8_t data)
{
    if(transmitting == 1) {
        last_error = i2cTranslateError(i2cWriteByte(i2c, data));
        if (last_error == I2C_ERROR_OK) {
            return 1;                   // return 1 byte written
        }
        return 0;                       // return 0 byte written -- FAIL
    }
    last_error = I2C_ERROR_NO_BEGIN;    // no begin, not transmitting
    return 0;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    if(transmitting == 1) {
        last_error = i2cTranslateError(i2cWriteBuffer(i2c, (uint8_t *) data, quantity));
        if (last_error == I2C_ERROR_OK) {
            return quantity;            // return number of transmited bytes (actually queued)
        }
        return 0;                       // return 0 byte written -- FAIL
    }
    last_error = I2C_ERROR_NO_BEGIN;    // no begin, not transmitting
    return 0;
}


void TwoWire::beginTransmission(uint8_t address)
{
    // if we already have a transaction started? hummm....
    // reset it all! No test for if (transmitting == 1)

    transmitting = 1;

    int err = i2cTranslateError(i2cStartTransaction(i2c, true));             // reset whole I2C transaction
    if (err == I2C_ERROR_OK) {
        err = i2cTranslateError(i2cWriteSlaveAddr(i2c, address, false));     // this is a Write Operation
    }
    last_error = err;
}


uint8_t TwoWire::endTransmission(bool sendStop)  // Assumes Wire.beginTransaction(), Wire.write()
{
    // only send something if a beginTransmission was executed
    if(transmitting == 1) {    
        if (sendStop) {  // we shall close the transaction and send it to the i2c bus 
            transmitting = 0;
            last_error = i2cCloseTransaction(i2c);
            return last_error;
        } else {
            // keep transaction open and send nothing
            return I2C_ERROR_OK;
        }
    }

    transmitting = 0;
    return I2C_ERROR_NO_BEGIN;
}


uint8_t TwoWire::requestFrom(uint8_t address, uint8_t size, bool sendStop)
{
    //use internal Wire rxBuffer, multiple requestFrom()'s may be pending, try to share rxBuffer
    uint32_t cnt = rxQueued; // currently queued reads, next available position in rxBuffer

    if(cnt < (I2C_BUFFER_LENGTH-1) && (size + cnt) <= I2C_BUFFER_LENGTH) { // any room left in rxBuffer
        rxQueued += size;
    } else { // no room to receive more!
        log_e("rxBuff overflow %d", cnt + size);
        cnt = 0;
        last_error = I2C_ERROR_MEMORY;
        flush();
        return cnt;
    }

    int err;
    if (transmitting == 1) {                        // this a regular i2c operation with a previous startTransaction
       err = i2cStartTransaction(i2c, false);       // ReStart I2C bus and do not reset the cmd_link
    } else {                                        // this is a direct readTransmission() 
       err = i2cStartTransaction(i2c, true);        // Start fresh I2C bus operation
    }
    err = last_error = i2cTranslateError(err);
    if (err != I2C_ERROR_OK) {
        return 0;                                   // FAIL, nothing was received from Slave
    }

    err = i2cWriteSlaveAddr(i2c, address, true);   // send the slave address and tell it we want to read data  
    err = last_error = i2cTranslateError(err);
    if (err != I2C_ERROR_OK) {
        return 0;                                   // FAIL, nothing was received from Slave
    }

    err = i2cRead(i2c, &rxBuffer[cnt], size);      // request Read from slave for maximum of size bytes
    err = last_error = i2cTranslateError(err);
    if (err != I2C_ERROR_OK) {
        return 0;                                   // FAIL, nothing was received from Slave
    }


    // Finally we send data to I2C BUS
    err = endTransmission(sendStop);                // send Stop if necessary
    last_error = err;                               // endTransmission() returns Arduino Wire model error codes
    // check I2C operation return 
    uint8_t ret = 0;                                // 0 bytes read so far
    if (err == I2C_ERROR_OK && sendStop) {          // slave read operation success and data moved 
        ret = size;                                 // read them all on success with stop, or nothing read on ReStart request
        // so this operation actually moved data, queuing is done.
        rxQueued = 0;
        rxIndex = 0;
        rxLength = size;
    }
    return ret;
}


uint8_t TwoWire::lastError()
{
    return (uint8_t)last_error;
}

const char ERRORTEXT[] =
    "OK\0"
    "MEMORY\0"
    "SLAVE NACK\0"
    "DATA NACK\0"
    "OTHER\0"
    "TIMEOUT\0"
    "NO BEGIN"
    "\0";


char * TwoWire::getErrorText(uint8_t err)
{
    uint8_t t = 0;
    bool found = false;
    char * message = (char*)&ERRORTEXT;

    while(!found && message[0]) {
        found = t == err;
        if(!found) {
            message = message + strlen(message) + 1;
            t++;
        }
    }
    if(!found) {
        return NULL;
    } else {
        return message;
    }
}

TwoWire Wire = TwoWire(0);
TwoWire Wire1 = TwoWire(1);
