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

#include <stdint.h>
#include <stdbool.h>

/**
 * USB Host MSC block device (TinyUSB host).
 * Pair with USBMSCFS for SD-like FatFS access.
 */
class USBHostMSCClass {
public:
  USBHostMSCClass();

  bool mounted() const {
    return _mounted;
  }
  uint8_t devAddr() const {
    return _dev_addr;
  }
  uint8_t lun() const {
    return _lun;
  }
  uint32_t blockCount() const {
    return _block_count;
  }
  uint32_t blockSize() const {
    return _block_size;
  }

  /** Synchronous SCSI READ(10) / WRITE(10). Blocks until done. */
  bool readBlocks(uint32_t lba, void *buffer, uint32_t blocks);
  bool writeBlocks(uint32_t lba, const void *buffer, uint32_t blocks);

  /** FatFs CTRL_SYNC — intentionally a no-op (SYNCHRONIZE CACHE wedges some sticks). */
  bool sync(void);

  /** @internal TinyUSB mount / umount. */
  void onMscMount(uint8_t dev_addr);
  void onMscUnmount(uint8_t dev_addr);

private:
  bool cacheEndpoints(uint8_t dev_addr);
  bool recoverBot(const char *reason);
  bool xferBlocks(bool is_write, uint32_t lba, void *buffer, uint32_t blocks);

  volatile bool _mounted;
  uint8_t _dev_addr;
  uint8_t _lun;
  uint8_t _itf_num;
  uint8_t _ep_in;
  uint8_t _ep_out;
  uint32_t _block_count;
  uint32_t _block_size;
};

extern USBHostMSCClass USBHostMSC;

#include "USBMSCFS.h"

#endif /* CFG_TUH_ENABLED && CFG_TUH_MSC */
#endif /* CONFIG_TINYUSB_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
