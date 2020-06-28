// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
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

#include <stdbool.h>
#include "descriptors_control.h"
#include "board.h"
#include "tusb.h"
#include "tusb_config.h"

#ifdef __cplusplus
extern "C" {
#endif


/* tinyusb uses buffers with type of uint8_t[] but in our driver we are reading them as a 32-bit word */
#if (CFG_TUD_ENDOINT0_SIZE < 4)
#   define CFG_TUD_ENDOINT0_SIZE 4
#   warning "CFG_TUD_ENDOINT0_SIZE was too low and was set to 4"
#endif

#if TUSB_OPT_DEVICE_ENABLED

#   if CFG_TUD_HID
#      if (CFG_TUD_HID_BUFSIZE < 4)
#         define CFG_TUD_HID_BUFSIZE 4
#         warning "CFG_TUD_HID_BUFSIZE was too low and was set to 4"
#      endif
#   endif

#   if CFG_TUD_CDC
#      if (CFG_TUD_CDC_EPSIZE < 4)
#         define CFG_TUD_CDC_EPSIZE 4
#         warning "CFG_TUD_CDC_EPSIZE was too low and was set to 4"
#      endif
#   endif

#   if CFG_TUD_MSC
#      if (CFG_TUD_MSC_BUFSIZE < 4)
#         define CFG_TUD_MSC_BUFSIZE 4
#         warning "CFG_TUD_MSC_BUFSIZE was too low and was set to 4"
#      endif
#   endif

#   if CFG_TUD_MIDI
#       if (CFG_TUD_MIDI_EPSIZE < 4)
#          define CFG_TUD_MIDI_EPSIZE 4
#          warning "CFG_TUD_MIDI_EPSIZE was too low and was set to 4"
#       endif
#   endif

#   if CFG_TUD_VENDOR
#       if (CFG_TUD_VENDOR_EPSIZE < 4)
#          define CFG_TUD_VENDOR_EPSIZE 4
#          warning "CFG_TUD_VENDOR_EPSIZE was too low and was set to 4"
#       endif
#   endif

#   if CFG_TUD_CUSTOM_CLASS
#          warning "Please check that the buffer is more then 4 bytes"
#   endif
#endif

typedef struct {
    bool external_phy;
} tinyusb_config_t;

esp_err_t tinyusb_driver_install(const tinyusb_config_t *config);
// TODO esp_err_t tinyusb_driver_uninstall(void); (IDF-1474)

/*
 * USB Persistence API
 * */
typedef enum {
    REBOOT_NO_PERSIST,
    REBOOT_PERSIST,
    REBOOT_BOOTLOADER,
    REBOOT_BOOTLOADER_DFU
} reboot_type_t;

/*
 * Enable Persist reboot
 * 
 * Note: Persistence should be enabled when ONLY CDC and DFU are being used
 * */
void tinyusb_persist_set_enable(bool enable);

/*
 * Set Persist reboot mode, if available, before calling esp_reboot();
 * */
void tinyusb_persist_set_mode(reboot_type_t mode);

/*
 * TinyUSB Dynamic Driver Loading
 * 
 * When enabled, usb drivers can be loaded at runtime, before calling tinyusb_driver_install()
 * */
#if CFG_TUSB_DYNAMIC_DRIVER_LOAD
#if CFG_TUSB_DEBUG >= 2
  #define DRIVER_NAME(_name)    .name = _name,
#else
  #define DRIVER_NAME(_name)
#endif

typedef struct
{
  #if CFG_TUSB_DEBUG >= 2
  char const* name;
  #endif

  void     (* init             ) (void);
  void     (* reset            ) (uint8_t rhport);
  uint16_t (* open             ) (uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len);
  bool     (* control_request  ) (uint8_t rhport, tusb_control_request_t const * request);
  bool     (* control_complete ) (uint8_t rhport, tusb_control_request_t const * request);
  bool     (* xfer_cb          ) (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
  void     (* sof              ) (uint8_t rhport); /* optional */
} usbd_class_driver_t;

bool usbd_device_driver_load(const usbd_class_driver_t * driver);

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOAD_DEFAULT_TUSB_DRIVER(name) \
    static usbd_class_driver_t name ## d_driver = { \
        DRIVER_NAME(TOSTRING(name)) \
        .init             = name ## d_init, \
        .reset            = name ## d_reset, \
        .open             = name ## d_open, \
        .control_request  = name ## d_control_request, \
        .control_complete = name ## d_control_complete, \
        .xfer_cb          = name ## d_xfer_cb, \
        .sof              = NULL \
    }; \
    TU_VERIFY (usbd_device_driver_load(&name ## d_driver));
#endif /* CFG_TUSB_DYNAMIC_DRIVER_LOAD */

#ifdef __cplusplus
}
#endif
