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
  Modified Nov 2017 by Chuck Todd (ctodd@cableone.net) - ESP32 support
*/

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
}

#include "esp32-hal-i2c.h"
#include "Wire.h"
#include "Arduino.h"
/* Declarations to support Slave Mode */
#include "esp_attr.h"
#include "soc/i2c_reg.h"
#include "soc/i2c_struct.h"

user_onRequest TwoWire::uReq[2];
user_onReceive TwoWire::uRcv[2];

#define DR_REG_I2C_EXT_BASE_FIXED               0x60013000
#define DR_REG_I2C1_EXT_BASE_FIXED              0x60027000

/*
	
void IRAM_ATTR slave_isr_handler(void* arg){
    // ...
uint32_t num = (uint32_t)arg;
i2c_dev_t * dev;
if(num==0) dev=(volatile i2c_dev_t*)DR_REG_I2C_EXT_BASE_FIXED;
else dev=(volatile i2c_dev_t*)DR_REG_I2C1_EXT_BASE_FIXED;

uint32_t stat = dev->int_status.val;		
		
}
*/


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
    ,txQueued(0)
    ,rxQueued(0)
		{}	

void TwoWire::onRequestService(void){}
void TwoWire::onReceiveService(uint8_t*buf, int count){}

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
		uReq[num] =NULL;
		uRcv[num] =NULL;

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
/* Original requestFrom() 11/2017 before ISR
*/
size_t TwoWire::oldRequestFrom(uint8_t address, size_t size, bool sendStop)
{
    i2cReleaseISR(i2c);

    if(size > I2C_BUFFER_LENGTH) {
        size = I2C_BUFFER_LENGTH;
    }
    last_error = i2cRead(i2c, address, false, rxBuffer, size, sendStop);
		size_t read = (last_error == 0)?size:0;
    rxIndex = 0;
    rxLength = read;
    return read;
}

/* @stickBreaker 11/2017 fix for ReSTART timeout, ISR
*/
size_t TwoWire::requestFrom(uint8_t address, size_t size, bool sendStop){

    uint16_t cnt = rxQueued; // currently queued reads 
    if(cnt<I2C_BUFFER_LENGTH){
      if((size+cnt)>I2C_BUFFER_LENGTH)
        size = (I2C_BUFFER_LENGTH-cnt);
      }
    else { // no room to receive more!
      log_e("no room %d",cnt);
      cnt = 0;
      rxIndex = 0;
      rxLength = 0;
      rxQueued = 0;
      last_error = I2C_ERROR_MEMORY;
      i2cFreeQueue(i2c);
      return cnt;
      }
      
    last_error =i2cAddQueueRead(i2c,address,&rxBuffer[cnt],size,sendStop,NULL);
    if(last_error==I2C_ERROR_OK){ // successfully queued the read
      rxQueued += size;
      if(sendStop){ //now actually process the queued commands
        last_error=i2cProcQueue(i2c);
        rxIndex = 0;
        rxLength = i2cQueueReadCount(i2c);
        rxQueued = 0;
        cnt = rxLength;
        i2cFreeQueue(i2c);
        }
      else { // stop not received, so wait for I2C stop,
        last_error=I2C_ERROR_CONTINUE;
        cnt = 0;
        }
      }
    else {// only possible error is I2C_ERROR_MEMORY
      cnt = 0;
      }
    return cnt;
}

size_t TwoWire::requestFrom(uint8_t address, uint8_t * readBuff, size_t size, bool sendStop){
    size_t cnt=0;
    last_error =i2cAddQueueRead(i2c,address,readBuff,size,sendStop,NULL);
    if(last_error==I2C_ERROR_OK){ // successfully queued the read
      if(sendStop){ //now actually process the queued commands
        last_error=i2cProcQueue(i2c);
        rxIndex = 0;
        rxLength = 0;
        txQueued = 0; // the SendStop=true will restart are Queueing 
        rxQueued = 0;
        cnt = i2cQueueReadCount(i2c); 

  //what about mix local buffers and Wire Buffers?
  //Handled it by verifying location, only contiguous sections of
  //rxBuffer are counted.
        uint8_t *savePtr;
        uint16_t len;
        uint8_t idx=0;
        while(I2C_ERROR_OK==i2cGetReadQueue(i2c,&savePtr,&len,&idx)){
          if(savePtr==(rxBuffer+rxLength)){
            rxLength = rxLength + len;
            }
          }
        i2cFreeQueue(i2c);
        }
      else { // stop not received, so wait for I2C stop,
        last_error=I2C_ERROR_CONTINUE;
        cnt = 0;
        }
      }
    else {// only possible error is I2C_ERROR_MEMORY
      cnt = 0;
      }
    return cnt;
}

uint8_t TwoWire::writeTransaction(uint8_t address, uint8_t *buff, size_t size, bool sendStop){
// will destroy any partially created beginTransaction()

last_error=i2cAddQueueWrite(i2c,address,buff,size,sendStop,NULL);

if(last_error==I2C_ERROR_OK){ //queued
  if(sendStop){ //now actually process the queued commands, including READs
    last_error=i2cProcQueue(i2c);
    rxIndex = 0;
    rxLength = 0;
    txQueued = 0; // the SendStop=true will restart all Queueing 
    rxQueued = 0;
    
  //what about mix local buffers and Wire Buffers?
  //Handled it by verifying location, only contiguous sections of
  //rxBuffer are counted.
    uint8_t *savePtr;
    uint16_t len;
    uint8_t idx=0;
    while(I2C_ERROR_OK==i2cGetReadQueue(i2c,&savePtr,&len,&idx)){
      if(savePtr==(rxBuffer+rxLength)){
        rxLength = rxLength + len;
        }
      }
    i2cFreeQueue(i2c);
    }
  else { // stop not received, so wait for I2C stop,
    last_error=I2C_ERROR_CONTINUE;
    }
  }
txIndex=0;
txLength=0;
transmitting = 0;
return static_cast<uint8_t>(last_error);
}

/*stickbreaker Dump i2c Interrupt buffer, i2c isr Debugging
*/
void TwoWire::dumpInts(){
  i2cDumpInts();
}

/*stickbreaker i2c isr Debugging
*/
size_t TwoWire::getClock(){
  return i2cGetFrequency(i2c); 
}

/*stickbreaker using the i2c hardware without an ISR
*/
size_t TwoWire::polledRequestFrom(uint8_t address, uint8_t* buf, size_t size, bool sendStop){

    i2cReleaseISR(i2c);

    rxIndex = 0;
    rxLength = 0;
	 if(size==0){
		 return 0;
		}
		last_error =pollI2cRead(i2c, address, false, buf, size, sendStop);
    if(last_error!=0){
			log_e("err=%d",last_error);
			}
		size_t read = (last_error==0)?size:0; 	
    return read;
}

/*stickbreaker simple ReSTART handling using internal Wire data buffers
*/
size_t TwoWire::transact(size_t readLen){ // Assumes Wire.beginTransaction(),Wire.write()
// this command replaces Wire.endTransmission(false) and Wire.requestFrom(readLen,true);
if(transmitting){
  last_error = static_cast<i2c_err_t>(endTransmission(false));
  }
  
if(last_error==I2C_ERROR_CONTINUE){ // must have queued the Write
  size_t cnt = requestFrom(txAddress,readLen,true);
  return cnt;
  }
else {
  last_error = I2C_ERROR_MISSING_WRITE;
  return 0;
  }
}

/*stickbreaker isr ReSTART with external read Buffer
*/
size_t TwoWire::transact(uint8_t * readBuff, size_t readLen){ // Assumes Wire.beginTransaction(),Wire.write()
// this command replaces Wire.endTransmission(false) and Wire.requestFrom(readLen,true);
if(transmitting){
  last_error = static_cast<i2c_err_t>(endTransmission(false));
  }

if(last_error==I2C_ERROR_CONTINUE){ // must have queued the write
  size_t cnt = requestFrom(txAddress,readBuff,readLen,true);
  return cnt;
  }
else {
  last_error = I2C_ERROR_MISSING_WRITE;
  return 0;
  }
}

/*stickbreaker isr
*/
uint8_t TwoWire::endTransmission(uint8_t sendStop){ // Assumes Wire.beginTransaction(), Wire.write()
// this command replaces Wire.endTransmission(true)

if(transmitting==1){
  last_error =i2cAddQueueWrite(i2c,txAddress,&txBuffer[txQueued],txLength,sendStop,NULL);  //queue tx element

  if(last_error == I2C_ERROR_OK){
    if(sendStop){
      last_error=i2cProcQueue(i2c);
      txQueued = 0;
      i2cFreeQueue(i2c);
      }
    else { // queued because it had sendStop==false
      // txlength is howmany bytes in txbufferhave been use
      txQueued = txLength;
      last_error = I2C_ERROR_CONTINUE;
      }
    }
  }
else {
  last_error= I2C_ERROR_NO_BEGIN;
  txQueued = 0;
  i2cFreeQueue(i2c); // cleanup
  }
txIndex = 0;
txLength =0;
transmitting = 0;
return last_error;
}

i2c_err_t TwoWire::lastError(){
	return last_error;
}

uint8_t TwoWire::oldEndTransmission(uint8_t sendStop)
{
    //disable ISR
    i2cReleaseISR(i2c);
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
    return static_cast<uint8_t>(requestFrom(static_cast<uint8_t>(address), static_cast<size_t>(quantity), static_cast<bool>(sendStop)));
}

void TwoWire::beginTransmission(uint8_t address)
{
    transmitting = 1;
    txAddress = address;
    txIndex = txQueued; // allow multiple beginTransmission(),write(),endTransmission(false) until endTransmission(true)
    txLength = txQueued;
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
    rxQueued = 0;
    txQueued = 0;
    i2cFreeQueue(i2c); // cleanup
}

void TwoWire::reset(void)
{
    i2cReset( i2c );
    i2c = NULL;
    begin( sda, scl );
}

TwoWire Wire = TwoWire(0);
