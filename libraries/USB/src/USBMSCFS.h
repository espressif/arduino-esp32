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
 *
 * GPT disks with a Microsoft Basic Data FAT partition are supported via a
 * synthetic MBR presented to FatFs (Arduino FatFs is built without GPT search).
 */
class USBMSCFS : public FS {
protected:
  uint8_t _pdrv;  ///< FatFs physical drive number, or 0xFF when not mounted

public:
  /**
   * @brief Construct a USB MSC filesystem object.
   * @param impl VFS/FS implementation pointer (normally supplied by the global instance).
   */
  USBMSCFS(FSImplPtr impl);

  /** @brief Unmount on destruction if still mounted. */
  ~USBMSCFS();

  /**
   * @brief Mount a FAT volume on the attached USB MSC device.
   *
   * Probes MBR / GPT / super-floppy layout, registers FatFs diskio, and mounts
   * the volume at @p mountpoint.
   *
   * @param mountpoint      VFS path (e.g. `"/usb"`).
   * @param max_files       Maximum simultaneously open files (same as `SD::begin`).
   * @param format_if_empty If mount fails with `FR_NO_FILESYSTEM`, optionally run `f_mkfs`
   *                        (destructive — use with care).
   * @return true on success.
   */
  bool begin(const char *mountpoint = "/usb", uint8_t max_files = 5, bool format_if_empty = false);

  /**
   * @brief Unmount the volume and release the FatFs drive slot.
   *
   * Safe to call when not mounted.
   */
  void end();

  /**
   * @brief Raw device capacity in bytes (`blockCount * blockSize`).
   * @return 0 if not mounted.
   */
  uint64_t cardSize();

  /**
   * @brief Number of sectors (logical blocks) on the MSC device.
   * @return 0 if not mounted.
   */
  size_t numSectors();

  /**
   * @brief Logical sector size in bytes (same as `USBHostMSC.blockSize()`).
   * @return 0 if not mounted.
   */
  size_t sectorSize();

  /**
   * @brief Total capacity of the mounted FAT volume in bytes.
   * @return 0 on error or if not mounted.
   */
  uint64_t totalBytes();

  /**
   * @brief Used space on the mounted FAT volume in bytes.
   * @return 0 on error or if not mounted.
   */
  uint64_t usedBytes();

  /**
   * @brief Read one sector via the FatFs diskio layer (honors synthetic MBR if used).
   * @param buffer Destination; must hold at least `sectorSize()` bytes.
   * @param sector Sector index as seen by FatFs.
   * @return true on success.
   */
  bool readRAW(uint8_t *buffer, uint32_t sector);

  /**
   * @brief Write one sector via the FatFs diskio layer (honors synthetic MBR if used).
   * @param buffer Source; must hold at least `sectorSize()` bytes.
   * @param sector Sector index as seen by FatFs.
   * @return true on success.
   */
  bool writeRAW(uint8_t *buffer, uint32_t sector);
};

}  // namespace fs

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_USB_MSC_FS)
/** Global USB MSC filesystem instance (same role as `SD`). */
extern fs::USBMSCFS USBMSCFS;
#endif

#endif /* CFG_TUH_ENABLED && CFG_TUH_MSC */
#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
