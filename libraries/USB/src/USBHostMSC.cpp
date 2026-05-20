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

#include "USBHostMSC.h"

#if SOC_USB_OTG_SUPPORTED && CONFIG_TINYUSB_ENABLED && CFG_TUH_ENABLED && CFG_TUH_MSC

#include "tusb.h"
#include "class/msc/msc_host.h"
#include "Arduino.h"
#include "USBHost.h"
#include "esp_heap_caps.h"
#include "soc/soc_caps.h"
#include "hal/cache_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

/** CPU filled buffer → RAM visible to USB DMA (bulk OUT). Uses HAL (not esp_cache_msync: DMA heap can be “invalid” for mm). */
static void msc_dma_cpu_to_mem(void *p, size_t len) {
#if defined(SOC_CACHE_WRITEBACK_SUPPORTED) && SOC_CACHE_WRITEBACK_SUPPORTED
  if (p != nullptr && len > 0) {
    const uint32_t va = (uint32_t)(uintptr_t)p;
    const uint32_t sz = (uint32_t)len;
    (void)cache_hal_writeback_addr(va, sz);
  }
#else
  (void)p;
  (void)len;
#endif
}

/** After USB DMA wrote into buffer (bulk IN), invalidate so CPU memcpy sees RAM. */
static void msc_dma_mem_to_cpu(void *p, size_t len) {
  if (p != nullptr && len > 0) {
    const uint32_t va = (uint32_t)(uintptr_t)p;
    const uint32_t sz = (uint32_t)len;
    (void)cache_hal_invalidate_addr(va, sz);
  }
}

/*
 * Must be a recursive mutex: wait_scsi() calls tuh_task(), which can complete SCSI
 * and re-enter FatFS -> disk_read/disk_write -> readBlocks/writeBlocks on the same
 * task. A plain mutex would deadlock on the second xSemaphoreTake.
 */
static SemaphoreHandle_t s_msc_io_mutex;
/** Signals SCSI completion from TinyUSB (usbhTuh task); avoids volatile spin across cores. */
static SemaphoreHandle_t s_scsi_done_sem = nullptr;
static volatile bool s_scsi_ok;

static void ensure_scsi_sem(void) {
  if (s_scsi_done_sem == nullptr) {
    s_scsi_done_sem = xSemaphoreCreateBinary();
  }
}

/** Drain stale signals; call before each read10/write10 submit. */
static void scsi_xfer_begin(void) {
  ensure_scsi_sem();
  while (xSemaphoreTake(s_scsi_done_sem, 0) == pdTRUE) {
  }
  s_scsi_ok = false;
}

/** USB DMA on ESP32-S2/S3/P4 expects buffer alignment (use 32-byte). */
#ifndef USBHOST_MSC_DMA_ALIGN
#define USBHOST_MSC_DMA_ALIGN 32
#endif

static void *msc_dma_alloc(size_t nbytes) {
  /* Prefer 32-byte alignment for USB DMA; fall back if allocator fails. */
  void *p = heap_caps_aligned_alloc(USBHOST_MSC_DMA_ALIGN, nbytes, MALLOC_CAP_DMA);
  if (p != nullptr) {
    return p;
  }
  return heap_caps_malloc(nbytes, MALLOC_CAP_DMA);
}

static void msc_dma_free(void *p) {
  heap_caps_free(p);
}

static void ensure_msc_mutex() {
  if (s_msc_io_mutex == nullptr) {
    s_msc_io_mutex = xSemaphoreCreateRecursiveMutex();
  }
}

static bool msc_complete_cb(uint8_t dev_addr, const tuh_msc_complete_data_t *cb_data) {
  (void)dev_addr;
  const bool sig_ok = (cb_data->csw->signature == MSC_CSW_SIGNATURE);
  const bool st_ok = (cb_data->csw->status == MSC_CSW_STATUS_PASSED);
  const bool ok = sig_ok && st_ok;
  if (!ok) {
    log_v("[USBHostMSC] CSW error: signature=%s (got 0x%08" PRIx32 ") status=%u (want PASSED=0)",
          sig_ok ? "OK" : "BAD",
          (uint32_t)cb_data->csw->signature,
          (unsigned)cb_data->csw->status);
  }
  ensure_scsi_sem();
  s_scsi_ok = ok;
  (void)xSemaphoreGive(s_scsi_done_sem);
  return true;
}

static bool wait_scsi(uint32_t timeout_ms, const char *op_tag) {
  ensure_scsi_sem();
  const uint32_t start = millis();

  if (USBHost.tuhBackgroundActive()) {
    /* Block on semaphore: completion runs on usbhTuh; proper memory ordering vs polling a volatile on another core. */
    if (xSemaphoreTake(s_scsi_done_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
      log_v("[USBHostMSC] %s: TIMEOUT after %" PRIu32 " ms (mounted=%d msc_ready=%d)",
            op_tag,
            timeout_ms,
            (int)USBHostMSC.mounted(),
            (int)tuh_msc_ready(USBHostMSC.devAddr()));
      return false;
    }
    log_v("[USBHostMSC] %s: SCSI finished ok=%d in %" PRIu32 " ms",
          op_tag,
          (int)s_scsi_ok,
          (uint32_t)(millis() - start));
    return s_scsi_ok;
  }

  /* No background worker: poll stack + non-blocking semaphore take */
  uint32_t last_log = start;
  while (1) {
    if (xSemaphoreTake(s_scsi_done_sem, 0) == pdTRUE) {
      log_v("[USBHostMSC] %s: SCSI finished ok=%d in %" PRIu32 " ms",
            op_tag,
            (int)s_scsi_ok,
            (uint32_t)(millis() - start));
      return s_scsi_ok;
    }
    tuh_task();
    yield();
    const uint32_t now = millis();
    if ((now - last_log) >= 500u) {
      last_log = now;
      log_v("[USBHostMSC] %s: still waiting for SCSI CB, elapsed=%" PRIu32 " ms ready=%d",
            op_tag,
            (uint32_t)(now - start),
            (int)tuh_msc_ready(USBHostMSC.devAddr()));
    }
    if ((now - start) > timeout_ms) {
      log_v("[USBHostMSC] %s: TIMEOUT after %" PRIu32 " ms (mounted=%d msc_ready=%d)",
            op_tag,
            timeout_ms,
            (int)USBHostMSC.mounted(),
            (int)tuh_msc_ready(USBHostMSC.devAddr()));
      return false;
    }
  }
}

USBHostMSCCard::USBHostMSCCard()
  : _mounted(false), _dev_addr(0), _lun(0), _block_count(0), _block_size(0) {
}

void USBHostMSCCard::onMscMount(uint8_t dev_addr) {
  _dev_addr = dev_addr;
  _lun = 0;
  _block_count = tuh_msc_get_block_count(dev_addr, _lun);
  _block_size = tuh_msc_get_block_size(dev_addr, _lun);
  _mounted = true;
  log_v("[USBHostMSC] mount dev=%u blocks=%u block_size=%u",
        (unsigned)dev_addr, (unsigned)_block_count, (unsigned)_block_size);
}

void USBHostMSCCard::onMscUnmount(uint8_t dev_addr) {
  if (dev_addr != _dev_addr) {
    return;
  }
  _mounted = false;
  _dev_addr = 0;
  _lun = 0;
  _block_count = 0;
  _block_size = 0;
  log_v("[USBHostMSC] umount dev=%u", (unsigned)dev_addr);
}

bool USBHostMSCCard::readBlocks(uint32_t lba, void *buffer, uint32_t blocks) {
  if (!_mounted || buffer == nullptr || blocks == 0 || _block_size == 0) {
    log_v("[USBHostMSC] readBlocks reject: mounted=%d buf=%p blocks=%u bsize=%u",
          (int)_mounted, buffer, (unsigned)blocks, (unsigned)_block_size);
    return false;
  }
  ensure_msc_mutex();
  if (xSemaphoreTakeRecursive(s_msc_io_mutex, pdMS_TO_TICKS(60000)) != pdTRUE) {
    log_v("[USBHostMSC] readBlocks: mutex timeout (60s)");
    return false;
  }

  log_v("[USBHostMSC] readBlocks enter lba=%" PRIu32 " blocks=%" PRIu32 " dev=%u lun=%u",
        (uint32_t)lba, (uint32_t)blocks, (unsigned)_dev_addr, (unsigned)_lun);

  uint8_t *user = (uint8_t *)buffer;
  uint32_t remaining = blocks;
  uint32_t cur_lba = lba;
  bool ok = true;

  while (remaining > 0 && ok) {
    uint32_t chunk = remaining;
    if (chunk > 0xFFFFu) {
      chunk = 0xFFFFu;
    }
    const size_t nbytes = (size_t)chunk * (size_t)_block_size;
    uint8_t *dma_buf = (uint8_t *)msc_dma_alloc(nbytes);
    if (dma_buf == nullptr) {
      log_v("[USBHostMSC] readBlocks: DMA alloc failed %" PRIu32 " bytes", (uint32_t)nbytes);
      ok = false;
      break;
    }

    scsi_xfer_begin();
    log_v("[USBHostMSC] read10 submit lba=%" PRIu32 " chunk=%" PRIu32 " nbytes=%u ready=%d",
          (uint32_t)cur_lba, (uint32_t)chunk, (unsigned)nbytes, (int)tuh_msc_ready(_dev_addr));
    if (!tuh_msc_read10(_dev_addr, _lun, dma_buf, cur_lba, (uint16_t)chunk, msc_complete_cb, 0)) {
      log_v("[USBHostMSC] read10 submit FAILED (stack busy?)");
      msc_dma_free(dma_buf);
      ok = false;
      break;
    }
    if (!wait_scsi(60000, "read10")) {
      msc_dma_free(dma_buf);
      ok = false;
      break;
    }
    msc_dma_mem_to_cpu(dma_buf, nbytes);
    memcpy(user, dma_buf, nbytes);
    msc_dma_free(dma_buf);

    user += nbytes;
    cur_lba += chunk;
    remaining -= chunk;
  }

  xSemaphoreGiveRecursive(s_msc_io_mutex);
  log_v("[USBHostMSC] readBlocks leave ok=%d", (int)ok);
  return ok;
}

bool USBHostMSCCard::writeBlocks(uint32_t lba, const void *buffer, uint32_t blocks) {
  if (!_mounted || buffer == nullptr || blocks == 0 || _block_size == 0) {
    log_v("[USBHostMSC] writeBlocks reject: mounted=%d buf=%p blocks=%u bsize=%u",
          (int)_mounted, buffer, (unsigned)blocks, (unsigned)_block_size);
    return false;
  }
  ensure_msc_mutex();
  if (xSemaphoreTakeRecursive(s_msc_io_mutex, pdMS_TO_TICKS(60000)) != pdTRUE) {
    log_v("[USBHostMSC] writeBlocks: mutex timeout (60s)");
    return false;
  }

  log_v("[USBHostMSC] writeBlocks enter lba=%" PRIu32 " blocks=%" PRIu32 " dev=%u lun=%u",
        (uint32_t)lba, (uint32_t)blocks, (unsigned)_dev_addr, (unsigned)_lun);

  const uint8_t *user = (const uint8_t *)buffer;
  uint32_t remaining = blocks;
  uint32_t cur_lba = lba;
  bool ok = true;

  while (remaining > 0 && ok) {
    uint32_t chunk = remaining;
    if (chunk > 0xFFFFu) {
      chunk = 0xFFFFu;
    }
    const size_t nbytes = (size_t)chunk * (size_t)_block_size;
    uint8_t *dma_buf = (uint8_t *)msc_dma_alloc(nbytes);
    if (dma_buf == nullptr) {
      log_v("[USBHostMSC] writeBlocks: DMA alloc failed %" PRIu32 " bytes", (uint32_t)nbytes);
      ok = false;
      break;
    }
    memcpy(dma_buf, user, nbytes);
    /* Bulk OUT: USB DMA reads RAM; flush CPU cache so the device does not see stale lines. */
    msc_dma_cpu_to_mem(dma_buf, nbytes);

    scsi_xfer_begin();
    log_v("[USBHostMSC] write10 submit lba=%" PRIu32 " chunk=%" PRIu32 " nbytes=%u ready=%d",
          (uint32_t)cur_lba, (uint32_t)chunk, (unsigned)nbytes, (int)tuh_msc_ready(_dev_addr));
    if (!tuh_msc_write10(_dev_addr, _lun, dma_buf, cur_lba, (uint16_t)chunk, msc_complete_cb, 0)) {
      log_v("[USBHostMSC] write10 submit FAILED (stack busy?)");
      msc_dma_free(dma_buf);
      ok = false;
      break;
    }
    if (!wait_scsi(120000, "write10")) {
      msc_dma_free(dma_buf);
      ok = false;
      break;
    }
    msc_dma_free(dma_buf);

    user += nbytes;
    cur_lba += chunk;
    remaining -= chunk;
  }

  xSemaphoreGiveRecursive(s_msc_io_mutex);
  log_v("[USBHostMSC] writeBlocks leave ok=%d", (int)ok);
  return ok;
}

USBHostMSCCard USBHostMSC;

extern "C" {

void tuh_msc_mount_cb(uint8_t dev_addr) {
  USBHostMSC.onMscMount(dev_addr);
}

void tuh_msc_umount_cb(uint8_t dev_addr) {
  USBHostMSC.onMscUnmount(dev_addr);
}

} /* extern "C" */

#endif /* SOC_USB_OTG && CFG_TUH_MSC */
