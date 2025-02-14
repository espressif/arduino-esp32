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
#include "soc/soc_caps.h"
#if SOC_I2C_SUPPORTED

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
}

#include "esp32-hal-i2c.h"
#if SOC_I2C_SUPPORT_SLAVE
#include "esp32-hal-i2c-slave.h"
#endif /* SOC_I2C_SUPPORT_SLAVE */
#include "Wire.h"
#include "Arduino.h"

TwoWire::TwoWire(uint8_t bus_num)
  : num(bus_num & 1), sda(-1), scl(-1), bufferSize(I2C_BUFFER_LENGTH)  // default Wire Buffer Size
    ,
    rxBuffer(NULL), rxIndex(0), rxLength(0), txBuffer(NULL), txLength(0), txAddress(0), _timeOutMillis(50), nonStop(false)
#if !CONFIG_DISABLE_HAL_LOCKS
    ,
    currentTaskHandle(NULL), lock(NULL)
#endif
#if SOC_I2C_SUPPORT_SLAVE
    ,
    is_slave(false), user_onRequest(NULL), user_onReceive(NULL)
#endif /* SOC_I2C_SUPPORT_SLAVE */
{
}

TwoWire::~TwoWire() {
  end();
#if !CONFIG_DISABLE_HAL_LOCKS
  if (lock != NULL) {
    vSemaphoreDelete(lock);
  }
#endif
}

bool TwoWire::initPins(int sdaPin, int sclPin) {
  if (sdaPin < 0) {  // default param passed
    if (num == 0) {
      if (sda == -1) {
        sdaPin = SDA;  //use Default Pin
      } else {
        sdaPin = sda;  // reuse prior pin
      }
    } else {
      if (sda == -1) {
#ifdef WIRE1_PIN_DEFINED
        sdaPin = SDA1;
#else
        log_e("no Default SDA Pin for Second Peripheral");
        return false;  //no Default pin for Second Peripheral
#endif
      } else {
        sdaPin = sda;  // reuse prior pin
      }
    }
  }

  if (sclPin < 0) {  // default param passed
    if (num == 0) {
      if (scl == -1) {
        sclPin = SCL;  // use Default pin
      } else {
        sclPin = scl;  // reuse prior pin
      }
    } else {
      if (scl == -1) {
#ifdef WIRE1_PIN_DEFINED
        sclPin = SCL1;
#else
        log_e("no Default SCL Pin for Second Peripheral");
        return false;  //no Default pin for Second Peripheral
#endif
      } else {
        sclPin = scl;  // reuse prior pin
      }
    }
  }

  sda = sdaPin;
  scl = sclPin;
  return true;
}

bool TwoWire::setPins(int sdaPin, int sclPin) {
#if !CONFIG_DISABLE_HAL_LOCKS
  if (lock == NULL) {
    lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return false;
    }
  }
  //acquire lock
  if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return false;
  }
#endif
  if (!i2cIsInit(num)) {
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

bool TwoWire::allocateWireBuffer() {
  // or both buffer can be allocated or none will be
  if (rxBuffer == NULL) {
    rxBuffer = (uint8_t *)malloc(bufferSize);
    if (rxBuffer == NULL) {
      log_e("Can't allocate memory for I2C_%d rxBuffer", num);
      return false;
    }
  }
  if (txBuffer == NULL) {
    txBuffer = (uint8_t *)malloc(bufferSize);
    if (txBuffer == NULL) {
      log_e("Can't allocate memory for I2C_%d txBuffer", num);
      freeWireBuffer();  // free rxBuffer for safety!
      return false;
    }
  }
  // in case both were allocated before, they must have the same size. All good.
  return true;
}

void TwoWire::freeWireBuffer() {
  if (rxBuffer != NULL) {
    free(rxBuffer);
    rxBuffer = NULL;
  }
  if (txBuffer != NULL) {
    free(txBuffer);
    txBuffer = NULL;
  }
}

size_t TwoWire::setBufferSize(size_t bSize) {
  // Maximum size .... HEAP limited ;-)
  if (bSize < 32) {  // 32 bytes is the I2C FIFO Len for ESP32/S2/S3/C3
    log_e("Minimum Wire Buffer size is 32 bytes");
    return 0;
  }

#if !CONFIG_DISABLE_HAL_LOCKS
  if (lock == NULL) {
    lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return 0;
    }
  }
  //acquire lock
  if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return 0;
  }
#endif
  // allocateWireBuffer allocates memory for both pointers or just free them
  if (rxBuffer != NULL || txBuffer != NULL) {
    // if begin() has been already executed, memory size changes... data may be lost. We don't care! :^)
    if (bSize != bufferSize) {
      // we want a new buffer size ... just reset buffer pointers and allocate new ones
      freeWireBuffer();
      bufferSize = bSize;
      if (!allocateWireBuffer()) {
        // failed! Error message already issued
        bSize = 0;  // returns error
        log_e("Buffer allocation failed");
      }
    }  // else nothing changes, all set!
  } else {
    // no memory allocated yet, just change the size value - allocation in begin()
    bufferSize = bSize;
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(lock);

#endif
  return bSize;
}

#if SOC_I2C_SUPPORT_SLAVE
// Slave Begin
bool TwoWire::begin(uint8_t addr, int sdaPin, int sclPin, uint32_t frequency) {
  bool started = false;
#if !CONFIG_DISABLE_HAL_LOCKS
  if (lock == NULL) {
    lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return false;
    }
  }
  //acquire lock
  if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return false;
  }
#endif
  if (is_slave) {
    log_w("Bus already started in Slave Mode.");
    started = true;
    goto end;
  }
  if (i2cIsInit(num)) {
    log_e("Bus already started in Master Mode.");
    goto end;
  }
  if (!allocateWireBuffer()) {
    // failed! Error Message already issued
    goto end;
  }
  if (!initPins(sdaPin, sclPin)) {
    goto end;
  }
  i2cSlaveAttachCallbacks(num, onRequestService, onReceiveService, this);
  if (i2cSlaveInit(num, sda, scl, addr, frequency, bufferSize, bufferSize) != ESP_OK) {
    log_e("Slave Init ERROR");
    goto end;
  }
  is_slave = true;
  started = true;
end:
  if (!started) {
    freeWireBuffer();
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(lock);
#endif
  return started;
}
#endif /* SOC_I2C_SUPPORT_SLAVE */

// Master Begin
bool TwoWire::begin(int sdaPin, int sclPin, uint32_t frequency) {
  bool started = false;
  esp_err_t err = ESP_OK;
#if !CONFIG_DISABLE_HAL_LOCKS
  if (lock == NULL) {
    lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return false;
    }
  }
  //acquire lock
  if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return false;
  }
#endif
#if SOC_I2C_SUPPORT_SLAVE
  if (is_slave) {
    log_e("Bus already started in Slave Mode.");
    goto end;
  }
#endif /* SOC_I2C_SUPPORT_SLAVE */
  if (i2cIsInit(num)) {
    log_w("Bus already started in Master Mode.");
    started = true;
    goto end;
  }
  if (!allocateWireBuffer()) {
    // failed! Error Message already issued
    goto end;
  }
  if (!initPins(sdaPin, sclPin)) {
    goto end;
  }
  err = i2cInit(num, sda, scl, frequency);
  started = (err == ESP_OK);

end:
  if (!started) {
    freeWireBuffer();
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(lock);
#endif
  return started;
}

bool TwoWire::end() {
  esp_err_t err = ESP_OK;
#if !CONFIG_DISABLE_HAL_LOCKS
  if (lock != NULL) {
    //acquire lock
    if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
      log_e("could not acquire lock");
      return false;
    }
#endif
#if SOC_I2C_SUPPORT_SLAVE
    if (is_slave) {
      err = i2cSlaveDeinit(num);
      if (err == ESP_OK) {
        is_slave = false;
      }
    } else
#endif /* SOC_I2C_SUPPORT_SLAVE */
      if (i2cIsInit(num)) {
        err = i2cDeinit(num);
      }
    freeWireBuffer();
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
  }
#endif
  return (err == ESP_OK);
}

uint32_t TwoWire::getClock() {
  uint32_t frequency = 0;
#if !CONFIG_DISABLE_HAL_LOCKS
  //acquire lock
  if (lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
  } else {
#endif
#if SOC_I2C_SUPPORT_SLAVE
    if (is_slave) {
      log_e("Bus is in Slave Mode");
    } else
#endif /* SOC_I2C_SUPPORT_SLAVE */
    {
      i2cGetClock(num, &frequency);
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    //release lock
    xSemaphoreGive(lock);
  }
#endif
  return frequency;
}

bool TwoWire::setClock(uint32_t frequency) {
  esp_err_t err = ESP_OK;
#if !CONFIG_DISABLE_HAL_LOCKS
  //acquire lock
  if (lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return false;
  }
#endif
#if SOC_I2C_SUPPORT_SLAVE
  if (is_slave) {
    log_e("Bus is in Slave Mode");
    err = ESP_FAIL;
  } else
#endif /* SOC_I2C_SUPPORT_SLAVE */
  {
    err = i2cSetClock(num, frequency);
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(lock);
#endif
  return (err == ESP_OK);
}

void TwoWire::setTimeOut(uint16_t timeOutMillis) {
  _timeOutMillis = timeOutMillis;
}

uint16_t TwoWire::getTimeOut() {
  return _timeOutMillis;
}

void TwoWire::beginTransmission(uint8_t address) {
#if SOC_I2C_SUPPORT_SLAVE
  if (is_slave) {
    log_e("Bus is in Slave Mode");
    return;
  }
#endif /* SOC_I2C_SUPPORT_SLAVE */
#if !CONFIG_DISABLE_HAL_LOCKS
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  if (currentTaskHandle != task) {
    //acquire lock
    if (lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
      log_e("could not acquire lock");
      return;
    }
    currentTaskHandle = task;
  }
#endif
  nonStop = false;
  txAddress = address;
  txLength = 0;
}

/*
https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
endTransmission() returns:
0: success.
1: data too long to fit in transmit buffer.
2: received NACK on transmit of address.
3: received NACK on transmit of data.
4: other error.
5: timeout
*/
uint8_t TwoWire::endTransmission(bool sendStop) {
#if SOC_I2C_SUPPORT_SLAVE
  if (is_slave) {
    log_e("Bus is in Slave Mode");
    return 4;
  }
#endif /* SOC_I2C_SUPPORT_SLAVE */
  if (txBuffer == NULL) {
    log_e("NULL TX buffer pointer");
    return 4;
  }
  esp_err_t err = ESP_OK;
  if (sendStop) {
    err = i2cWrite(num, txAddress, txBuffer, txLength, _timeOutMillis);
#if !CONFIG_DISABLE_HAL_LOCKS
    currentTaskHandle = NULL;
    //release lock
    xSemaphoreGive(lock);
#endif
  } else {
    //mark as non-stop
    nonStop = true;
  }
  switch (err) {
    case ESP_OK:            return 0;
    case ESP_FAIL:          return 2;
    case ESP_ERR_NOT_FOUND: return 2;
    case ESP_ERR_TIMEOUT:   return 5;
    default:                break;
  }
  return 4;
}

uint8_t TwoWire::endTransmission() {
  return endTransmission(true);
}

size_t TwoWire::requestFrom(uint8_t address, size_t size, bool sendStop) {
#if SOC_I2C_SUPPORT_SLAVE
  if (is_slave) {
    log_e("Bus is in Slave Mode");
    return 0;
  }
#endif /* SOC_I2C_SUPPORT_SLAVE */
  if (rxBuffer == NULL || txBuffer == NULL) {
    log_e("NULL buffer pointer");
    return 0;
  }
  esp_err_t err = ESP_OK;
#if !CONFIG_DISABLE_HAL_LOCKS
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  if (currentTaskHandle != task) {
    //acquire lock
    if (lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
      log_e("could not acquire lock");
      return 0;
    }
    currentTaskHandle = task;
  }
#endif
  if (nonStop) {
    if (address != txAddress) {
      log_e("Unfinished Repeated Start transaction! Expected address do not match! %u != %u", address, txAddress);
#if !CONFIG_DISABLE_HAL_LOCKS
      currentTaskHandle = NULL;
      //release lock
      xSemaphoreGive(lock);
#endif
      return 0;
    }
    nonStop = false;
    rxIndex = 0;
    rxLength = 0;
    err = i2cWriteReadNonStop(num, address, txBuffer, txLength, rxBuffer, size, _timeOutMillis, &rxLength);
    if (err) {
      log_e("i2cWriteReadNonStop returned Error %d", err);
    }
  } else {
    rxIndex = 0;
    rxLength = 0;
    err = i2cRead(num, address, rxBuffer, size, _timeOutMillis, &rxLength);
    if (err) {
      log_e("i2cRead returned Error %d", err);
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  currentTaskHandle = NULL;
  //release lock
  xSemaphoreGive(lock);
#endif
  return rxLength;
}

size_t TwoWire::requestFrom(uint8_t address, size_t size) {
  return requestFrom(address, size, true);
}

size_t TwoWire::write(uint8_t data) {
  if (txBuffer == NULL) {
    log_e("NULL TX buffer pointer");
    return 0;
  }
  if (txLength >= bufferSize) {
    return 0;
  }
  txBuffer[txLength++] = data;
  return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity) {
  for (size_t i = 0; i < quantity; ++i) {
    if (!write(data[i])) {
      return i;
    }
  }
  return quantity;
}

int TwoWire::available() {
  int result = rxLength - rxIndex;
  return result;
}

int TwoWire::read() {
  int value = -1;
  if (rxBuffer == NULL) {
    log_e("NULL RX buffer pointer");
    return value;
  }
  if (rxIndex < rxLength) {
    value = rxBuffer[rxIndex++];
  }
  return value;
}

int TwoWire::peek() {
  int value = -1;
  if (rxBuffer == NULL) {
    log_e("NULL RX buffer pointer");
    return value;
  }
  if (rxIndex < rxLength) {
    value = rxBuffer[rxIndex];
  }
  return value;
}

void TwoWire::flush() {
  rxIndex = 0;
  rxLength = 0;
  txLength = 0;
  //i2cFlush(num); // cleanup
}

void TwoWire::onReceive(void (*function)(int)) {
#if SOC_I2C_SUPPORT_SLAVE
  user_onReceive = function;
#endif
}

// sets function called on slave read
void TwoWire::onRequest(void (*function)(void)) {
#if SOC_I2C_SUPPORT_SLAVE
  user_onRequest = function;
#endif
}

#if SOC_I2C_SUPPORT_SLAVE

size_t TwoWire::slaveWrite(const uint8_t *buffer, size_t len) {
  return i2cSlaveWrite(num, buffer, len, _timeOutMillis);
}

void TwoWire::onReceiveService(uint8_t num, uint8_t *inBytes, size_t numBytes, bool stop, void *arg) {
  TwoWire *wire = (TwoWire *)arg;
  if (!wire->user_onReceive) {
    return;
  }
  if (wire->rxBuffer == NULL) {
    log_e("NULL RX buffer pointer");
    return;
  }
  for (uint8_t i = 0; i < numBytes; ++i) {
    wire->rxBuffer[i] = inBytes[i];
  }
  wire->rxIndex = 0;
  wire->rxLength = numBytes;
  wire->user_onReceive(numBytes);
}

void TwoWire::onRequestService(uint8_t num, void *arg) {
  TwoWire *wire = (TwoWire *)arg;
  if (!wire->user_onRequest) {
    return;
  }
  if (wire->txBuffer == NULL) {
    log_e("NULL TX buffer pointer");
    return;
  }
  wire->txLength = 0;
  wire->user_onRequest();
  if (wire->txLength) {
    wire->slaveWrite((uint8_t *)wire->txBuffer, wire->txLength);
  }
}

#endif /* SOC_I2C_SUPPORT_SLAVE */

TwoWire Wire = TwoWire(0);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#if SOC_I2C_NUM > 1
TwoWire Wire1 = TwoWire(1);
#elif SOC_I2C_NUM > 2
TwoWire Wire2 = TwoWire(2);
#endif /* SOC_I2C_NUM */
#else
#if SOC_HP_I2C_NUM > 1
TwoWire Wire1 = TwoWire(1);
#endif /* SOC_HP_I2C_NUM */
#endif

#endif /* SOC_I2C_SUPPORTED */
