/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, hathach for Adafruit
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "tusb_option.h"

#if defined ARDUINO_NRF52_ADAFRUIT && TUSB_OPT_DEVICE_ENABLED

#include "nrfx.h"
#include "nrfx_power.h"

#include "Arduino.h"
#include "arduino/Adafruit_USBD_Device.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

#define USBD_STACK_SZ (200)

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
extern "C" void USBD_IRQHandler(void) {
#if CFG_SYSVIEW
  SEGGER_SYSVIEW_RecordEnterISR();
#endif

  tud_int_handler(0);

#if CFG_SYSVIEW
  SEGGER_SYSVIEW_RecordExitISR();
#endif
}

//--------------------------------------------------------------------+
// Porting API
//--------------------------------------------------------------------+

static void usb_hardware_init(void);

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
static void usb_device_task(void *param) {
  (void)param;

  tusb_init();

  // RTOS forever loop
  while (1) {
    tud_task();
  }
}

void TinyUSB_Port_InitDevice(uint8_t rhport) {
  (void)rhport;

  usb_hardware_init();

  // Create a task for tinyusb device stack
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SZ, NULL, TASK_PRIO_HIGH,
              NULL);
}

void TinyUSB_Port_EnterDFU(void) {
  // Reset to Bootloader
  enterSerialDfu();
}

uint8_t TinyUSB_Port_GetSerialNumber(uint8_t serial_id[16]) {
  uint32_t *serial_32 = (uint32_t *)serial_id;

  serial_32[0] = __builtin_bswap32(NRF_FICR->DEVICEID[1]);
  serial_32[1] = __builtin_bswap32(NRF_FICR->DEVICEID[0]);

  return 8;
}

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+

// tinyusb function that handles power event (detected, ready, removed)
// We must call it within SD's SOC event handler, or set it as power event
// handler if SD is not enabled.
extern "C" void tusb_hal_nrf_power_event(uint32_t event);

static void power_event_handler(nrfx_power_usb_evt_t event) {
  tusb_hal_nrf_power_event((uint32_t)event);
}

// Init usb hardware when starting up. Softdevice is not enabled yet
static void usb_hardware_init(void) {
  // Priorities 0, 1, 4 (nRF52) are reserved for SoftDevice
  // 2 is highest for application
  NVIC_SetPriority(USBD_IRQn, 2);

  // USB power may already be ready at this time -> no event generated
  // We need to invoke the handler based on the status initially
  uint32_t usb_reg = NRF_POWER->USBREGSTATUS;

  // Power module init
  const nrfx_power_config_t pwr_cfg = {0};
  nrfx_power_init(&pwr_cfg);

  // Register tusb function as USB power handler
  const nrfx_power_usbevt_config_t config = {.handler = power_event_handler};

  nrfx_power_usbevt_init(&config);
  nrfx_power_usbevt_enable();

  if (usb_reg & POWER_USBREGSTATUS_VBUSDETECT_Msk) {
    tusb_hal_nrf_power_event(NRFX_POWER_USB_EVT_DETECTED);
  }
}

#endif // USE_TINYUSB
