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

#include "soc/soc_caps.h"
#if SOC_I2C_SUPPORTED

#include <esp32-hal.h>
#include <esp32-hal-log.h>
#if !CONFIG_DISABLE_HAL_LOCKS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif
#include "HardwareI2C.h"
#include "Stream.h"

// WIRE_HAS_BUFFER_SIZE means Wire has setBufferSize()
#define WIRE_HAS_BUFFER_SIZE 1
// WIRE_HAS_END means Wire has end()
#define WIRE_HAS_END 1

#ifndef I2C_BUFFER_LENGTH
#define I2C_BUFFER_LENGTH 128  // Default size, if none is set using Wire::setBuffersize(size_t)
#endif
#if SOC_I2C_SUPPORT_SLAVE
typedef void (*user_onRequest)(void);
typedef void (*user_onReceive)(uint8_t *, int);
#endif /* SOC_I2C_SUPPORT_SLAVE */

class TwoWire : public HardwareI2C {
protected:
  uint8_t num;
  int8_t sda;
  int8_t scl;

  size_t bufferSize;
  uint8_t *rxBuffer;
  size_t rxIndex;
  size_t rxLength;

  uint8_t *txBuffer;
  size_t txLength;
  uint16_t txAddress;

  uint32_t _timeOutMillis;
  bool nonStop;
#if !CONFIG_DISABLE_HAL_LOCKS
  TaskHandle_t currentTaskHandle;
  SemaphoreHandle_t lock;
#endif
private:
#if SOC_I2C_SUPPORT_SLAVE
  bool is_slave;
  void (*user_onRequest)(void);
  void (*user_onReceive)(int);
  static void onRequestService(uint8_t, void *);
  static void onReceiveService(uint8_t, uint8_t *, size_t, bool, void *);
#endif /* SOC_I2C_SUPPORT_SLAVE */
  bool initPins(int sdaPin, int sclPin);
  bool allocateWireBuffer();
  void freeWireBuffer();

public:
  TwoWire(uint8_t bus_num);
  ~TwoWire();

  bool begin() override final {
    return begin(-1, -1);
  }

  bool begin(uint8_t address) override final {
#if SOC_I2C_SUPPORT_SLAVE
    return begin(address, -1, -1, 0);
#else
    log_e("I2C slave is not supported on " CONFIG_IDF_TARGET);
    return false;
#endif
  }

  bool end() override;

  bool setClock(uint32_t freq) override;

  void beginTransmission(uint8_t address) override;
  uint8_t endTransmission(bool stopBit) override;
  uint8_t endTransmission() override;

  size_t requestFrom(uint8_t address, size_t len, bool stopBit) override;
  size_t requestFrom(uint8_t address, size_t len) override;

  void onReceive(void (*)(int)) override;
  void onRequest(void (*)(void)) override;

  //call setPins() first, so that begin() can be called without arguments from libraries
  bool setPins(int sda, int scl);

  bool begin(int sda, int scl, uint32_t frequency = 0);  // returns true, if successful init of i2c bus
#if SOC_I2C_SUPPORT_SLAVE
  bool begin(uint8_t slaveAddr, int sda, int scl, uint32_t frequency);
#endif /* SOC_I2C_SUPPORT_SLAVE */

  size_t setBufferSize(size_t bSize);

  void setTimeOut(uint16_t timeOutMillis);  // default timeout of i2c transactions is 50ms
  uint16_t getTimeOut();

  uint32_t getClock();

  size_t write(uint8_t) override;
  size_t write(const uint8_t *, size_t) override;
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;

#if SOC_I2C_SUPPORT_SLAVE
  size_t slaveWrite(const uint8_t *, size_t);
#endif /* SOC_I2C_SUPPORT_SLAVE */
};

extern TwoWire Wire;
#if SOC_I2C_NUM > 1
extern TwoWire Wire1;
#endif /* SOC_I2C_NUM */

#endif /* SOC_I2C_SUPPORTED */
#endif /* TwoWire_h */
