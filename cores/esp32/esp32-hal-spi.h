// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MAIN_ESP32_HAL_SPI_H_
#define MAIN_ESP32_HAL_SPI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define SPI_HAS_TRANSACTION

#define FSPI  1 //SPI bus attached to the flash (can use the same data lines but different SS)
#define HSPI  2 //SPI bus normally mapped to pins 12 - 15, but can be matrixed to any pins
#define VSPI  3 //SPI bus normally attached to pins 5, 18, 19 and 23, but can be matrixed to any pins

// This defines are not representing the real Divider of the ESP32
// the Defines match to an AVR Arduino on 16MHz for better compatibility
#define SPI_CLOCK_DIV2    0x00101001 //8 MHz
#define SPI_CLOCK_DIV4    0x00241001 //4 MHz
#define SPI_CLOCK_DIV8    0x004c1001 //2 MHz
#define SPI_CLOCK_DIV16   0x009c1001 //1 MHz
#define SPI_CLOCK_DIV32   0x013c1001 //500 KHz
#define SPI_CLOCK_DIV64   0x027c1001 //250 KHz
#define SPI_CLOCK_DIV128  0x04fc1001 //125 KHz

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

#define SPI_CS0 0
#define SPI_CS1 1
#define SPI_CS2 2
#define SPI_CS_MASK_ALL 0x7

#define SPI_LSBFIRST 0
#define SPI_MSBFIRST 1

struct spi_struct_t;
typedef struct spi_struct_t spi_t;

spi_t * spiStartBus(uint8_t spi_num, uint32_t clockDiv, uint8_t dataMode, uint8_t bitOrder);
void spiStopBus(spi_t * spi);

//Attach/Detach Signal Pins
void spiAttachSCK(spi_t * spi, int8_t sck);
void spiAttachMISO(spi_t * spi, int8_t miso);
void spiAttachMOSI(spi_t * spi, int8_t mosi);
void spiDetachSCK(spi_t * spi, int8_t sck);
void spiDetachMISO(spi_t * spi, int8_t miso);
void spiDetachMOSI(spi_t * spi, int8_t mosi);

//Attach/Detach SS pin to SPI_CSx signal
void spiAttachSS(spi_t * spi, uint8_t cs_num, int8_t ss);
void spiDetachSS(spi_t * spi, int8_t ss);

//Enable/Disable SPI_CSx pins
void spiEnableSSPins(spi_t * spi, uint8_t cs_mask);
void spiDisableSSPins(spi_t * spi, uint8_t cs_mask);

//Enable/Disable hardware control of SPI_CSx pins
void spiSSEnable(spi_t * spi);
void spiSSDisable(spi_t * spi);

//Activate enabled SPI_CSx pins
void spiSSSet(spi_t * spi);
//Deactivate enabled SPI_CSx pins
void spiSSClear(spi_t * spi);

void spiWaitReady(spi_t * spi);

uint32_t spiGetClockDiv(spi_t * spi);
uint8_t spiGetDataMode(spi_t * spi);
uint8_t spiGetBitOrder(spi_t * spi);


/*
 * Non transaction based lock methods (each locks and unlocks when called)
 * */
void spiSetClockDiv(spi_t * spi, uint32_t clockDiv);
void spiSetDataMode(spi_t * spi, uint8_t dataMode);
void spiSetBitOrder(spi_t * spi, uint8_t bitOrder);

void spiWrite(spi_t * spi, const uint32_t *data, uint8_t len);
void spiWriteByte(spi_t * spi, uint8_t data);
void spiWriteWord(spi_t * spi, uint16_t data);
void spiWriteLong(spi_t * spi, uint32_t data);

void spiTransfer(spi_t * spi, uint32_t *out, uint8_t len);
uint8_t spiTransferByte(spi_t * spi, uint8_t data);
uint16_t spiTransferWord(spi_t * spi, uint16_t data);
uint32_t spiTransferLong(spi_t * spi, uint32_t data);
void spiTransferBytes(spi_t * spi, const uint8_t * data, uint8_t * out, uint32_t size);
void spiTransferBits(spi_t * spi, uint32_t data, uint32_t * out, uint8_t bits);

/*
 * New (EXPERIMENTAL) Transaction lock based API (lock once until endTransaction)
 * */
void spiTransaction(spi_t * spi, uint32_t clockDiv, uint8_t dataMode, uint8_t bitOrder);
void spiSimpleTransaction(spi_t * spi);
void spiEndTransaction(spi_t * spi);

void spiWriteNL(spi_t * spi, const void * data_in, uint32_t len);
void spiWriteByteNL(spi_t * spi, uint8_t data);
void spiWriteShortNL(spi_t * spi, uint16_t data);
void spiWriteLongNL(spi_t * spi, uint32_t data);
void spiWritePixelsNL(spi_t * spi, const void * data_in, uint32_t len);

#define spiTransferNL(spi, data, len) spiTransferBytesNL(spi, data, data, len)
uint8_t spiTransferByteNL(spi_t * spi, uint8_t data);
uint16_t spiTransferShortNL(spi_t * spi, uint16_t data);
uint32_t spiTransferLongNL(spi_t * spi, uint32_t data);
void spiTransferBytesNL(spi_t * spi, const void * data_in, uint8_t * data_out, uint32_t len);
void spiTransferBitsNL(spi_t * spi, uint32_t data_in, uint32_t * data_out, uint8_t bits);

/*
 * Helper functions to translate frequency to clock divider and back
 * */
uint32_t spiFrequencyToClockDiv(uint32_t freq);
uint32_t spiClockDivToFrequency(uint32_t freq);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_ESP32_HAL_SPI_H_ */
