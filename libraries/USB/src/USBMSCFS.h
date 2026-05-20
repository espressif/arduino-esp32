// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
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
 * @brief FAT filesystem on a USB Mass Storage device (host mode).
 *
 * Same usage pattern as `SD` / `SD_MMC`: call `USBHost.begin()`, run
 * `USBHost.task()` in `loop()`, wait until `USBHostMSC.mounted()`, then
 * `USBMSCFS.begin("/usb")` and use `open()`, `exists()`, etc.
 */
class USBMSCFS : public FS {
protected:
  uint8_t _pdrv;

public:
  USBMSCFS(FSImplPtr impl);
  ~USBMSCFS();

  /**
   * Mount FAT volume on the attached USB MSC device (LUN 0).
   * @param mountpoint VFS path (e.g. "/usb")
   * @param max_files  Max open files for FatFS (same as SD::begin)
   * @param format_if_empty If mount fails with FR_NO_FILESYSTEM, optionally mkfs (use with care)
   */
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
