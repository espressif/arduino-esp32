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

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief USB Host MSC (Mass Storage) — block device API for TinyUSB host.
 *
 * Populated when `tuh_msc_mount_cb` runs; cleared on `tuh_msc_umount_cb`.
 * Call `USBHost.task()` (or `tuh_task()`) from loop so transfers complete.
 *
 * Use with `USBMSCFS` for the same FatFS/VFS pattern as `SD` / `SD_MMC`.
 */
class USBHostMSCCard {
public:
  /** User-provided (non-implicit) so the definition can live in .cpp (C++20 constexpr default ctor clash). */
  USBHostMSCCard();

  bool mounted() const {
    return _mounted;
  }

  uint8_t devAddr() const {
    return _dev_addr;
  }

  /** Logical unit (default 0). */
  uint8_t lun() const {
    return _lun;
  }

  uint32_t blockCount() const {
    return _block_count;
  }

  uint32_t blockSize() const {
    return _block_size;
  }

  /**
   * Synchronous READ(10). Blocks calling task until done (runs tuh_task internally).
   * @param lba First block index
   * @param buffer Destination (copied from internal DMA buffer if needed)
   * @param blocks Number of blocks (each blockSize() bytes)
   */
  bool readBlocks(uint32_t lba, void *buffer, uint32_t blocks);

  /**
   * Synchronous WRITE(10). Blocks until done.
   */
  bool writeBlocks(uint32_t lba, const void *buffer, uint32_t blocks);

  /** @internal TinyUSB mount callback (do not call from sketches). */
  void onMscMount(uint8_t dev_addr);
  /** @internal TinyUSB umount callback. */
  void onMscUnmount(uint8_t dev_addr);

private:
  volatile bool _mounted;
  uint8_t _dev_addr;
  uint8_t _lun;
  uint32_t _block_count;
  uint32_t _block_size;
};

extern USBHostMSCCard USBHostMSC;

#endif /* CFG_TUH_ENABLED && CFG_TUH_MSC */
#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
