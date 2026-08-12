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
  /**
   * @brief Construct an empty (unmounted) MSC card handle.
   *
   * Non-default so the out-of-line definition can live in the .cpp
   * (avoids a C++20 constexpr default-ctor clash with the global instance).
   */
  USBHostMSCCard();

  /**
   * @brief True after TinyUSB has finished MSC enumeration for a device.
   * @return false if no MSC device is attached or it was unmounted.
   */
  bool mounted() const {
    return _mounted;
  }

  /**
   * @brief TinyUSB device address of the mounted MSC device.
   * @return Address in use, or 0 if not mounted.
   */
  uint8_t devAddr() const {
    return _dev_addr;
  }

  /**
   * @brief Logical unit number used for SCSI commands.
   * @return LUN index (currently always 0).
   */
  uint8_t lun() const {
    return _lun;
  }

  /**
   * @brief Number of addressable blocks reported by READ CAPACITY(10).
   * @return Block count, or 0 if not mounted.
   */
  uint32_t blockCount() const {
    return _block_count;
  }

  /**
   * @brief Size of one logical block in bytes (typically 512).
   * @return Block size, or 0 if not mounted.
   */
  uint32_t blockSize() const {
    return _block_size;
  }

  /**
   * @brief Synchronous SCSI READ(10).
   *
   * Blocks the calling task until the transfer finishes (pumps `tuh_task`
   * when the TinyUSB background task is not running). Data are copied from
   * an internal DMA-capable buffer into @p buffer when needed.
   *
   * @param lba    First logical block address.
   * @param buffer Destination buffer; must hold `blocks * blockSize()` bytes.
   * @param blocks Number of blocks to read.
   * @return true on success.
   */
  bool readBlocks(uint32_t lba, void *buffer, uint32_t blocks);

  /**
   * @brief Synchronous SCSI WRITE(10).
   *
   * Same blocking / DMA rules as `readBlocks()`.
   *
   * @param lba    First logical block address.
   * @param buffer Source buffer; must hold `blocks * blockSize()` bytes.
   * @param blocks Number of blocks to write.
   * @return true on success.
   */
  bool writeBlocks(uint32_t lba, const void *buffer, uint32_t blocks);

  /**
   * @brief FatFs `CTRL_SYNC` hook.
   *
   * No-op on purpose: issuing SCSI SYNCHRONIZE CACHE wedges some USB sticks.
   *
   * @return true if a device is currently mounted.
   */
  bool sync(void);

  /**
   * @brief TinyUSB mount callback — do not call from sketches.
   * @param dev_addr Address of the newly mounted MSC device.
   */
  void onMscMount(uint8_t dev_addr);

  /**
   * @brief TinyUSB unmount callback — do not call from sketches.
   * @param dev_addr Address of the device that was removed.
   */
  void onMscUnmount(uint8_t dev_addr);

private:
  /**
   * @brief Cache MSC interface number and bulk IN/OUT endpoints from the config descriptor.
   * @param dev_addr Device to inspect.
   * @return true if both bulk endpoints were found.
   */
  bool cacheEndpoints(uint8_t dev_addr);

  /**
   * @brief Recover a wedged Bulk-Only Transport pipe (abort, MSC reset, clear halt).
   * @param reason Short tag included in log messages (e.g. "timeout").
   * @return true if endpoints look ready again after recovery.
   */
  bool recoverBot(const char *reason);

  /**
   * @brief Shared implementation for `readBlocks` / `writeBlocks`.
   * @param is_write true for WRITE(10), false for READ(10).
   * @param lba      First logical block address.
   * @param buffer   User buffer (source or destination).
   * @param blocks   Number of blocks.
   * @return true on success.
   */
  bool xferBlocks(bool is_write, uint32_t lba, void *buffer, uint32_t blocks);

  volatile bool _mounted;   ///< Set while an MSC device is mounted
  uint8_t _dev_addr;        ///< TinyUSB device address
  uint8_t _lun;             ///< Active LUN (0)
  uint8_t _itf_num;         ///< MSC interface number (for Bulk-Only Reset)
  uint8_t _ep_in;           ///< Bulk IN endpoint address
  uint8_t _ep_out;          ///< Bulk OUT endpoint address
  uint32_t _block_count;    ///< Capacity in blocks
  uint32_t _block_size;     ///< Bytes per block
};

/** Global MSC card instance used by sketches and `USBMSCFS`. */
extern USBHostMSCCard USBHostMSC;

#endif /* CFG_TUH_ENABLED && CFG_TUH_MSC */
#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
