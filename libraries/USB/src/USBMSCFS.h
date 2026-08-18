// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
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

#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include "sdkconfig.h"
#if CONFIG_TINYUSB_ENABLED

#include "tusb_config.h"
#if CFG_TUH_ENABLED && CFG_TUH_MSC

#include "FS.h"

namespace fs {

/**
 * FAT on USB MSC (host) — same pattern as SD / SD_MMC.
 * USBHost.begin() + USBHost.task(); wait for USBHostMSC.mounted();
 * then USBMSCFS.begin("/usb") and open("/") (paths are volume-relative).
 *
 * GPT + Microsoft Basic Data FAT is supported via a synthetic MBR for FatFs.
 */
class USBMSCFS : public FS {
protected:
  uint8_t _pdrv;  // FatFs drive, or 0xFF when not mounted

public:
  USBMSCFS(FSImplPtr impl);
  ~USBMSCFS();

  bool begin(const char *mountpoint = "/usb", uint8_t max_files = 5, bool format_if_empty = false);
  void end();

  uint64_t cardSize();
  size_t numSectors();
  size_t sectorSize();
  uint64_t totalBytes();
  uint64_t usedBytes();

  bool readRAW(uint8_t *buffer, uint32_t sector);
  bool writeRAW(uint8_t *buffer, uint32_t sector);
};

}  // namespace fs

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_USB_MSC_FS)
extern fs::USBMSCFS USBMSCFS;
#endif

#endif /* CFG_TUH_ENABLED && CFG_TUH_MSC */
#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
