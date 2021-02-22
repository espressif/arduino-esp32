// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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

#ifndef ESP_H
#define ESP_H

#include <Arduino.h>
#include <esp_partition.h>

/**
 * AVR macros for WDT managment
 */
typedef enum {
    WDTO_0MS    = 0,   //!< WDTO_0MS
    WDTO_15MS   = 15,  //!< WDTO_15MS
    WDTO_30MS   = 30,  //!< WDTO_30MS
    WDTO_60MS   = 60,  //!< WDTO_60MS
    WDTO_120MS  = 120, //!< WDTO_120MS
    WDTO_250MS  = 250, //!< WDTO_250MS
    WDTO_500MS  = 500, //!< WDTO_500MS
    WDTO_1S     = 1000,//!< WDTO_1S
    WDTO_2S     = 2000,//!< WDTO_2S
    WDTO_4S     = 4000,//!< WDTO_4S
    WDTO_8S     = 8000 //!< WDTO_8S
} WDTO_t;


typedef enum {
    FM_QIO = 0x00,
    FM_QOUT = 0x01,
    FM_DIO = 0x02,
    FM_DOUT = 0x03,
    FM_FAST_READ = 0x04,
    FM_SLOW_READ = 0x05,
    FM_UNKNOWN = 0xff
} FlashMode_t;

typedef enum {
    SKETCH_SIZE_TOTAL = 0,
    SKETCH_SIZE_FREE = 1
} sketchSize_t;

class EspClass
{
public:
    EspClass() {}
    ~EspClass() {}
    void restart();

    //Internal RAM
    uint32_t getHeapSize(); //total heap size
    uint32_t getFreeHeap(); //available heap
    uint32_t getMinFreeHeap(); //lowest level of free heap since boot
    uint32_t getMaxAllocHeap(); //largest block of heap that can be allocated at once

    //SPI RAM
    uint32_t getPsramSize();
    uint32_t getFreePsram();
    uint32_t getMinFreePsram();
    uint32_t getMaxAllocPsram();

    uint8_t getChipRevision();
    const char * getChipModel();
    uint8_t getChipCores();
    uint32_t getCpuFreqMHz(){ return getCpuFrequencyMhz(); }
    inline uint32_t getCycleCount() __attribute__((always_inline));
    const char * getSdkVersion();

    void deepSleep(uint32_t time_us);

    uint32_t getFlashChipSize();
    uint32_t getFlashChipSpeed();
    FlashMode_t getFlashChipMode();

    uint32_t magicFlashChipSize(uint8_t byte);
    uint32_t magicFlashChipSpeed(uint8_t byte);
    FlashMode_t magicFlashChipMode(uint8_t byte);

    uint32_t getSketchSize();
    String getSketchMD5();
    uint32_t getFreeSketchSpace();

    bool flashEraseSector(uint32_t sector);
    bool flashWrite(uint32_t offset, uint32_t *data, size_t size);
    bool flashRead(uint32_t offset, uint32_t *data, size_t size);

    bool partitionEraseRange(const esp_partition_t *partition, uint32_t offset, size_t size);
    bool partitionWrite(const esp_partition_t *partition, uint32_t offset, uint32_t *data, size_t size);
    bool partitionRead(const esp_partition_t *partition, uint32_t offset, uint32_t *data, size_t size);

    uint64_t getEfuseMac();

};

uint32_t IRAM_ATTR EspClass::getCycleCount()
{
    uint32_t ccount;
    __asm__ __volatile__("esync; rsr %0,ccount":"=a" (ccount));
    return ccount;
}

extern EspClass ESP;

#endif //ESP_H
