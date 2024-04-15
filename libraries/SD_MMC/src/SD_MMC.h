// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#ifndef _SDMMC_H_
#define _SDMMC_H_

#include "sdkconfig.h"
#include "soc/soc_caps.h"
#ifdef SOC_SDMMC_HOST_SUPPORTED

#include "FS.h"
#include "driver/sdmmc_types.h"
#include "sd_defines.h"

// If reading/writing to the SD card is unstable,
// you can define BOARD_MAX_SDMMC_FREQ with lower value (Ex. SDMMC_FREQ_DEFAULT)
// in pins_arduino.h for your board variant.
#ifndef BOARD_MAX_SDMMC_FREQ
#define BOARD_MAX_SDMMC_FREQ SDMMC_FREQ_HIGHSPEED
#endif

namespace fs {

class SDMMCFS : public FS {
protected:
  sdmmc_card_t* _card;
  int8_t _pin_clk = -1;
  int8_t _pin_cmd = -1;
  int8_t _pin_d0 = -1;
  int8_t _pin_d1 = -1;
  int8_t _pin_d2 = -1;
  int8_t _pin_d3 = -1;
  bool _mode1bit = false;

public:
  SDMMCFS(FSImplPtr impl);
  bool setPins(int clk, int cmd, int d0);
  bool setPins(int clk, int cmd, int d0, int d1, int d2, int d3);
  bool begin(const char* mountpoint = "/sdcard", bool mode1bit = false, bool format_if_mount_failed = false, int sdmmc_frequency = BOARD_MAX_SDMMC_FREQ, uint8_t maxOpenFiles = 5);
  void end();
  sdcard_type_t cardType();
  uint64_t cardSize();
  uint64_t totalBytes();
  uint64_t usedBytes();

private:
  static bool sdmmcDetachBus(void* bus_pointer);
};

}

extern fs::SDMMCFS SD_MMC;

#endif /* SOC_SDMMC_HOST_SUPPORTED */
#endif /* _SDMMC_H_ */
