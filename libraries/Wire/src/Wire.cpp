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
        sda = sdaPin;
        scl = sclPin;
    } else {
        log_e("bus already initialized. change pins only when not.");
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
#endif
    return !i2cIsInit(num);
}

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
    if(i2cIsInit(num)){
        started = true;
        goto end;
    }
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
                goto end; //no Default pin for Second Peripheral
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
                goto end; //no Default pin for Second Peripheral
            } else {
                sclPin = scl;    // reuse prior pin
            }
        }
    }

    sda = sdaPin;
    scl = sclPin;
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
        if(i2cIsInit(num)){
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
        i2cGetClock(num, &frequency);
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
    err = i2cSetClock(num, frequency);
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

uint8_t TwoWire::requestFrom(uint16_t address, uint8_t size, bool sendStop)
{
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

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(quantity), static_cast<bool>(sendStop));
}

uint8_t TwoWire::requestFrom(uint16_t address, uint8_t quantity, uint8_t sendStop)
{
    return requestFrom(address, static_cast<size_t>(quantity), static_cast<bool>(sendStop));
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(quantity), true);
}

uint8_t TwoWire::requestFrom(uint16_t address, uint8_t quantity)
{
    return requestFrom(address, static_cast<size_t>(quantity), true);
}

uint8_t TwoWire::requestFrom(int address, int quantity)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(quantity), true);
}

uint8_t TwoWire::requestFrom(int address, int quantity, int sendStop)
{
    return static_cast<uint8_t>(requestFrom(static_cast<uint16_t>(address), static_cast<size_t>(quantity), static_cast<bool>(sendStop)));
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

TwoWire Wire = TwoWire(0);
TwoWire Wire1 = TwoWire(1);
