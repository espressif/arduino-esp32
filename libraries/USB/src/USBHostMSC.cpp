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
#include "tusb_config.h"
#include "esp_heap_caps.h"
#include "soc/soc_caps.h"
#include "hal/cache_hal.h"
#if SOC_CACHE_WRITEBACK_SUPPORTED && __has_include("esp_cache.h")
#include "esp_cache.h"
#define USBHOST_MSC_USE_ESP_CACHE_MSYNC 1
#else
#define USBHOST_MSC_USE_ESP_CACHE_MSYNC 0
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#ifndef USBHOST_MSC_MAX_SECTORS
#define USBHOST_MSC_MAX_SECTORS 1
#endif

#if CFG_TUH_DWC2_DMA_ENABLE
static size_t msc_dma_cache_line_size(void) {
  static size_t line = 0;
  if (line == 0) {
#if CFG_TUH_MEM_DCACHE_LINE_SIZE > 0
    line = (size_t)CFG_TUH_MEM_DCACHE_LINE_SIZE;
#elif defined(CONFIG_CACHE_L1_CACHE_LINE_SIZE) && (CONFIG_CACHE_L1_CACHE_LINE_SIZE > 0)
    line = (size_t)CONFIG_CACHE_L1_CACHE_LINE_SIZE;
#else
    line = 64;
#endif
  }
  return line;
}

static size_t msc_dma_sync_size(size_t nbytes) {
  const size_t line = msc_dma_cache_line_size();
  return (nbytes + line - 1) & ~(line - 1);
}

static void msc_dma_cpu_to_mem(void *p, size_t len) {
  if (p == nullptr || len == 0) {
    return;
  }
  const size_t sync_len = msc_dma_sync_size(len);
#if USBHOST_MSC_USE_ESP_CACHE_MSYNC
  (void)esp_cache_msync(p, sync_len, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
#elif defined(SOC_CACHE_WRITEBACK_SUPPORTED) && SOC_CACHE_WRITEBACK_SUPPORTED
  (void)cache_hal_writeback_addr((uint32_t)(uintptr_t)p, (uint32_t)sync_len);
#endif
}

static void msc_dma_mem_to_cpu(void *p, size_t len) {
  if (p == nullptr || len == 0) {
    return;
  }
  const size_t sync_len = msc_dma_sync_size(len);
#if USBHOST_MSC_USE_ESP_CACHE_MSYNC
  if (esp_cache_msync(p, sync_len, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE | ESP_CACHE_MSYNC_FLAG_TYPE_DATA) !=
      ESP_OK) {
    (void)cache_hal_invalidate_addr((uint32_t)(uintptr_t)p, (uint32_t)sync_len);
  }
#elif defined(SOC_CACHE_WRITEBACK_SUPPORTED) && SOC_CACHE_WRITEBACK_SUPPORTED
  (void)cache_hal_invalidate_addr((uint32_t)(uintptr_t)p, (uint32_t)sync_len);
#endif
}
#endif /* CFG_TUH_DWC2_DMA_ENABLE */

static SemaphoreHandle_t s_msc_io_mutex;
static SemaphoreHandle_t s_scsi_done_sem = nullptr;
static volatile bool s_scsi_ok;
static volatile bool s_ctrl_done;
static volatile bool s_ctrl_ok;

static void ensure_scsi_sem(void) {
  if (s_scsi_done_sem == nullptr) {
    s_scsi_done_sem = xSemaphoreCreateBinary();
  }
}

static void scsi_xfer_begin(void) {
  ensure_scsi_sem();
  while (xSemaphoreTake(s_scsi_done_sem, 0) == pdTRUE) {
  }
  s_scsi_ok = false;
}

static void pump_tuh(void) {
  if (!USBHost.tuhBackgroundActive()) {
    tuh_task();
  }
  yield();
}

static bool wait_msc_ep_ready(uint8_t dev_addr, uint32_t timeout_ms) {
  const uint32_t start = millis();
  while (dev_addr != 0 && !tuh_msc_ready(dev_addr)) {
    pump_tuh();
    if ((millis() - start) > timeout_ms) {
      return false;
    }
  }
  return true;
}

static void ctrl_complete_cb(tuh_xfer_t *xfer) {
  s_ctrl_ok = (xfer->result == XFER_RESULT_SUCCESS);
  s_ctrl_done = true;
}

static bool msc_control_xfer(uint8_t daddr, const tusb_control_request_t *req) {
  s_ctrl_done = false;
  s_ctrl_ok = false;
  tuh_xfer_t xfer = {};
  xfer.daddr = daddr;
  xfer.setup = req;
  xfer.complete_cb = ctrl_complete_cb;
  if (!tuh_control_xfer(&xfer)) {
    return false;
  }
  const uint32_t start = millis();
  while (!s_ctrl_done) {
    pump_tuh();
    if ((millis() - start) > 2000u) {
      return false;
    }
  }
  return s_ctrl_ok;
}

static void *msc_dma_alloc(size_t nbytes) {
#if CFG_TUH_DWC2_DMA_ENABLE
  const size_t align = msc_dma_cache_line_size();
  const size_t alloc_sz = msc_dma_sync_size(nbytes);
  void *p = heap_caps_aligned_alloc(align, alloc_sz, MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_INTERNAL);
  if (p == nullptr) {
    p = heap_caps_aligned_alloc(align, alloc_sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  }
  if (p == nullptr) {
    log_e("[USBHostMSC] aligned DMA alloc failed (%u bytes)", (unsigned)alloc_sz);
  }
  return p;
#else
  void *p = heap_caps_malloc(nbytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  return (p != nullptr) ? p : heap_caps_malloc(nbytes, MALLOC_CAP_DMA);
#endif
}

static void msc_dma_free_safe(uint8_t dev_addr, void *p) {
  if (p == nullptr) {
    return;
  }
  if (dev_addr != 0 && !tuh_msc_ready(dev_addr) && !wait_msc_ep_ready(dev_addr, 2000)) {
    log_e("[USBHostMSC] leaking DMA buf %p — BOT still busy after timeout", p);
    return;
  }
  heap_caps_free(p);
}

static void ensure_msc_mutex(void) {
  if (s_msc_io_mutex == nullptr) {
    s_msc_io_mutex = xSemaphoreCreateRecursiveMutex();
  }
}

static bool msc_complete_cb(uint8_t dev_addr, const tuh_msc_complete_data_t *cb_data) {
  (void)dev_addr;
  const bool ok = (cb_data->csw->signature == MSC_CSW_SIGNATURE) && (cb_data->csw->status == MSC_CSW_STATUS_PASSED);
  if (!ok) {
    log_e("[USBHostMSC] CSW error: signature=%s status=%u",
          (cb_data->csw->signature == MSC_CSW_SIGNATURE) ? "OK" : "BAD",
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
  while ((millis() - start) <= timeout_ms) {
    const TickType_t wait = USBHost.tuhBackgroundActive() ? pdMS_TO_TICKS(100) : 0;
    if (xSemaphoreTake(s_scsi_done_sem, wait) == pdTRUE) {
      return s_scsi_ok;
    }
    if (!USBHost.tuhBackgroundActive()) {
      tuh_task();
      yield();
    }
  }
  log_e("[USBHostMSC] %s: TIMEOUT after %" PRIu32 " ms (mounted=%d ready=%d)",
        op_tag,
        timeout_ms,
        (int)USBHostMSC.mounted(),
        (int)tuh_msc_ready(USBHostMSC.devAddr()));
  return false;
}

USBHostMSCClass::USBHostMSCClass()
  : _mounted(false), _dev_addr(0), _lun(0), _itf_num(0), _ep_in(0), _ep_out(0), _block_count(0), _block_size(0) {
}

bool USBHostMSCClass::cacheEndpoints(uint8_t dev_addr) {
  _itf_num = 0;
  _ep_in = 0;
  _ep_out = 0;

  uint8_t cfg[256];
  if (tuh_descriptor_get_configuration_sync(dev_addr, 0, cfg, sizeof(cfg)) != XFER_RESULT_SUCCESS) {
    log_e("[USBHostMSC] get configuration descriptor failed");
    return false;
  }

  const tusb_desc_configuration_t *cfg_desc = (const tusb_desc_configuration_t *)cfg;
  uint16_t total = tu_le16toh(cfg_desc->wTotalLength);
  if (total > sizeof(cfg)) {
    total = sizeof(cfg);
  }

  for (uint16_t index = sizeof(tusb_desc_configuration_t); index + 2 <= total;) {
    const uint8_t *p = cfg + index;
    const uint8_t len = p[0];
    const uint8_t type = p[1];
    if (len < 2 || (index + len) > total) {
      break;
    }

    if (type == TUSB_DESC_INTERFACE && len >= sizeof(tusb_desc_interface_t)) {
      const tusb_desc_interface_t *itf = (const tusb_desc_interface_t *)p;
      if (itf->bInterfaceClass == TUSB_CLASS_MSC && itf->bInterfaceSubClass == MSC_SUBCLASS_SCSI &&
          itf->bInterfaceProtocol == MSC_PROTOCOL_BOT) {
        _itf_num = itf->bInterfaceNumber;
        uint16_t ep_index = index + len;
        for (uint8_t n = 0; n < itf->bNumEndpoints && (ep_index + 2) <= total; n++) {
          const uint8_t *ep = cfg + ep_index;
          if (ep[0] < 2 || (ep_index + ep[0]) > total) {
            break;
          }
          if (ep[1] == TUSB_DESC_ENDPOINT && ep[0] >= sizeof(tusb_desc_endpoint_t)) {
            const tusb_desc_endpoint_t *epd = (const tusb_desc_endpoint_t *)ep;
            if (epd->bmAttributes.xfer == TUSB_XFER_BULK) {
              if (tu_edpt_dir(epd->bEndpointAddress) == TUSB_DIR_IN) {
                _ep_in = epd->bEndpointAddress;
              } else {
                _ep_out = epd->bEndpointAddress;
              }
            }
          }
          ep_index = (uint16_t)(ep_index + ep[0]);
        }
        break;
      }
    }
    index = (uint16_t)(index + len);
  }

  if (_ep_in == 0 || _ep_out == 0) {
    log_e("[USBHostMSC] MSC bulk endpoints not found in config");
    return false;
  }
  log_d("[USBHostMSC] itf=%u ep_in=0x%02x ep_out=0x%02x", (unsigned)_itf_num, (unsigned)_ep_in, (unsigned)_ep_out);
  return true;
}

static bool msc_clear_halt(uint8_t daddr, uint8_t ep) {
  tusb_control_request_t clr = {};
  clr.bmRequestType_bit.recipient = TUSB_REQ_RCPT_ENDPOINT;
  clr.bmRequestType_bit.type = TUSB_REQ_TYPE_STANDARD;
  clr.bmRequestType_bit.direction = TUSB_DIR_OUT;
  clr.bRequest = TUSB_REQ_CLEAR_FEATURE;
  clr.wValue = TUSB_REQ_FEATURE_EDPT_HALT;
  clr.wIndex = ep;
  return msc_control_xfer(daddr, &clr);
}

bool USBHostMSCClass::recoverBot(const char *reason) {
  if (_dev_addr == 0) {
    return false;
  }
  if ((_ep_in == 0 || _ep_out == 0) && !cacheEndpoints(_dev_addr)) {
    log_e("[USBHostMSC] BOT recover skipped (%s) — endpoints unknown", reason ? reason : "?");
    return false;
  }

  log_e("[USBHostMSC] BOT recover (%s): abort + MSC reset + clear halt", reason ? reason : "?");

  (void)tuh_edpt_abort_xfer(_dev_addr, _ep_in);
  (void)tuh_edpt_abort_xfer(_dev_addr, _ep_out);
  ensure_scsi_sem();
  s_scsi_ok = false;
  (void)xSemaphoreGive(s_scsi_done_sem);

  tusb_control_request_t reset_req = {};
  reset_req.bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE;
  reset_req.bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS;
  reset_req.bmRequestType_bit.direction = TUSB_DIR_OUT;
  reset_req.bRequest = MSC_REQ_RESET;
  reset_req.wIndex = _itf_num;
  if (!msc_control_xfer(_dev_addr, &reset_req)) {
    log_e("[USBHostMSC] MSC Bulk-Only Reset failed");
  }
  if (!msc_clear_halt(_dev_addr, _ep_in)) {
    log_e("[USBHostMSC] CLEAR_FEATURE ep 0x%02x failed", (unsigned)_ep_in);
  }
  if (!msc_clear_halt(_dev_addr, _ep_out)) {
    log_e("[USBHostMSC] CLEAR_FEATURE ep 0x%02x failed", (unsigned)_ep_out);
  }

  delay(10);
  const bool ready = wait_msc_ep_ready(_dev_addr, 3000);
  log_e("[USBHostMSC] BOT recover done ready=%d", (int)ready);
  return ready;
}

void USBHostMSCClass::onMscMount(uint8_t dev_addr) {
  _dev_addr = dev_addr;
  _lun = 0;
  _itf_num = 0;
  _ep_in = 0;
  _ep_out = 0;
  _block_count = tuh_msc_get_block_count(dev_addr, _lun);
  _block_size = tuh_msc_get_block_size(dev_addr, _lun);
  _mounted = true;
  log_d("[USBHostMSC] mount dev=%u blocks=%u block_size=%u",
        (unsigned)dev_addr, (unsigned)_block_count, (unsigned)_block_size);
}

void USBHostMSCClass::onMscUnmount(uint8_t dev_addr) {
  if (dev_addr != _dev_addr) {
    return;
  }
  _mounted = false;
  _dev_addr = 0;
  _lun = 0;
  _itf_num = 0;
  _ep_in = 0;
  _ep_out = 0;
  _block_count = 0;
  _block_size = 0;
  log_d("[USBHostMSC] umount dev=%u", (unsigned)dev_addr);
}

static bool msc_submit_rw(bool is_write, uint8_t dev, uint8_t lun, void *buf, uint32_t lba, uint16_t blocks) {
  for (int attempt = 0; attempt < 20; attempt++) {
    if (!wait_msc_ep_ready(dev, 3000)) {
      return false;
    }
    scsi_xfer_begin();
    const bool submitted = is_write ? tuh_msc_write10(dev, lun, buf, lba, blocks, msc_complete_cb, 0)
                                    : tuh_msc_read10(dev, lun, buf, lba, blocks, msc_complete_cb, 0);
    if (submitted) {
      return true;
    }
    delay(2);
  }
  return false;
}

bool USBHostMSCClass::xferBlocks(bool is_write, uint32_t lba, void *buffer, uint32_t blocks) {
  if (!_mounted || buffer == nullptr || blocks == 0 || _block_size == 0) {
    return false;
  }

  ensure_msc_mutex();
  if (xSemaphoreTakeRecursive(s_msc_io_mutex, pdMS_TO_TICKS(60000)) != pdTRUE) {
    log_e("[USBHostMSC] %s: mutex timeout", is_write ? "writeBlocks" : "readBlocks");
    return false;
  }

  uint8_t *user = (uint8_t *)buffer;
  uint32_t remaining = blocks;
  uint32_t cur_lba = lba;
  const char *op = is_write ? "write10" : "read10";
  const uint32_t scsi_timeout = is_write ? 8000u : 10000u;
  bool ok = true;

  while (remaining > 0 && ok) {
    uint32_t chunk = remaining;
    if (chunk > (uint32_t)USBHOST_MSC_MAX_SECTORS) {
      chunk = (uint32_t)USBHOST_MSC_MAX_SECTORS;
    }
    const size_t nbytes = (size_t)chunk * (size_t)_block_size;
    uint8_t *dma_buf = (uint8_t *)msc_dma_alloc(nbytes);
    if (dma_buf == nullptr) {
      ok = false;
      break;
    }

    if (is_write) {
      memcpy(dma_buf, user, nbytes);
#if CFG_TUH_DWC2_DMA_ENABLE
      msc_dma_cpu_to_mem(dma_buf, nbytes);
#endif
    }

    bool done = false;
    for (int try_n = 0; try_n < 2 && !done; try_n++) {
      if (!msc_submit_rw(is_write, _dev_addr, _lun, dma_buf, cur_lba, (uint16_t)chunk)) {
        if (try_n == 0 && recoverBot("submit")) {
          continue;
        }
        log_e("[USBHostMSC] %s submit failed lba=%" PRIu32, op, cur_lba);
        break;
      }
      if (wait_scsi(scsi_timeout, op)) {
        done = true;
        break;
      }
      if (try_n == 0) {
        (void)recoverBot("timeout");
      }
    }

    if (!done) {
      msc_dma_free_safe(_dev_addr, dma_buf);
      ok = false;
      break;
    }

    if (!is_write) {
#if CFG_TUH_DWC2_DMA_ENABLE
      msc_dma_mem_to_cpu(dma_buf, nbytes);
#endif
      memcpy(user, dma_buf, nbytes);
    }
    msc_dma_free_safe(_dev_addr, dma_buf);

    if (is_write) {
      /* Pace sector writes — some controllers wedge if hammered with no gap. */
      delay(1);
    }

    user += nbytes;
    cur_lba += chunk;
    remaining -= chunk;
  }

  xSemaphoreGiveRecursive(s_msc_io_mutex);
  return ok;
}

bool USBHostMSCClass::readBlocks(uint32_t lba, void *buffer, uint32_t blocks) {
  return xferBlocks(false, lba, buffer, blocks);
}

bool USBHostMSCClass::writeBlocks(uint32_t lba, const void *buffer, uint32_t blocks) {
  return xferBlocks(true, lba, (void *)buffer, blocks);
}

bool USBHostMSCClass::sync(void) {
  return mounted();
}

USBHostMSCClass USBHostMSC;

extern "C" {

void tuh_msc_mount_cb(uint8_t dev_addr) {
  USBHostMSC.onMscMount(dev_addr);
}

void tuh_msc_umount_cb(uint8_t dev_addr) {
  USBHostMSC.onMscUnmount(dev_addr);
}

} /* extern "C" */

#endif /* SOC_USB_OTG && CFG_TUH_MSC */
