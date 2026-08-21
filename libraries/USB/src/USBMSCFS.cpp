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

/* FatFs in Arduino-ESP32 is built with FF_LBA64=0, so GPT is not searched by f_mount.
 * For GPT disks we expose a synthetic MBR that points at the FAT partition; FatFs then
 * uses normal MBR scanning with absolute LBAs (no super-floppy remapping). */
static const uint8_t kGuidMsBasicData[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44,
                                             0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};

typedef struct {
  char *base_path;
  bool use_synth_mbr; /**< true: disk_read(0) returns synthetic MBR for GPT→FAT */
  uint32_t gpt_part_lba; /**< Absolute start LBA of FAT partition (GPT) */
  uint32_t gpt_part_count; /**< Partition sector count for synthetic PTE */
} usb_msc_slot_t;

static usb_msc_slot_t *s_usbmsc_slots[FF_VOLUMES] = {NULL};

static uint32_t ld_u16(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t ld_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t ld_u64(const uint8_t *p) {
  return (uint64_t)ld_u32(p) | ((uint64_t)ld_u32(p + 4) << 32);
}

static void st_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

static bool sector_looks_fat_vbr(const uint8_t *sec) {
  if (ld_u16(sec + 510) != 0xAA55) {
    return false;
  }
  if (memcmp(sec + 82, "FAT32   ", 8) == 0) {
    return true;
  }
  if (memcmp(sec + 3, "EXFAT   ", 8) == 0) {
    return false; /* exFAT — not mountable unless FF_FS_EXFAT */
  }
  const uint8_t jmp = sec[0];
  if (jmp != 0xEB && jmp != 0xE9 && jmp != 0xE8) {
    return false;
  }
  /* FAT12/16 heuristic (same idea as FatFs check_fs). */
  const uint32_t bps = ld_u16(sec + 11);
  const uint8_t spc = sec[13];
  return (bps >= 512 && bps <= 4096 && (bps & (bps - 1)) == 0) && spc != 0 && (spc & (spc - 1)) == 0 &&
         ld_u16(sec + 14) != 0 && sec[16] >= 1 && sec[16] <= 2;
}

static bool sector_looks_exfat_vbr(const uint8_t *sec) {
  return ld_u16(sec + 510) == 0xAA55 && memcmp(sec + 3, "EXFAT   ", 8) == 0;
}

static void usbmsc_log_sector0_diag(const uint8_t *sec, uint32_t block_size) {
  log_e(
    "USBMSCFS: LBA0 diag bsize=%" PRIu32 " sig=0x%04" PRIx32 " jmp=0x%02x type32='%.8s' oem='%.8s' mbr0_sys=0x%02x mbr0_lba=%" PRIu32,
    block_size,
    (uint32_t)ld_u16(sec + 510),
    (unsigned)sec[0],
    (const char *)(sec + 82),
    (const char *)(sec + 3),
    (unsigned)sec[0x1BE + 4],
    (uint32_t)ld_u32(sec + 0x1BE + 8));
}

static void usbmsc_fill_synth_mbr(uint8_t *sec, uint32_t part_lba, uint32_t part_count) {
  memset(sec, 0, 512);
  /* One primary partition: FAT32 LBA (0x0C), start/count in LBA addressing. */
  sec[0x1BE + 4] = 0x0C;
  st_u32(sec + 0x1BE + 8, part_lba);
  st_u32(sec + 0x1BE + 12, part_count);
  sec[510] = 0x55;
  sec[511] = 0xAA;
}

/**
 * Probe MBR/GPT/SFD. For GPT+FAT, returns partition LBA/count for synthetic MBR.
 * @return true if a plausible FAT volume layout was found.
 */
static bool usbmsc_probe_volume(uint32_t *out_gpt_lba, uint32_t *out_gpt_count, bool *out_exfat) {
  *out_gpt_lba = 0;
  *out_gpt_count = 0;
  *out_exfat = false;

  if (!USBHostMSC.mounted()) {
    return false;
  }

  const uint32_t bsize = USBHostMSC.blockSize();
  const uint32_t bcount = USBHostMSC.blockCount();
  if (bsize < 512 || bcount < 2) {
    log_e("USBMSCFS: bad MSC geometry blocks=%" PRIu32 " bsize=%" PRIu32, bcount, bsize);
    return false;
  }

  uint8_t *sec = (uint8_t *)malloc(bsize);
  if (sec == nullptr) {
    return false;
  }

  if (!USBHostMSC.readBlocks(0, sec, 1)) {
    log_e("USBMSCFS: failed to read LBA 0");
    free(sec);
    return false;
  }

  if (sector_looks_exfat_vbr(sec)) {
    *out_exfat = true;
    usbmsc_log_sector0_diag(sec, bsize);
    free(sec);
    return false;
  }

  if (sector_looks_fat_vbr(sec)) {
    log_d("USBMSCFS: FAT volume at LBA 0 (super-floppy)");
    free(sec);
    return true;
  }

  if (sec[0x1BE + 4] == 0xEE) {
    /* Protective MBR → GPT header at LBA 1. */
    if (!USBHostMSC.readBlocks(1, sec, 1) || memcmp(sec, "EFI PART", 8) != 0) {
      log_e("USBMSCFS: protective MBR but GPT header missing/invalid");
      if (USBHostMSC.readBlocks(0, sec, 1)) {
        usbmsc_log_sector0_diag(sec, bsize);
      }
      free(sec);
      return false;
    }

    const uint64_t pt_lba = ld_u64(sec + 72);
    const uint32_t n_ent = ld_u32(sec + 80);
    uint32_t ent_sz = ld_u32(sec + 84);
    if (ent_sz < 128) {
      ent_sz = 128;
    }
    if (pt_lba == 0 || n_ent == 0 || pt_lba >= bcount) {
      log_e("USBMSCFS: invalid GPT partition table location");
      free(sec);
      return false;
    }

    const uint32_t ents_per_sec = bsize / ent_sz;
    if (ents_per_sec == 0) {
      free(sec);
      return false;
    }

    for (uint32_t i = 0; i < n_ent && i < 128; i++) {
      const uint32_t lba = (uint32_t)pt_lba + (i / ents_per_sec);
      const uint32_t ofs = (i % ents_per_sec) * ent_sz;
      if ((i % ents_per_sec) == 0) {
        if (lba >= bcount || !USBHostMSC.readBlocks(lba, sec, 1)) {
          break;
        }
      }
      if (memcmp(sec + ofs, kGuidMsBasicData, 16) != 0) {
        continue;
      }
      const uint64_t first = ld_u64(sec + ofs + 32);
      const uint64_t last = ld_u64(sec + ofs + 40);
      if (first == 0 || first >= bcount || last < first) {
        continue;
      }

      /* Probe VBR in-place, then reload the partition-table sector for further entries. */
      if (!USBHostMSC.readBlocks((uint32_t)first, sec, 1)) {
        continue;
      }
      if (sector_looks_exfat_vbr(sec)) {
        *out_exfat = true;
        log_e("USBMSCFS: GPT partition at LBA %" PRIu32 " is exFAT (not enabled in FatFs)", (uint32_t)first);
        free(sec);
        return false;
      }
      if (sector_looks_fat_vbr(sec)) {
        *out_gpt_lba = (uint32_t)first;
        *out_gpt_count = (uint32_t)(last - first + 1);
        log_i("USBMSCFS: GPT FAT partition at LBA %" PRIu32 " (%" PRIu32 " sectors) — synthetic MBR for FatFs",
              *out_gpt_lba,
              *out_gpt_count);
        free(sec);
        return true;
      }
      if (!USBHostMSC.readBlocks(lba, sec, 1)) {
        break;
      }
    }

    log_e("USBMSCFS: GPT disk but no FAT12/16/32 Microsoft Basic Data partition found");
    free(sec);
    return false;
  }

  /* Classic MBR: leave offset 0 and let FatFs scan primary partitions. */
  bool any_part = false;
  for (int i = 0; i < 4; i++) {
    const uint8_t sys = sec[0x1BE + i * 16 + 4];
    const uint32_t start = ld_u32(sec + 0x1BE + i * 16 + 8);
    if (sys != 0 && start != 0) {
      any_part = true;
      log_d("USBMSCFS: MBR part[%d] sys=0x%02x start=%" PRIu32, i, (unsigned)sys, start);
    }
  }
  if (!any_part) {
    usbmsc_log_sector0_diag(sec, bsize);
    log_e("USBMSCFS: LBA0 is not FAT VBR and has no MBR/GPT partitions (bad read or unknown layout)");
    free(sec);
    return false;
  }

  free(sec);
  return true;
}

static DSTATUS ff_usbmsc_status(unsigned char pdrv) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr || !USBHostMSC.mounted()) {
    return STA_NOINIT;
  }
  return 0;
}

static DSTATUS ff_usbmsc_initialize(unsigned char pdrv) {
  return ff_usbmsc_status(pdrv);
}

static DRESULT ff_usbmsc_read(unsigned char pdrv, unsigned char *buff, uint32_t sector, unsigned count) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr || buff == nullptr || count == 0) {
    return RES_PARERR;
  }
  if (!USBHostMSC.mounted()) {
    return RES_NOTRDY;
  }

  const usb_msc_slot_t *slot = s_usbmsc_slots[pdrv];
  const uint32_t bsize = USBHostMSC.blockSize();
  log_v("[USBMSCFS] disk_read pdrv=%u sector=%" PRIu32 " count=%u synth=%d",
        (unsigned)pdrv,
        (uint32_t)sector,
        (unsigned)count,
        (int)slot->use_synth_mbr);

  if (slot->use_synth_mbr && sector == 0) {
    if (bsize < 512) {
      return RES_ERROR;
    }
    /* FatFs only needs the MBR in the first 512 bytes of LBA 0. */
    memset(buff, 0, (size_t)bsize * (size_t)count);
    usbmsc_fill_synth_mbr(buff, slot->gpt_part_lba, slot->gpt_part_count);
    if (count > 1) {
      if (!USBHostMSC.readBlocks(1, buff + bsize, (uint32_t)count - 1u)) {
        return RES_ERROR;
      }
    }
    return RES_OK;
  }

  return USBHostMSC.readBlocks(sector, buff, (uint32_t)count) ? RES_OK : RES_ERROR;
}

static DRESULT ff_usbmsc_write(unsigned char pdrv, const unsigned char *buff, uint32_t sector, unsigned count) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr || buff == nullptr || count == 0) {
    return RES_PARERR;
  }
  if (!USBHostMSC.mounted()) {
    return RES_NOTRDY;
  }

  const usb_msc_slot_t *slot = s_usbmsc_slots[pdrv];
  log_v("[USBMSCFS] disk_write pdrv=%u sector=%" PRIu32 " count=%u",
        (unsigned)pdrv,
        (uint32_t)sector,
        (unsigned)count);

  if (slot->use_synth_mbr && sector == 0) {
    /* Do not overwrite the real protective GPT MBR with our synthetic table. */
    if (count > 1) {
      if (!USBHostMSC.writeBlocks(1, buff + USBHostMSC.blockSize(), (uint32_t)count - 1u)) {
        return RES_ERROR;
      }
    }
    return RES_OK;
  }

  return USBHostMSC.writeBlocks(sector, buff, (uint32_t)count) ? RES_OK : RES_ERROR;
}

static DRESULT ff_usbmsc_ioctl(unsigned char pdrv, unsigned char cmd, void *buff) {
  if (pdrv >= FF_VOLUMES || s_usbmsc_slots[pdrv] == nullptr) {
    return RES_PARERR;
  }
  switch (cmd) {
    case CTRL_SYNC:
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

static void usbmsc_begin_cleanup(const char *mountpoint, uint8_t pdrv, bool vfs_registered) {
  if (vfs_registered && mountpoint != nullptr) {
    esp_vfs_fat_unregister_path(mountpoint);
  }
  ff_diskio_register(pdrv, NULL);
  usbmsc_free_slot(pdrv);
}

bool USBMSCFS::begin(const char *mountpoint, uint8_t max_files, bool format_if_empty) {
  if (_pdrv != 0xFF) {
    return true;
  }
  if (mountpoint == nullptr || !USBHostMSC.mounted()) {
    return false;
  }

  uint32_t gpt_lba = 0;
  uint32_t gpt_count = 0;
  bool is_exfat = false;
  if (!usbmsc_probe_volume(&gpt_lba, &gpt_count, &is_exfat)) {
    if (is_exfat) {
#if FF_FS_EXFAT
      log_e("USBMSCFS: exFAT detected but mount probe failed");
#else
      log_e("USBMSCFS: disk/partition is exFAT — Arduino FatFs has exFAT disabled; reformat as FAT32");
#endif
    }
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
  slot->use_synth_mbr = (gpt_lba != 0);
  slot->gpt_part_lba = gpt_lba;
  slot->gpt_part_count = gpt_count;

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
    usbmsc_begin_cleanup(mountpoint, pdrv, false);
    _pdrv = 0xFF;
    return false;
  }

  FRESULT res = f_mount(fs, drv, 1);
  if (res != FR_OK) {
    if (res == FR_NO_FILESYSTEM) {
      uint8_t *sec0 = (uint8_t *)malloc(USBHostMSC.blockSize());
      const uint32_t diag_lba = (gpt_lba != 0) ? gpt_lba : 0;
      if (sec0 && USBHostMSC.readBlocks(diag_lba, sec0, 1)) {
        usbmsc_log_sector0_diag(sec0, USBHostMSC.blockSize());
      }
      free(sec0);
#if FF_FS_EXFAT
      log_e("USBMSCFS: f_mount failed (13 FR_NO_FILESYSTEM) — not FAT32/FAT16/exFAT, or bad sector read");
#else
      log_e("USBMSCFS: f_mount failed (13 FR_NO_FILESYSTEM) — use FAT32/FAT16 (exFAT disabled), or check USB read/cache");
#endif
    } else {
      log_e("USBMSCFS: f_mount failed (%d)", (int)res);
    }

    if (res == FR_NO_FILESYSTEM && format_if_empty) {
      BYTE *work = (BYTE *)malloc(sizeof(BYTE) * FF_MAX_SS);
      if (work == nullptr) {
        log_e("USBMSCFS: alloc for f_mkfs failed");
        usbmsc_begin_cleanup(mountpoint, pdrv, true);
        _pdrv = 0xFF;
        return false;
      }
      const MKFS_PARM opt = {(BYTE)FM_ANY, 0, 0, 0, 0};
      res = f_mkfs(drv, &opt, work, sizeof(BYTE) * FF_MAX_SS);
      free(work);
      if (res == FR_OK) {
        res = f_mount(fs, drv, 1);
      }
      if (res != FR_OK) {
        log_e("USBMSCFS: format/remount failed (%d)", (int)res);
        usbmsc_begin_cleanup(mountpoint, pdrv, true);
        _pdrv = 0xFF;
        return false;
      }
    } else {
      usbmsc_begin_cleanup(mountpoint, pdrv, true);
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
