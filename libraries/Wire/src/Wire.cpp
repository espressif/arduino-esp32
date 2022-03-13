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
  Modified Nov 2021 by Hristo Gochkov <Me-No-Dev> to support ESP-IDF API
 */

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
}

#include "esp32-hal-i2c.h"
#include "esp32-hal-i2c-slave.h"
#include "Wire.h"
#include "Arduino.h"

TwoWire::TwoWire(uint8_t bus_num)
    :num(bus_num & 1)
    ,sda(-1)
    ,scl(-1)
    ,rxIndex(0)
    ,rxLength(0)
    ,txLength(0)
    ,txAddress(0)
    ,_timeOutMillis(50)
    ,nonStop(false)
#if !CONFIG_DISABLE_HAL_LOCKS
    ,nonStopTask(NULL)
    ,lock(NULL)
#endif
    ,is_slave(false)
    ,user_onRequest(NULL)
    ,user_onReceive(NULL)
{}

TwoWire::~TwoWire()
{
    end();
#if !CONFIG_DISABLE_HAL_LOCKS
    if(lock != NULL){
        vSemaphoreDelete(lock);
    }
#endif
}

bool TwoWire::initPins(int sdaPin, int sclPin)
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
                log_e("no Default SDA Pin for Second Peripheral");
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
                log_e("no Default SCL Pin for Second Peripheral");
                return false; //no Default pin for Second Peripheral
            } else {
                sclPin = scl;    // reuse prior pin
            }
        }
    }

    sda = sdaPin;
    scl = sclPin;
    return true;
}

bool TwoWire::setPins(int sdaPin, int sclPin)
{
#if !CONFIG_DISABLE_HAL_LOCKS
    if(lock == NULL){
        lock = xSemaphoreCreateMutex();
        if(lock == NULL){
            log_e("xSemaphoreCreateMutex failed");
            return false;
        }
    }
    //acquire lock
    if(xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
        log_e("could not acquire lock");
        return false;
    }
#endif
    if(!i2cIsInit(num)){
        initPins(sdaPin, sclPin);
    } else {
        log_e("bus already initialized. change pins only when not.");
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
#endif
    return !i2cIsInit(num);
}

// Slave Begin
bool TwoWire::begin(uint8_t addr, int sdaPin, int sclPin, uint32_t frequency)
{
    bool started = false;
#if !CONFIG_DISABLE_HAL_LOCKS
    if(lock == NULL){
        lock = xSemaphoreCreateMutex();
        if(lock == NULL){
            log_e("xSemaphoreCreateMutex failed");
            return false;
        }
    }
    //acquire lock
    if(xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
        log_e("could not acquire lock");
        return false;
    }
#endif
    if(is_slave){
        log_w("Bus already started in Slave Mode.");
        started = true;
        goto end;
    }
    if(i2cIsInit(num)){
        log_e("Bus already started in Master Mode.");
        goto end;
    }
    if(!initPins(sdaPin, sclPin)){
        goto end;
    }
    i2cSlaveAttachCallbacks(num, onRequestService, onReceiveService, this);
    if(i2cSlaveInit(num, sda, scl, addr, frequency, I2C_BUFFER_LENGTH, I2C_BUFFER_LENGTH) != ESP_OK){
        log_e("Slave Init ERROR");
        goto end;
    }
    is_slave = true;
    started = true;
end:
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
#endif
    return started;
}

// Master Begin
bool TwoWire::begin(int sdaPin, int sclPin, uint32_t frequency)
{
    bool started = false;
    esp_err_t err = ESP_OK;
#if !CONFIG_DISABLE_HAL_LOCKS
    if(lock == NULL){
        lock = xSemaphoreCreateMutex();
        if(lock == NULL){
            log_e("xSemaphoreCreateMutex failed");
            return false;
        }
    }
    //acquire lock
    if(xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
        log_e("could not acquire lock");
        return false;
    }
#endif
    if(is_slave){
        log_e("Bus already started in Slave Mode.");
        goto end;
    }
    if(i2cIsInit(num)){
        log_w("Bus already started in Master Mode.");
        started = true;
        goto end;
    }
    if(!initPins(sdaPin, sclPin)){
        goto end;
    }
    err = i2cInit(num, sda, scl, frequency);
    started = (err == ESP_OK);

end:
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
#endif
    return started;

}

bool TwoWire::end()
{
    esp_err_t err = ESP_OK;
#if !CONFIG_DISABLE_HAL_LOCKS
    if(lock != NULL){
        //acquire lock
        if(xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
            log_e("could not acquire lock");
            return false;
        }
#endif
        if(is_slave){
            err = i2cSlaveDeinit(num);
            if(err == ESP_OK){
                is_slave = false;
            }
        } else if(i2cIsInit(num)){
            err = i2cDeinit(num);
        }
#if !CONFIG_DISABLE_HAL_LOCKS
        //release lock
        xSemaphoreGive(lock);
    }
#endif
    return (err == ESP_OK);
}

uint32_t TwoWire::getClock()
{
    uint32_t frequency = 0;
#if !CONFIG_DISABLE_HAL_LOCKS
    //acquire lock
    if(lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
        log_e("could not acquire lock");
    } else {
#endif
        if(is_slave){
            log_e("Bus is in Slave Mode");
        } else {
            i2cGetClock(num, &frequency);
        }
#if !CONFIG_DISABLE_HAL_LOCKS
        //release lock
        xSemaphoreGive(lock);
    }
#endif
    return frequency;
}

bool TwoWire::setClock(uint32_t frequency)
{
    esp_err_t err = ESP_OK;
#if !CONFIG_DISABLE_HAL_LOCKS
    //acquire lock
    if(lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
        log_e("could not acquire lock");
        return false;
    }
#endif
    if(is_slave){
        log_e("Bus is in Slave Mode");
        err = ESP_FAIL;
    } else {
        err = i2cSetClock(num, frequency);
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
#endif
    return (err == ESP_OK);
}

void TwoWire::setTimeOut(uint16_t timeOutMillis)
{
    _timeOutMillis = timeOutMillis;
}

uint16_t TwoWire::getTimeOut()
{
    return _timeOutMillis;
}

void TwoWire::beginTransmission(uint16_t address)
{
    if(is_slave){
        log_e("Bus is in Slave Mode");
        return;
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    if(nonStop && nonStopTask == xTaskGetCurrentTaskHandle()){
        log_e("Unfinished Repeated Start transaction! Expected requestFrom, not beginTransmission! Clearing...");
        //release lock
        xSemaphoreGive(lock);
    }
    //acquire lock
    if(lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
        log_e("could not acquire lock");
        return;
    }
#endif
    nonStop = false;
    txAddress = address;
    txLength = 0;
}

uint8_t TwoWire::endTransmission(bool sendStop)
{
    if(is_slave){
        log_e("Bus is in Slave Mode");
        return 4;
    }
    esp_err_t err = ESP_OK;
    if(sendStop){
        err = i2cWrite(num, txAddress, txBuffer, txLength, _timeOutMillis);
#if !CONFIG_DISABLE_HAL_LOCKS
        //release lock
        xSemaphoreGive(lock);
#endif
    } else {
        //mark as non-stop
        nonStop = true;
#if !CONFIG_DISABLE_HAL_LOCKS
        nonStopTask = xTaskGetCurrentTaskHandle();
#endif
    }
    switch(err){
        case ESP_OK: return 0;
        case ESP_FAIL: return 2;
        case ESP_ERR_TIMEOUT: return 5;
        default: break;
    }
    return 4;
}

size_t TwoWire::requestFrom(uint16_t address, size_t size, bool sendStop)
{
    if(is_slave){
        log_e("Bus is in Slave Mode");
        return 0;
    }
    esp_err_t err = ESP_OK;
    if(nonStop
#if !CONFIG_DISABLE_HAL_LOCKS
    && nonStopTask == xTaskGetCurrentTaskHandle()
#endif
    ){
        if(address != txAddress){
            log_e("Unfinished Repeated Start transaction! Expected address do not match! %u != %u", address, txAddress);
            return 0;
        }
        nonStop = false;
        rxIndex = 0;
        rxLength = 0;
        err = i2cWriteReadNonStop(num, address, txBuffer, txLength, rxBuffer, size, _timeOutMillis, &rxLength);
    } else {
#if !CONFIG_DISABLE_HAL_LOCKS
        //acquire lock
        if(lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE){
            log_e("could not acquire lock");
            return 0;
        }
#endif
        rxIndex = 0;
        rxLength = 0;
        err = i2cRead(num, address, rxBuffer, size, _timeOutMillis, &rxLength);
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
#endif
    return rxLength;
}

size_t TwoWire::write(uint8_t data)
{
    if(txLength >= I2C_BUFFER_LENGTH) {
        return 0;
    }
    txBuffer[txLength++] = data;
    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    for(size_t i = 0; i < quantity; ++i) {
        if(!write(data[i])) {
            return i;
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
        value = rxBuffer[rxIndex++];
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
    txLength = 0;
    //i2cFlush(num); // cleanup
}

size_t TwoWire::requestFrom(uint8_t address, size_t len, bool sendStop)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(len), static_cast<bool>(sendStop));
}
  
uint8_t TwoWire::requestFrom(uint8_t address, uint8_t len, uint8_t sendStop)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(len), static_cast<bool>(sendStop));
}

uint8_t TwoWire::requestFrom(uint16_t address, uint8_t len, uint8_t sendStop)
{
    return requestFrom(address, static_cast<size_t>(len), static_cast<bool>(sendStop));
}

/* Added to match the Arduino function definition: https://github.com/arduino/ArduinoCore-API/blob/173e8eadced2ad32eeb93bcbd5c49f8d6a055ea6/api/HardwareI2C.h#L39
 * See: https://github.com/arduino-libraries/ArduinoECCX08/issues/25
*/
uint8_t TwoWire::requestFrom(uint16_t address, uint8_t len, bool stopBit)
{
    return requestFrom((uint16_t)address, (size_t)len, stopBit);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t len)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(len), true);
}

uint8_t TwoWire::requestFrom(uint16_t address, uint8_t len)
{
    return requestFrom(address, static_cast<size_t>(len), true);
}

uint8_t TwoWire::requestFrom(int address, int len)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(len), true);
}

uint8_t TwoWire::requestFrom(int address, int len, int sendStop)
{
    return static_cast<uint8_t>(requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(len), static_cast<bool>(sendStop)));
}

void TwoWire::beginTransmission(int address)
{
    beginTransmission(static_cast<uint16_t>(address));
}

void TwoWire::beginTransmission(uint8_t address)
{
    beginTransmission(static_cast<uint16_t>(address));
}

uint8_t TwoWire::endTransmission(void)
{
    return endTransmission(true);
}

size_t TwoWire::slaveWrite(const uint8_t * buffer, size_t len)
{
    return i2cSlaveWrite(num, buffer, len, _timeOutMillis);
}

void TwoWire::onReceiveService(uint8_t num, uint8_t* inBytes, size_t numBytes, bool stop, void * arg)
{
    TwoWire * wire = (TwoWire*)arg;
    if(!wire->user_onReceive){
        return;
    }
    for(uint8_t i = 0; i < numBytes; ++i){
        wire->rxBuffer[i] = inBytes[i];    
    }
    wire->rxIndex = 0;
    wire->rxLength = numBytes;
    wire->user_onReceive(numBytes);
}

void TwoWire::onRequestService(uint8_t num, void * arg)
{
    TwoWire * wire = (TwoWire*)arg;
    if(!wire->user_onRequest){
        return;
    }
    wire->txLength = 0;
    wire->user_onRequest();
    if(wire->txLength){
        wire->slaveWrite((uint8_t*)wire->txBuffer, wire->txLength);
    }
}

void TwoWire::onReceive( void (*function)(int) )
{
  user_onReceive = function;
}

// sets function called on slave read
void TwoWire::onRequest( void (*function)(void) )
{
  user_onRequest = function;
}


TwoWire Wire = TwoWire(0);
TwoWire Wire1 = TwoWire(1);
