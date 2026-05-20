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

#define ARDUINO_CORE_BUILD

#include "USBMSCFS.h"

#if SOC_USB_OTG_SUPPORTED && CONFIG_TINYUSB_ENABLED && CFG_TUH_ENABLED && CFG_TUH_MSC

#include "USBHostMSC.h"
#include "vfs_api.h"
#include "ff.h"
#include "esp_vfs_fat.h"
#include "esp_err.h"
#include "Arduino.h"
#include "esp32-hal-log.h"

extern "C" {
#include "diskio.h"
#include "diskio_impl.h"
}

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *base_path;
} usb_msc_slot_t;

static usb_msc_slot_t *s_usbmsc_slots[FF_VOLUMES] = {NULL};

static DSTATUS ff_usbmsc_initialize(unsigned char pdrv) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr) {
    return STA_NOINIT;
  }
  if (!USBHostMSC.mounted()) {
    return STA_NOINIT;
  }
  return 0;
}

static DSTATUS ff_usbmsc_status(unsigned char pdrv) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr) {
    return STA_NOINIT;
  }
  if (!USBHostMSC.mounted()) {
    return STA_NOINIT;
  }
  return 0;
}

static DRESULT ff_usbmsc_read(unsigned char pdrv, unsigned char *buff, uint32_t sector, unsigned count) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr) {
    return RES_PARERR;
  }
  if (!USBHostMSC.mounted()) {
    return RES_NOTRDY;
  }
  log_v("[USBMSCFS] disk_read pdrv=%u sector=%" PRIu32 " count=%u",
        (unsigned)pdrv, (uint32_t)sector, (unsigned)count);
  const bool ok = USBHostMSC.readBlocks(sector, buff, (uint32_t)count);
  log_v("[USBMSCFS] disk_read -> %s", ok ? "RES_OK" : "RES_ERROR");
  return ok ? RES_OK : RES_ERROR;
}

static DRESULT ff_usbmsc_write(unsigned char pdrv, const unsigned char *buff, uint32_t sector, unsigned count) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr) {
    return RES_PARERR;
  }
  if (!USBHostMSC.mounted()) {
    return RES_NOTRDY;
  }
  log_v("[USBMSCFS] disk_write pdrv=%u sector=%" PRIu32 " count=%u",
        (unsigned)pdrv, (uint32_t)sector, (unsigned)count);
  const bool ok = USBHostMSC.writeBlocks(sector, buff, (uint32_t)count);
  log_v("[USBMSCFS] disk_write -> %s", ok ? "RES_OK" : "RES_ERROR");
  return ok ? RES_OK : RES_ERROR;
}

static DRESULT ff_usbmsc_ioctl(unsigned char pdrv, unsigned char cmd, void *buff) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr) {
    return RES_PARERR;
  }
  switch (cmd) {
    case CTRL_SYNC:
      log_v("[USBMSCFS] disk_ioctl pdrv=%u CTRL_SYNC", (unsigned)pdrv);
      return RES_OK;
    case GET_SECTOR_COUNT:
      if (!USBHostMSC.mounted() || buff == nullptr) {
        return RES_ERROR;
      }
      *((unsigned long *)buff) = (unsigned long)USBHostMSC.blockCount();
      return RES_OK;
    case GET_SECTOR_SIZE:
      if (!USBHostMSC.mounted() || buff == nullptr) {
        return RES_ERROR;
      }
      *((WORD *)buff) = (WORD)USBHostMSC.blockSize();
      return RES_OK;
    case GET_BLOCK_SIZE:
      if (buff == nullptr) {
        return RES_ERROR;
      }
      *((uint32_t *)buff) = 1;
      return RES_OK;
    default:
      return RES_PARERR;
  }
}

static void usbmsc_free_slot(uint8_t pdrv) {
  if (pdrv >= FF_VOLUMES) {
    return;
  }
  usb_msc_slot_t *slot = s_usbmsc_slots[pdrv];
  if (slot != nullptr) {
    if (slot->base_path) {
      free(slot->base_path);
    }
    free(slot);
    s_usbmsc_slots[pdrv] = nullptr;
  }
}

namespace fs {

USBMSCFS::USBMSCFS(FSImplPtr impl) : FS(impl), _pdrv(0xFF) {}

USBMSCFS::~USBMSCFS() {
  end();
}

bool USBMSCFS::begin(const char *mountpoint, uint8_t max_files, bool format_if_empty) {
  if (_pdrv != 0xFF) {
    return true;
  }
  if (mountpoint == nullptr || !USBHostMSC.mounted()) {
    return false;
  }

  uint8_t pdrv = 0xFF;
  if (ff_diskio_get_drive(&pdrv) != ESP_OK || pdrv == 0xFF) {
    log_e("USBMSCFS: no free FatFS drive slot (ff_diskio_get_drive)");
    return false;
  }

  usb_msc_slot_t *slot = (usb_msc_slot_t *)calloc(1, sizeof(usb_msc_slot_t));
  if (slot == nullptr) {
    return false;
  }
  slot->base_path = strdup(mountpoint);
  if (slot->base_path == nullptr) {
    free(slot);
    return false;
  }

  static const ff_diskio_impl_t usb_msc_diskio = {
    ff_usbmsc_initialize,
    ff_usbmsc_status,
    ff_usbmsc_read,
    ff_usbmsc_write,
    ff_usbmsc_ioctl,
  };

  ff_diskio_register(pdrv, &usb_msc_diskio);
  s_usbmsc_slots[pdrv] = slot;
  _pdrv = pdrv;

  FATFS *fs = nullptr;
  char drv[3] = {(char)('0' + pdrv), ':', 0};
  esp_err_t err = esp_vfs_fat_register(mountpoint, drv, max_files, &fs);
  if (err != ESP_OK) {
    log_e("USBMSCFS: esp_vfs_fat_register failed 0x%x", err);
    ff_diskio_register(pdrv, NULL);
    usbmsc_free_slot(pdrv);
    _pdrv = 0xFF;
    return false;
  }

  FRESULT res = f_mount(fs, drv, 1);
  if (res != FR_OK) {
    log_e("USBMSCFS: f_mount failed (%d)", (int)res);
    if (res == FR_NO_FILESYSTEM && format_if_empty) { /* (13) no valid FAT */
      BYTE *work = (BYTE *)malloc(sizeof(BYTE) * FF_MAX_SS);
      if (work == nullptr) {
        log_e("USBMSCFS: alloc for f_mkfs failed");
        esp_vfs_fat_unregister_path(mountpoint);
        ff_diskio_register(pdrv, NULL);
        usbmsc_free_slot(pdrv);
        _pdrv = 0xFF;
        return false;
      }
      const MKFS_PARM opt = {(BYTE)FM_ANY, 0, 0, 0, 0};
      res = f_mkfs(drv, &opt, work, sizeof(BYTE) * FF_MAX_SS);
      free(work);
      if (res != FR_OK) {
        log_e("USBMSCFS: f_mkfs failed (%d)", (int)res);
        esp_vfs_fat_unregister_path(mountpoint);
        ff_diskio_register(pdrv, NULL);
        usbmsc_free_slot(pdrv);
        _pdrv = 0xFF;
        return false;
      }
      res = f_mount(fs, drv, 1);
      if (res != FR_OK) {
        log_e("USBMSCFS: f_mount after mkfs failed (%d)", (int)res);
        esp_vfs_fat_unregister_path(mountpoint);
        ff_diskio_register(pdrv, NULL);
        usbmsc_free_slot(pdrv);
        _pdrv = 0xFF;
        return false;
      }
    } else {
      esp_vfs_fat_unregister_path(mountpoint);
      ff_diskio_register(pdrv, NULL);
      usbmsc_free_slot(pdrv);
      _pdrv = 0xFF;
      return false;
    }
  }

  _impl->mountpoint(mountpoint);
  return true;
}

void USBMSCFS::end() {
  if (_pdrv == 0xFF) {
    return;
  }
  uint8_t pdrv = _pdrv;
  _impl->mountpoint(nullptr);

  char drv[3] = {(char)('0' + pdrv), ':', 0};
  (void)f_mount(NULL, drv, 0);

  if (s_usbmsc_slots[pdrv] != nullptr && s_usbmsc_slots[pdrv]->base_path != nullptr) {
    esp_vfs_fat_unregister_path(s_usbmsc_slots[pdrv]->base_path);
  }

  ff_diskio_register(pdrv, NULL);
  usbmsc_free_slot(pdrv);
  _pdrv = 0xFF;
}

uint64_t USBMSCFS::cardSize() {
  if (_pdrv == 0xFF || !USBHostMSC.mounted()) {
    return 0;
  }
  return (uint64_t)USBHostMSC.blockCount() * (uint64_t)USBHostMSC.blockSize();
}

size_t USBMSCFS::numSectors() {
  if (_pdrv == 0xFF || !USBHostMSC.mounted()) {
    return 0;
  }
  return (size_t)USBHostMSC.blockCount();
}

size_t USBMSCFS::sectorSize() {
  if (_pdrv == 0xFF || !USBHostMSC.mounted()) {
    return 0;
  }
  return (size_t)USBHostMSC.blockSize();
}

uint64_t USBMSCFS::totalBytes() {
  FATFS *fsinfo;
  DWORD fre_clust;
  char drv[3] = {(char)('0' + _pdrv), ':', 0};
  if (_pdrv == 0xFF || f_getfree(drv, &fre_clust, &fsinfo) != 0) {
    return 0;
  }
  uint64_t size = ((uint64_t)(fsinfo->csize)) * (fsinfo->n_fatent - 2)
#if _MAX_SS != 512
                  * (fsinfo->ssize);
#else
                  * 512;
#endif
  return size;
}

uint64_t USBMSCFS::usedBytes() {
  FATFS *fsinfo;
  DWORD fre_clust;
  char drv[3] = {(char)('0' + _pdrv), ':', 0};
  if (_pdrv == 0xFF || f_getfree(drv, &fre_clust, &fsinfo) != 0) {
    return 0;
  }
  uint64_t size = ((uint64_t)(fsinfo->csize)) * ((fsinfo->n_fatent - 2) - (fsinfo->free_clst))
#if _MAX_SS != 512
                  * (fsinfo->ssize);
#else
                  * 512;
#endif
  return size;
}

bool USBMSCFS::readRAW(uint8_t *buffer, uint32_t sector) {
  if (_pdrv == 0xFF || buffer == nullptr) {
    return false;
  }
  return ff_usbmsc_read(_pdrv, buffer, sector, 1u) == RES_OK;
}

bool USBMSCFS::writeRAW(uint8_t *buffer, uint32_t sector) {
  if (_pdrv == 0xFF || buffer == nullptr) {
    return false;
  }
  return ff_usbmsc_write(_pdrv, buffer, sector, 1u) == RES_OK;
}

}  // namespace fs

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_USB_MSC_FS)
fs::USBMSCFS USBMSCFS(FSImplPtr(new VFSImpl()));
#endif

#endif /* SOC_USB_OTG && CFG_TUH_MSC */
