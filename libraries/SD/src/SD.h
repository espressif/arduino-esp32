// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _SD_H_
#define _SD_H_

#include "FS.h"
#include "SPI.h"
#include "sd_defines.h"

namespace fs
{

class SDFS : public FS
{
protected:
    uint8_t _pdrv;

public:
    SDFS(FSImplPtr impl);
    bool begin(uint8_t ssPin=SS, SPIClass &spi=SPI, uint32_t frequency=4000000, const char * mountpoint="/sd", uint8_t max_files=5);
    void end();
    sdcard_type_t cardType();
    uint64_t cardSize();
    uint64_t totalBytes();
    uint64_t usedBytes();
};

}

extern fs::SDFS SD;

using namespace fs;
typedef fs::File        SDFile;
typedef fs::SDFS        SDFileSystemClass;
#define SDFileSystem    SD

#endif /* _SD_H_ */
