/*
  SPI.h - SPI library for esp32

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

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
#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED

#include "soc/soc_caps.h"
#if SOC_GPSPI_SUPPORTED

#include <stdlib.h>
#include "pins_arduino.h"
#include "esp32-hal-spi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SPI_HAS_TRANSACTION

class SPISettings {
public:
  SPISettings() : _clock(1000000), _bitOrder(SPI_MSBFIRST), _dataMode(SPI_MODE0) {}
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) : _clock(clock), _bitOrder(bitOrder), _dataMode(dataMode) {}
  uint32_t _clock;
  uint8_t _bitOrder;
  uint8_t _dataMode;
};

class SPIClass {
private:
  int8_t _spi_num;
  spi_t *_spi;
  bool _use_hw_ss;
  int8_t _sck;
  int8_t _miso;
  int8_t _mosi;
  int8_t _ss;
  uint32_t _div;
  uint32_t _freq;
  bool _inTransaction;
#if !CONFIG_DISABLE_HAL_LOCKS
  SemaphoreHandle_t paramLock = NULL;
#endif
  void writePattern_(const uint8_t *data, uint8_t size, uint8_t repeat);

public:
  SPIClass(uint8_t spi_bus = HSPI);
  ~SPIClass();
  void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1);
  void end();

  void setHwCs(bool use);
  void setBitOrder(uint8_t bitOrder);
  void setDataMode(uint8_t dataMode);
  void setFrequency(uint32_t freq);
  void setClockDivider(uint32_t clockDiv);

  uint32_t getClockDivider();

  void beginTransaction(SPISettings settings);
  void endTransaction(void);
  void transfer(void *data, uint32_t size);
  uint8_t transfer(uint8_t data);
  uint16_t transfer16(uint16_t data);
  uint32_t transfer32(uint32_t data);

  void transferBytes(const uint8_t *data, uint8_t *out, uint32_t size);
  void transferBits(uint32_t data, uint32_t *out, uint8_t bits);

  void write(uint8_t data);
  void write16(uint16_t data);
  void write32(uint32_t data);
  void writeBytes(const uint8_t *data, uint32_t size);
  void writePixels(const void *data, uint32_t size);  //ili9341 compatible
  void writePattern(const uint8_t *data, uint8_t size, uint32_t repeat);

  spi_t *bus() {
    return _spi;
  }
  int8_t pinSS() {
    return _ss;
  }
};

extern SPIClass SPI;

#endif /* SOC_GPSPI_SUPPORTED */
#endif /* _SPI_H_INCLUDED */
