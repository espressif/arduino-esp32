/*
 SPI.cpp - SPI library for esp8266

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

#include "SPI.h"
#if SOC_GPSPI_SUPPORTED

#include "io_pin_remap.h"
#include "esp32-hal-log.h"

#if !CONFIG_DISABLE_HAL_LOCKS
#define SPI_PARAM_LOCK() \
  do {                   \
  } while (xSemaphoreTake(paramLock, portMAX_DELAY) != pdPASS)
#define SPI_PARAM_UNLOCK() xSemaphoreGive(paramLock)
#else
#define SPI_PARAM_LOCK()
#define SPI_PARAM_UNLOCK()
#endif

SPIClass::SPIClass(uint8_t spi_bus)
  : _spi_num(spi_bus), _spi(NULL), _use_hw_ss(false), _sck(-1), _miso(-1), _mosi(-1), _ss(-1), _div(0), _freq(1000000), _inTransaction(false)
#if !CONFIG_DISABLE_HAL_LOCKS
    ,
    paramLock(NULL) {
  if (paramLock == NULL) {
    paramLock = xSemaphoreCreateMutex();
    if (paramLock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return;
    }
  }
}
#else
{
}
#endif

SPIClass::~SPIClass() {
  end();
#if !CONFIG_DISABLE_HAL_LOCKS
  if (paramLock != NULL) {
    vSemaphoreDelete(paramLock);
    paramLock = NULL;
  }
#endif
}

void SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {
  if (_spi) {
    return;
  }

  if (!_div) {
    _div = spiFrequencyToClockDiv(_freq);
  }

  _spi = spiStartBus(_spi_num, _div, SPI_MODE0, SPI_MSBFIRST);
  if (!_spi) {
    return;
  }

  if (sck == -1 && miso == -1 && mosi == -1 && ss == -1) {
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    _sck = (_spi_num == FSPI) ? SCK : -1;
    _miso = (_spi_num == FSPI) ? MISO : -1;
    _mosi = (_spi_num == FSPI) ? MOSI : -1;
    _ss = (_spi_num == FSPI) ? SS : -1;
#elif CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32P4
    _sck = SCK;
    _miso = MISO;
    _mosi = MOSI;
    _ss = SS;
#else
    _sck = (_spi_num == VSPI) ? SCK : 14;
    _miso = (_spi_num == VSPI) ? MISO : 12;
    _mosi = (_spi_num == VSPI) ? MOSI : 13;
    _ss = (_spi_num == VSPI) ? SS : 15;
#endif
  } else {
    _sck = sck;
    _miso = miso;
    _mosi = mosi;
    _ss = ss;
  }

  if (!spiAttachSCK(_spi, _sck)) {
    goto err;
  }
  if (_miso >= 0 && !spiAttachMISO(_spi, _miso)) {
    goto err;
  }
  if (_mosi >= 0 && !spiAttachMOSI(_spi, _mosi)) {
    goto err;
  }
  return;

err:
  log_e("Attaching pins to SPI failed.");
}

void SPIClass::end() {
  if (!_spi) {
    return;
  }
  spiDetachSCK(_spi);
  spiDetachMISO(_spi);
  spiDetachMOSI(_spi);
  setHwCs(false);
  if (spiGetClockDiv(_spi) != 0) {
    spiStopBus(_spi);
  }
  _spi = NULL;
}

void SPIClass::setHwCs(bool use) {
  if (_ss < 0) {
    return;
  }
  if (use && !_use_hw_ss) {
    spiAttachSS(_spi, 0, _ss);
    spiSSEnable(_spi);
  } else if (!use && _use_hw_ss) {
    spiSSDisable(_spi);
    spiDetachSS(_spi);
  }
  _use_hw_ss = use;
}

void SPIClass::setSSInvert(bool invert) {
  if (_spi) {
    spiSSInvert(_spi, invert);
  }
}

void SPIClass::setFrequency(uint32_t freq) {
  SPI_PARAM_LOCK();
  //check if last freq changed
  uint32_t cdiv = spiGetClockDiv(_spi);
  if (_freq != freq || _div != cdiv) {
    _freq = freq;
    _div = spiFrequencyToClockDiv(_freq);
    spiSetClockDiv(_spi, _div);
  }
  SPI_PARAM_UNLOCK();
}

void SPIClass::setClockDivider(uint32_t clockDiv) {
  SPI_PARAM_LOCK();
  _div = clockDiv;
  spiSetClockDiv(_spi, _div);
  SPI_PARAM_UNLOCK();
}

uint32_t SPIClass::getClockDivider() {
  return spiGetClockDiv(_spi);
}

void SPIClass::setDataMode(uint8_t dataMode) {
  spiSetDataMode(_spi, dataMode);
}

void SPIClass::setBitOrder(uint8_t bitOrder) {
  spiSetBitOrder(_spi, bitOrder);
}

void SPIClass::beginTransaction(SPISettings settings) {
  SPI_PARAM_LOCK();
  //check if last freq changed
  uint32_t cdiv = spiGetClockDiv(_spi);
  if (_freq != settings._clock || _div != cdiv) {
    _freq = settings._clock;
    _div = spiFrequencyToClockDiv(_freq);
  }
  spiTransaction(_spi, _div, settings._dataMode, settings._bitOrder);
  _inTransaction = true;
}

void SPIClass::endTransaction() {
  if (_inTransaction) {
    _inTransaction = false;
    spiEndTransaction(_spi);
    SPI_PARAM_UNLOCK();  // <-- Im not sure should it be here or right after spiTransaction()
  }
}

void SPIClass::write(uint8_t data) {
  if (_inTransaction) {
    return spiWriteByteNL(_spi, data);
  }
  spiWriteByte(_spi, data);
}

uint8_t SPIClass::transfer(uint8_t data) {
  if (_inTransaction) {
    return spiTransferByteNL(_spi, data);
  }
  return spiTransferByte(_spi, data);
}

void SPIClass::write16(uint16_t data) {
  if (_inTransaction) {
    return spiWriteShortNL(_spi, data);
  }
  spiWriteWord(_spi, data);
}

uint16_t SPIClass::transfer16(uint16_t data) {
  if (_inTransaction) {
    return spiTransferShortNL(_spi, data);
  }
  return spiTransferWord(_spi, data);
}

void SPIClass::write32(uint32_t data) {
  if (_inTransaction) {
    return spiWriteLongNL(_spi, data);
  }
  spiWriteLong(_spi, data);
}

uint32_t SPIClass::transfer32(uint32_t data) {
  if (_inTransaction) {
    return spiTransferLongNL(_spi, data);
  }
  return spiTransferLong(_spi, data);
}

void SPIClass::transferBits(uint32_t data, uint32_t *out, uint8_t bits) {
  if (_inTransaction) {
    return spiTransferBitsNL(_spi, data, out, bits);
  }
  spiTransferBits(_spi, data, out, bits);
}

/**
 * @param data uint8_t *
 * @param size uint32_t
 */
void SPIClass::writeBytes(const uint8_t *data, uint32_t size) {
  if (_inTransaction) {
    return spiWriteNL(_spi, data, size);
  }
  spiSimpleTransaction(_spi);
  spiWriteNL(_spi, data, size);
  spiEndTransaction(_spi);
}

void SPIClass::transfer(void *data, uint32_t size) {
  transferBytes((const uint8_t *)data, (uint8_t *)data, size);
}

/**
 * @param data void *
 * @param size uint32_t
 */
void SPIClass::writePixels(const void *data, uint32_t size) {
  if (_inTransaction) {
    return spiWritePixelsNL(_spi, data, size);
  }
  spiSimpleTransaction(_spi);
  spiWritePixelsNL(_spi, data, size);
  spiEndTransaction(_spi);
}

/**
 * @param data uint8_t * data buffer. can be NULL for Read Only operation
 * @param out  uint8_t * output buffer. can be NULL for Write Only operation
 * @param size uint32_t
 */
void SPIClass::transferBytes(const uint8_t *data, uint8_t *out, uint32_t size) {
  if (_inTransaction) {
    return spiTransferBytesNL(_spi, data, out, size);
  }
  spiTransferBytes(_spi, data, out, size);
}

/**
 * @param data uint8_t *
 * @param size uint8_t  max for size is 64Byte
 * @param repeat uint32_t
 */
void SPIClass::writePattern(const uint8_t *data, uint8_t size, uint32_t repeat) {
  if (size > 64) {
    return;  //max Hardware FIFO
  }

  uint32_t byte = (size * repeat);
  uint8_t r = (64 / size);
  const uint8_t max_bytes_FIFO = r * size;  // Max number of whole patterns (in bytes) that can fit into the hardware FIFO

  while (byte) {
    if (byte > max_bytes_FIFO) {
      writePattern_(data, size, r);
      byte -= max_bytes_FIFO;
    } else {
      writePattern_(data, size, (byte / size));
      byte = 0;
    }
  }
}

void SPIClass::writePattern_(const uint8_t *data, uint8_t size, uint8_t repeat) {
  uint8_t bytes = (size * repeat);
  uint8_t buffer[64];
  uint8_t *bufferPtr = &buffer[0];
  const uint8_t *dataPtr;
  uint8_t dataSize = bytes;
  for (uint8_t i = 0; i < repeat; i++) {
    dataSize = size;
    dataPtr = data;
    while (dataSize--) {
      *bufferPtr = *dataPtr;
      dataPtr++;
      bufferPtr++;
    }
  }

  writeBytes(&buffer[0], bytes);
}

#if CONFIG_IDF_TARGET_ESP32
SPIClass SPI(VSPI);
#else
SPIClass SPI(FSPI);
#endif

#endif /* SOC_GPSPI_SUPPORTED */
