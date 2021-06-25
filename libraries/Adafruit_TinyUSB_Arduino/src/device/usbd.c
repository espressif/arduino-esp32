/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED

#include "tusb.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "device/dcd.h"

#ifndef CFG_TUD_TASK_QUEUE_SZ
#define CFG_TUD_TASK_QUEUE_SZ   16
#endif

#ifndef CFG_TUD_EP_MAX
#define CFG_TUD_EP_MAX          9
#endif

//--------------------------------------------------------------------+
// Device Data
//--------------------------------------------------------------------+

// Invalid driver ID in itf2drv[] ep2drv[][] mapping
enum { DRVID_INVALID = 0xFFu };

typedef struct
{
  struct TU_ATTR_PACKED
  {
    volatile uint8_t connected    : 1;
    volatile uint8_t addressed    : 1;
    volatile uint8_t suspended    : 1;

    uint8_t remote_wakeup_en      : 1; // enable/disable by host
    uint8_t remote_wakeup_support : 1; // configuration descriptor's attribute
    uint8_t self_powered          : 1; // configuration descriptor's attribute
  };

  volatile uint8_t cfg_num; // current active configuration (0x00 is not configured)
  uint8_t speed;

  uint8_t itf2drv[16];     // map interface number to driver (0xff is invalid)
  uint8_t ep2drv[CFG_TUD_EP_MAX][2]; // map endpoint to driver ( 0xff is invalid )

  struct TU_ATTR_PACKED
  {
    volatile bool busy    : 1;
    volatile bool stalled : 1;
    volatile bool claimed : 1;

    // TODO merge ep2drv here, 4-bit should be sufficient
  }ep_status[CFG_TUD_EP_MAX][2];

}usbd_device_t;

static usbd_device_t _usbd_dev;

//--------------------------------------------------------------------+
// Class Driver
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= 2
  #define DRIVER_NAME(_name)    .name = _name,
#else
  #define DRIVER_NAME(_name)
#endif

// Built-in class drivers
static usbd_class_driver_t const _usbd_driver[] =
{
  #if CFG_TUD_CDC
  {
    DRIVER_NAME("CDC")
    .init             = cdcd_init,
    .reset            = cdcd_reset,
    .open             = cdcd_open,
    .control_xfer_cb  = cdcd_control_xfer_cb,
    .xfer_cb          = cdcd_xfer_cb,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_MSC
  {
    DRIVER_NAME("MSC")
    .init             = mscd_init,
    .reset            = mscd_reset,
    .open             = mscd_open,
    .control_xfer_cb  = mscd_control_xfer_cb,
    .xfer_cb          = mscd_xfer_cb,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_HID
  {
    DRIVER_NAME("HID")
    .init             = hidd_init,
    .reset            = hidd_reset,
    .open             = hidd_open,
    .control_xfer_cb  = hidd_control_xfer_cb,
    .xfer_cb          = hidd_xfer_cb,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_AUDIO
  {
    DRIVER_NAME("AUDIO")
    .init             = audiod_init,
    .reset            = audiod_reset,
    .open             = audiod_open,
    .control_xfer_cb  = audiod_control_xfer_cb,
    .xfer_cb          = audiod_xfer_cb,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_MIDI
  {
    DRIVER_NAME("MIDI")
    .init             = midid_init,
    .open             = midid_open,
    .reset            = midid_reset,
    .control_xfer_cb  = midid_control_xfer_cb,
    .xfer_cb          = midid_xfer_cb,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_VENDOR
  {
    DRIVER_NAME("VENDOR")
    .init             = vendord_init,
    .reset            = vendord_reset,
    .open             = vendord_open,
    .control_xfer_cb  = tud_vendor_control_xfer_cb,
    .xfer_cb          = vendord_xfer_cb,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_USBTMC
  {
    DRIVER_NAME("TMC")
    .init             = usbtmcd_init_cb,
    .reset            = usbtmcd_reset_cb,
    .open             = usbtmcd_open_cb,
    .control_xfer_cb  = usbtmcd_control_xfer_cb,
    .xfer_cb          = usbtmcd_xfer_cb,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_DFU_RUNTIME
  {
    DRIVER_NAME("DFU-RUNTIME")
    .init             = dfu_rtd_init,
    .reset            = dfu_rtd_reset,
    .open             = dfu_rtd_open,
    .control_xfer_cb  = dfu_rtd_control_xfer_cb,
    .xfer_cb          = NULL,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_DFU_MODE
  {
    DRIVER_NAME("DFU-MODE")
    .init             = dfu_moded_init,
    .reset            = dfu_moded_reset,
    .open             = dfu_moded_open,
    .control_xfer_cb  = dfu_moded_control_xfer_cb,
    .xfer_cb          = NULL,
    .sof              = NULL
  },
  #endif

  #if CFG_TUD_NET
  {
    DRIVER_NAME("NET")
    .init             = netd_init,
    .reset            = netd_reset,
    .open             = netd_open,
    .control_xfer_cb  = netd_control_xfer_cb,
    .xfer_cb          = netd_xfer_cb,
    .sof              = NULL,
  },
  #endif

  #if CFG_TUD_BTH
  {
    DRIVER_NAME("BTH")
    .init             = btd_init,
    .reset            = btd_reset,
    .open             = btd_open,
    .control_xfer_cb  = btd_control_xfer_cb,
    .xfer_cb          = btd_xfer_cb,
    .sof              = NULL
  },
  #endif
};

enum { BUILTIN_DRIVER_COUNT = TU_ARRAY_SIZE(_usbd_driver) };

// Additional class drivers implemented by application
static usbd_class_driver_t const * _app_driver = NULL;
static uint8_t _app_driver_count = 0;

// virtually joins built-in and application drivers together.
// Application is positioned first to allow overwriting built-in ones.
static inline usbd_class_driver_t const * get_driver(uint8_t drvid)
{
  // Application drivers
  if ( usbd_app_driver_get_cb )
  {
    if ( drvid < _app_driver_count ) return &_app_driver[drvid];
    drvid -= _app_driver_count;
  }

  // Built-in drivers
  if (drvid < BUILTIN_DRIVER_COUNT) return &_usbd_driver[drvid];

  return NULL;
}

#define TOTAL_DRIVER_COUNT    (_app_driver_count + BUILTIN_DRIVER_COUNT)

//--------------------------------------------------------------------+
// DCD Event
//--------------------------------------------------------------------+

static bool _usbd_initialized = false;

// Event queue
// OPT_MODE_DEVICE is used by OS NONE for mutex (disable usb isr)
OSAL_QUEUE_DEF(OPT_MODE_DEVICE, _usbd_qdef, CFG_TUD_TASK_QUEUE_SZ, dcd_event_t);
static osal_queue_t _usbd_q;

// Mutex for claiming endpoint, only needed when using with preempted RTOS
#if CFG_TUSB_OS != OPT_OS_NONE
static osal_mutex_def_t _ubsd_mutexdef;
static osal_mutex_t _usbd_mutex;
#endif


//--------------------------------------------------------------------+
// Prototypes
//--------------------------------------------------------------------+
static void mark_interface_endpoint(uint8_t ep2drv[][2], uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id);
static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request);
static bool process_set_config(uint8_t rhport, uint8_t cfg_num);
static bool process_get_descriptor(uint8_t rhport, tusb_control_request_t const * p_request);

// from usbd_control.c
void usbd_control_reset(void);
void usbd_control_set_request(tusb_control_request_t const *request);
void usbd_control_set_complete_callback( usbd_control_xfer_cb_t fp );
bool usbd_control_xfer_cb (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);


//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= 2
static char const* const _usbd_event_str[DCD_EVENT_COUNT] =
{
  "Invalid"        ,
  "Bus Reset"      ,
  "Unplugged"      ,
  "SOF"            ,
  "Suspend"        ,
  "Resume"         ,
  "Setup Received" ,
  "Xfer Complete"  ,
  "Func Call"
};

static char const* const _tusb_std_request_str[] =
{
  "Get Status"        ,
  "Clear Feature"     ,
  "Reserved"          ,
  "Set Feature"       ,
  "Reserved"          ,
  "Set Address"       ,
  "Get Descriptor"    ,
  "Set Descriptor"    ,
  "Get Configuration" ,
  "Set Configuration" ,
  "Get Interface"     ,
  "Set Interface"     ,
  "Synch Frame"
};

static char const* const _tusb_speed_str[] = { "Full", "Low", "High" };

// for usbd_control to print the name of control complete driver
void usbd_driver_print_control_complete_name(usbd_control_xfer_cb_t callback)
{
  for (uint8_t i = 0; i < TOTAL_DRIVER_COUNT; i++)
  {
    usbd_class_driver_t const * driver = get_driver(i);
    if ( driver->control_xfer_cb == callback )
    {
      TU_LOG2("  %s control complete\r\n", driver->name);
      return;
    }
  }
}

#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+
tusb_speed_t tud_speed_get(void)
{
  return (tusb_speed_t) _usbd_dev.speed;
}

bool tud_connected(void)
{
  return _usbd_dev.connected;
}

bool tud_mounted(void)
{
  return _usbd_dev.cfg_num ? true : false;
}

bool tud_suspended(void)
{
  return _usbd_dev.suspended;
}

bool tud_remote_wakeup(void)
{
  // only wake up host if this feature is supported and enabled and we are suspended
  TU_VERIFY (_usbd_dev.suspended && _usbd_dev.remote_wakeup_support && _usbd_dev.remote_wakeup_en );
  dcd_remote_wakeup(TUD_OPT_RHPORT);
  return true;
}

bool tud_disconnect(void)
{
  TU_VERIFY(dcd_disconnect);
  dcd_disconnect(TUD_OPT_RHPORT);
  return true;
}

bool tud_connect(void)
{
  TU_VERIFY(dcd_connect);
  dcd_connect(TUD_OPT_RHPORT);
  return true;
}

//--------------------------------------------------------------------+
// USBD Task
//--------------------------------------------------------------------+
bool tud_inited(void)
{
  return _usbd_initialized;
}

bool tud_init (uint8_t rhport)
{
  // skip if already initialized
  if (_usbd_initialized) return _usbd_initialized;

  TU_LOG2("USBD init\r\n");

  tu_varclr(&_usbd_dev);

#if CFG_TUSB_OS != OPT_OS_NONE
  // Init device mutex
  _usbd_mutex = osal_mutex_create(&_ubsd_mutexdef);
  TU_ASSERT(_usbd_mutex);
#endif

  // Init device queue & task
  _usbd_q = osal_queue_create(&_usbd_qdef);
  TU_ASSERT(_usbd_q);

  // Get application driver if available
  if ( usbd_app_driver_get_cb )
  {
    _app_driver = usbd_app_driver_get_cb(&_app_driver_count);
  }

  // Init class drivers
  for (uint8_t i = 0; i < TOTAL_DRIVER_COUNT; i++)
  {
    usbd_class_driver_t const * driver = get_driver(i);
    TU_LOG2("%s init\r\n", driver->name);
    driver->init();
  }

  // Init device controller driver
  dcd_init(rhport);
  dcd_int_enable(rhport);

  _usbd_initialized = true;

  return true;
}

static void usbd_reset(uint8_t rhport)
{
  tu_varclr(&_usbd_dev);

  memset(_usbd_dev.itf2drv, DRVID_INVALID, sizeof(_usbd_dev.itf2drv)); // invalid mapping
  memset(_usbd_dev.ep2drv , DRVID_INVALID, sizeof(_usbd_dev.ep2drv )); // invalid mapping

  usbd_control_reset();

  for ( uint8_t i = 0; i < TOTAL_DRIVER_COUNT; i++ )
  {
    get_driver(i)->reset(rhport);
  }
}

bool tud_task_event_ready(void)
{
  // Skip if stack is not initialized
  if ( !tusb_inited() ) return false;

  return !osal_queue_empty(_usbd_q);
}

/* USB Device Driver task
 * This top level thread manages all device controller event and delegates events to class-specific drivers.
 * This should be called periodically within the mainloop or rtos thread.
 *
   @code
    int main(void)
    {
      application_init();
      tusb_init();

      while(1) // the mainloop
      {
        application_code();
        tud_task(); // tinyusb device task
      }
    }
    @endcode
 */
void tud_task (void)
{
  // Skip if stack is not initialized
  if ( !tusb_inited() ) return;

  // Loop until there is no more events in the queue
  while (1)
  {
    dcd_event_t event;

    if ( !osal_queue_receive(_usbd_q, &event) ) return;

#if CFG_TUSB_DEBUG >= 2
    if (event.event_id == DCD_EVENT_SETUP_RECEIVED) TU_LOG2("\r\n"); // extra line for setup
    TU_LOG2("USBD %s ", event.event_id < DCD_EVENT_COUNT ? _usbd_event_str[event.event_id] : "CORRUPTED");
#endif

    switch ( event.event_id )
    {
      case DCD_EVENT_BUS_RESET:
        TU_LOG2(": %s Speed\r\n", _tusb_speed_str[event.bus_reset.speed]);
        usbd_reset(event.rhport);
        _usbd_dev.speed = event.bus_reset.speed;
      break;

      case DCD_EVENT_UNPLUGGED:
        TU_LOG2("\r\n");
        usbd_reset(event.rhport);

        // invoke callback
        if (tud_umount_cb) tud_umount_cb();
      break;

      case DCD_EVENT_SETUP_RECEIVED:
        TU_LOG2_VAR(&event.setup_received);
        TU_LOG2("\r\n");

        // Mark as connected after receiving 1st setup packet.
        // But it is easier to set it every time instead of wasting time to check then set
        _usbd_dev.connected = 1;

        // mark both in & out control as free
        _usbd_dev.ep_status[0][TUSB_DIR_OUT].busy = false;
        _usbd_dev.ep_status[0][TUSB_DIR_OUT].claimed = 0;
        _usbd_dev.ep_status[0][TUSB_DIR_IN ].busy = false;
        _usbd_dev.ep_status[0][TUSB_DIR_IN ].claimed = 0;

        // Process control request
        if ( !process_control_request(event.rhport, &event.setup_received) )
        {
          TU_LOG2("  Stall EP0\r\n");
          // Failed -> stall both control endpoint IN and OUT
          dcd_edpt_stall(event.rhport, 0);
          dcd_edpt_stall(event.rhport, 0 | TUSB_DIR_IN_MASK);
        }
      break;

      case DCD_EVENT_XFER_COMPLETE:
      {
        // Invoke the class callback associated with the endpoint address
        uint8_t const ep_addr = event.xfer_complete.ep_addr;
        uint8_t const epnum   = tu_edpt_number(ep_addr);
        uint8_t const ep_dir  = tu_edpt_dir(ep_addr);

        TU_LOG2("on EP %02X with %u bytes\r\n", ep_addr, (unsigned int) event.xfer_complete.len);

        _usbd_dev.ep_status[epnum][ep_dir].busy = false;
        _usbd_dev.ep_status[epnum][ep_dir].claimed = 0;

        if ( 0 == epnum )
        {
          usbd_control_xfer_cb(event.rhport, ep_addr, (xfer_result_t)event.xfer_complete.result, event.xfer_complete.len);
        }
        else
        {
          usbd_class_driver_t const * driver = get_driver( _usbd_dev.ep2drv[epnum][ep_dir] );
          TU_ASSERT(driver, );

          TU_LOG2("  %s xfer callback\r\n", driver->name);
          driver->xfer_cb(event.rhport, ep_addr, (xfer_result_t)event.xfer_complete.result, event.xfer_complete.len);
        }
      }
      break;

      case DCD_EVENT_SUSPEND:
        TU_LOG2("\r\n");
        if (tud_suspend_cb) tud_suspend_cb(_usbd_dev.remote_wakeup_en);
      break;

      case DCD_EVENT_RESUME:
        TU_LOG2("\r\n");
        if (tud_resume_cb) tud_resume_cb();
      break;

      case DCD_EVENT_SOF:
        TU_LOG2("\r\n");
        for ( uint8_t i = 0; i < TOTAL_DRIVER_COUNT; i++ )
        {
          usbd_class_driver_t const * driver = get_driver(i);
          if ( driver->sof ) driver->sof(event.rhport);
        }
      break;

      case USBD_EVENT_FUNC_CALL:
        TU_LOG2("\r\n");
        if ( event.func_call.func ) event.func_call.func(event.func_call.param);
      break;

      default:
        TU_BREAKPOINT();
      break;
    }
  }
}

//--------------------------------------------------------------------+
// Control Request Parser & Handling
//--------------------------------------------------------------------+

// Helper to invoke class driver control request handler
static bool invoke_class_control(uint8_t rhport, usbd_class_driver_t const * driver, tusb_control_request_t const * request)
{
  usbd_control_set_complete_callback(driver->control_xfer_cb);
  TU_LOG2("  %s control request\r\n", driver->name);
  return driver->control_xfer_cb(rhport, CONTROL_STAGE_SETUP, request);
}

// This handles the actual request and its response.
// return false will cause its caller to stall control endpoint
static bool process_control_request(uint8_t rhport, tusb_control_request_t const * p_request)
{
  usbd_control_set_complete_callback(NULL);

  TU_ASSERT(p_request->bmRequestType_bit.type < TUSB_REQ_TYPE_INVALID);

  // Vendor request
  if ( p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR )
  {
    TU_VERIFY(tud_vendor_control_xfer_cb);

    usbd_control_set_complete_callback(tud_vendor_control_xfer_cb);
    return tud_vendor_control_xfer_cb(rhport, CONTROL_STAGE_SETUP, p_request);
  }

#if CFG_TUSB_DEBUG >= 2
  if (TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type && p_request->bRequest <= TUSB_REQ_SYNCH_FRAME)
  {
    TU_LOG2("  %s", _tusb_std_request_str[p_request->bRequest]);
    if (TUSB_REQ_GET_DESCRIPTOR != p_request->bRequest) TU_LOG2("\r\n");
  }
#endif

  switch ( p_request->bmRequestType_bit.recipient )
  {
    //------------- Device Requests e.g in enumeration -------------//
    case TUSB_REQ_RCPT_DEVICE:
      if ( TUSB_REQ_TYPE_CLASS == p_request->bmRequestType_bit.type )
      {
        uint8_t const itf = tu_u16_low(p_request->wIndex);
        TU_VERIFY(itf < TU_ARRAY_SIZE(_usbd_dev.itf2drv));

        usbd_class_driver_t const * driver = get_driver(_usbd_dev.itf2drv[itf]);
        TU_VERIFY(driver);

        // forward to class driver: "non-STD request to Interface"
        return invoke_class_control(rhport, driver, p_request);
      }

      if ( TUSB_REQ_TYPE_STANDARD != p_request->bmRequestType_bit.type )
      {
        // Non standard request is not supported
        TU_BREAKPOINT();
        return false;
      }

      switch ( p_request->bRequest )
      {
        case TUSB_REQ_SET_ADDRESS:
          // Depending on mcu, status phase could be sent either before or after changing device address,
          // or even require stack to not response with status at all
          // Therefore DCD must take full responsibility to response and include zlp status packet if needed.
          usbd_control_set_request(p_request); // set request since DCD has no access to tud_control_status() API
          dcd_set_address(rhport, (uint8_t) p_request->wValue);
          // skip tud_control_status()
          _usbd_dev.addressed = 1;
        break;

        case TUSB_REQ_GET_CONFIGURATION:
        {
          uint8_t cfg_num = _usbd_dev.cfg_num;
          tud_control_xfer(rhport, p_request, &cfg_num, 1);
        }
        break;

        case TUSB_REQ_SET_CONFIGURATION:
        {
          uint8_t const cfg_num = (uint8_t) p_request->wValue;

          if ( !_usbd_dev.cfg_num && cfg_num ) TU_ASSERT( process_set_config(rhport, cfg_num) );
          _usbd_dev.cfg_num = cfg_num;

          tud_control_status(rhport, p_request);
        }
        break;

        case TUSB_REQ_GET_DESCRIPTOR:
          TU_VERIFY( process_get_descriptor(rhport, p_request) );
        break;

        case TUSB_REQ_SET_FEATURE:
          // Only support remote wakeup for device feature
          TU_VERIFY(TUSB_REQ_FEATURE_REMOTE_WAKEUP == p_request->wValue);

          // Host may enable remote wake up before suspending especially HID device
          _usbd_dev.remote_wakeup_en = true;
          tud_control_status(rhport, p_request);
        break;

        case TUSB_REQ_CLEAR_FEATURE:
          // Only support remote wakeup for device feature
          TU_VERIFY(TUSB_REQ_FEATURE_REMOTE_WAKEUP == p_request->wValue);

          // Host may disable remote wake up after resuming
          _usbd_dev.remote_wakeup_en = false;
          tud_control_status(rhport, p_request);
        break;

        case TUSB_REQ_GET_STATUS:
        {
          // Device status bit mask
          // - Bit 0: Self Powered
          // - Bit 1: Remote Wakeup enabled
          uint16_t status = (_usbd_dev.self_powered ? 1 : 0) | (_usbd_dev.remote_wakeup_en ? 2 : 0);
          tud_control_xfer(rhport, p_request, &status, 2);
        }
        break;

        // Unknown/Unsupported request
        default: TU_BREAKPOINT(); return false;
      }
    break;

    //------------- Class/Interface Specific Request -------------//
    case TUSB_REQ_RCPT_INTERFACE:
    {
      uint8_t const itf = tu_u16_low(p_request->wIndex);
      TU_VERIFY(itf < TU_ARRAY_SIZE(_usbd_dev.itf2drv));

      usbd_class_driver_t const * driver = get_driver(_usbd_dev.itf2drv[itf]);
      TU_VERIFY(driver);

      // all requests to Interface (STD or Class) is forwarded to class driver.
      // notable requests are: GET HID REPORT DESCRIPTOR, SET_INTERFACE, GET_INTERFACE
      if ( !invoke_class_control(rhport, driver, p_request) )
      {
        // For GET_INTERFACE and SET_INTERFACE, it is mandatory to respond even if the class
        // driver doesn't use alternate settings or implement this
        TU_VERIFY(TUSB_REQ_TYPE_STANDARD == p_request->bmRequestType_bit.type);

        if (TUSB_REQ_GET_INTERFACE == p_request->bRequest)
        {
          uint8_t alternate = 0;
          tud_control_xfer(rhport, p_request, &alternate, 1);
        }else if (TUSB_REQ_SET_INTERFACE == p_request->bRequest)
        {
          tud_control_status(rhport, p_request);
        } else
        {
          return false;
        }
      }
    }
    break;

    //------------- Endpoint Request -------------//
    case TUSB_REQ_RCPT_ENDPOINT:
    {
      uint8_t const ep_addr = tu_u16_low(p_request->wIndex);
      uint8_t const ep_num  = tu_edpt_number(ep_addr);
      uint8_t const ep_dir  = tu_edpt_dir(ep_addr);

      TU_ASSERT(ep_num < TU_ARRAY_SIZE(_usbd_dev.ep2drv) );

      usbd_class_driver_t const * driver = get_driver(_usbd_dev.ep2drv[ep_num][ep_dir]);

      if ( TUSB_REQ_TYPE_STANDARD != p_request->bmRequestType_bit.type )
      {
        // Forward class request to its driver
        TU_VERIFY(driver);
        return invoke_class_control(rhport, driver, p_request);
      }
      else
      {
        // Handle STD request to endpoint
        switch ( p_request->bRequest )
        {
          case TUSB_REQ_GET_STATUS:
          {
            uint16_t status = usbd_edpt_stalled(rhport, ep_addr) ? 0x0001 : 0x0000;
            tud_control_xfer(rhport, p_request, &status, 2);
          }
          break;

          case TUSB_REQ_CLEAR_FEATURE:
          case TUSB_REQ_SET_FEATURE:
          {
            if ( TUSB_REQ_FEATURE_EDPT_HALT == p_request->wValue )
            {
              if ( TUSB_REQ_CLEAR_FEATURE ==  p_request->bRequest )
              {
                usbd_edpt_clear_stall(rhport, ep_addr);
              }else
              {
                usbd_edpt_stall(rhport, ep_addr);
              }
            }

            if (driver)
            {
              // Some classes such as USBTMC needs to clear/re-init its buffer when receiving CLEAR_FEATURE request
              // We will also forward std request targeted endpoint to class drivers as well

              // STD request must always be ACKed regardless of driver returned value
              // Also clear complete callback if driver set since it can also stall the request.
              (void) invoke_class_control(rhport, driver, p_request);
              usbd_control_set_complete_callback(NULL);

              // skip ZLP status if driver already did that
              if ( !_usbd_dev.ep_status[0][TUSB_DIR_IN].busy ) tud_control_status(rhport, p_request);
            }
          }
          break;

          // Unknown/Unsupported request
          default: TU_BREAKPOINT(); return false;
        }
      }
    }
    break;

    // Unknown recipient
    default: TU_BREAKPOINT(); return false;
  }

  return true;
}

// Process Set Configure Request
// This function parse configuration descriptor & open drivers accordingly
static bool process_set_config(uint8_t rhport, uint8_t cfg_num)
{
  tusb_desc_configuration_t const * desc_cfg = (tusb_desc_configuration_t const *) tud_descriptor_configuration_cb(cfg_num-1); // index is cfg_num-1
  TU_ASSERT(desc_cfg != NULL && desc_cfg->bDescriptorType == TUSB_DESC_CONFIGURATION);

  // Parse configuration descriptor
  _usbd_dev.remote_wakeup_support = (desc_cfg->bmAttributes & TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP) ? 1 : 0;
  _usbd_dev.self_powered = (desc_cfg->bmAttributes & TUSB_DESC_CONFIG_ATT_SELF_POWERED) ? 1 : 0;

  // Parse interface descriptor
  uint8_t const * p_desc   = ((uint8_t const*) desc_cfg) + sizeof(tusb_desc_configuration_t);
  uint8_t const * desc_end = ((uint8_t const*) desc_cfg) + desc_cfg->wTotalLength;

  while( p_desc < desc_end )
  {
    tusb_desc_interface_assoc_t const * desc_itf_assoc = NULL;

    // Class will always starts with Interface Association (if any) and then Interface descriptor
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc) )
    {
      desc_itf_assoc = (tusb_desc_interface_assoc_t const *) p_desc;
      p_desc = tu_desc_next(p_desc); // next to Interface
    }

    TU_ASSERT( TUSB_DESC_INTERFACE == tu_desc_type(p_desc) );

    tusb_desc_interface_t const * desc_itf = (tusb_desc_interface_t const*) p_desc;
    uint16_t const remaining_len = desc_end-p_desc;

    uint8_t drv_id;
    for (drv_id = 0; drv_id < TOTAL_DRIVER_COUNT; drv_id++)
    {
      usbd_class_driver_t const *driver = get_driver(drv_id);
      uint16_t const drv_len = driver->open(rhport, desc_itf, remaining_len);

      if ( drv_len > 0 )
      {
        // Open successfully, check if length is correct
        TU_ASSERT( sizeof(tusb_desc_interface_t) <= drv_len && drv_len <= remaining_len);

        // Interface number must not be used already
        TU_ASSERT(DRVID_INVALID == _usbd_dev.itf2drv[desc_itf->bInterfaceNumber]);

        TU_LOG2("  %s opened\r\n", driver->name);
        _usbd_dev.itf2drv[desc_itf->bInterfaceNumber] = drv_id;

        // If IAD exist, assign all interfaces to the same driver
        if (desc_itf_assoc)
        {
          // IAD's first interface number and class should match with opened interface
          TU_ASSERT(desc_itf_assoc->bFirstInterface == desc_itf->bInterfaceNumber &&
                    desc_itf_assoc->bFunctionClass  == desc_itf->bInterfaceClass);

          for(uint8_t i=1; i<desc_itf_assoc->bInterfaceCount; i++)
          {
            _usbd_dev.itf2drv[desc_itf->bInterfaceNumber+i] = drv_id;
          }
        }

        mark_interface_endpoint(_usbd_dev.ep2drv, p_desc, drv_len, drv_id); // TODO refactor

        p_desc += drv_len; // next interface

        break;
      }
    }

    // Failed if cannot find supported driver
    TU_ASSERT(drv_id < TOTAL_DRIVER_COUNT);
  }

  // invoke callback
  if (tud_mount_cb) tud_mount_cb();

  return true;
}

// Helper marking endpoint of interface belongs to class driver
static void mark_interface_endpoint(uint8_t ep2drv[][2], uint8_t const* p_desc, uint16_t desc_len, uint8_t driver_id)
{
  uint16_t len = 0;

  while( len < desc_len )
  {
    if ( TUSB_DESC_ENDPOINT == tu_desc_type(p_desc) )
    {
      uint8_t const ep_addr = ((tusb_desc_endpoint_t const*) p_desc)->bEndpointAddress;

      ep2drv[tu_edpt_number(ep_addr)][tu_edpt_dir(ep_addr)] = driver_id;
    }

    len   = (uint16_t)(len + tu_desc_len(p_desc));
    p_desc = tu_desc_next(p_desc);
  }
}

// return descriptor's buffer and update desc_len
static bool process_get_descriptor(uint8_t rhport, tusb_control_request_t const * p_request)
{
  tusb_desc_type_t const desc_type = (tusb_desc_type_t) tu_u16_high(p_request->wValue);
  uint8_t const desc_index = tu_u16_low( p_request->wValue );

  switch(desc_type)
  {
    case TUSB_DESC_DEVICE:
    {
      TU_LOG2(" Device\r\n");

      uint16_t len = sizeof(tusb_desc_device_t);

      // Only send up to EP0 Packet Size if not addressed
      // This only happens with the very first get device descriptor and EP0 size = 8 or 16.
      if ((CFG_TUD_ENDPOINT0_SIZE < sizeof(tusb_desc_device_t)) && !_usbd_dev.addressed)
      {
        len = CFG_TUD_ENDPOINT0_SIZE;

        // Hack here: we modify the request length to prevent usbd_control response with zlp
        ((tusb_control_request_t*) p_request)->wLength = CFG_TUD_ENDPOINT0_SIZE;
      }

      return tud_control_xfer(rhport, p_request, (void*) tud_descriptor_device_cb(), len);
    }
    break;

    case TUSB_DESC_BOS:
    {
      TU_LOG2(" BOS\r\n");

      // requested by host if USB > 2.0 ( i.e 2.1 or 3.x )
      if (!tud_descriptor_bos_cb) return false;

      tusb_desc_bos_t const* desc_bos = (tusb_desc_bos_t const*) tud_descriptor_bos_cb();

      uint16_t total_len;
      // Use offsetof to avoid pointer to the odd/misaligned address
      memcpy(&total_len, (uint8_t*) desc_bos + offsetof(tusb_desc_bos_t, wTotalLength), 2);

      return tud_control_xfer(rhport, p_request, (void*) desc_bos, total_len);
    }
    break;

    case TUSB_DESC_CONFIGURATION:
    {
      TU_LOG2(" Configuration[%u]\r\n", desc_index);

      tusb_desc_configuration_t const* desc_config = (tusb_desc_configuration_t const*) tud_descriptor_configuration_cb(desc_index);
      TU_ASSERT(desc_config);

      uint16_t total_len;
      // Use offsetof to avoid pointer to the odd/misaligned address
      memcpy(&total_len, (uint8_t*) desc_config + offsetof(tusb_desc_configuration_t, wTotalLength), 2);

      return tud_control_xfer(rhport, p_request, (void*) desc_config, total_len);
    }
    break;

    case TUSB_DESC_STRING:
    {
      TU_LOG2(" String[%u]\r\n", desc_index);

      // String Descriptor always uses the desc set from user
      uint8_t const* desc_str = (uint8_t const*) tud_descriptor_string_cb(desc_index, p_request->wIndex);
      TU_VERIFY(desc_str);

      // first byte of descriptor is its size
      return tud_control_xfer(rhport, p_request, (void*) desc_str, desc_str[0]);
    }
    break;

    case TUSB_DESC_DEVICE_QUALIFIER:
      TU_LOG2(" Device Qualifier\r\n");

      // Host sends this request to ask why our device with USB BCD from 2.0
      // but is running at Full/Low Speed. If not highspeed capable stall this request,
      // otherwise return the descriptor that could work in highspeed mode
      if ( tud_descriptor_device_qualifier_cb )
      {
        uint8_t const* desc_qualifier = tud_descriptor_device_qualifier_cb();
        TU_ASSERT(desc_qualifier);

        // first byte of descriptor is its size
        return tud_control_xfer(rhport, p_request, (void*) desc_qualifier, desc_qualifier[0]);
      }else
      {
        return false;
      }
    break;

    case TUSB_DESC_OTHER_SPEED_CONFIG:
      TU_LOG2(" Other Speed Configuration\r\n");

      // After Device Qualifier descriptor is received host will ask for this descriptor
      return false; // not supported
    break;

    default: return false;
  }
}

//--------------------------------------------------------------------+
// DCD Event Handler
//--------------------------------------------------------------------+
void dcd_event_handler(dcd_event_t const * event, bool in_isr)
{
  switch (event->event_id)
  {
    case DCD_EVENT_UNPLUGGED:
      // UNPLUGGED event can be bouncing, only processing if we are currently connected
      if ( _usbd_dev.connected )
      {
        _usbd_dev.connected  = 0;
        _usbd_dev.addressed  = 0;
        _usbd_dev.cfg_num    = 0;
        _usbd_dev.suspended  = 0;
        osal_queue_send(_usbd_q, event, in_isr);
      }
    break;

    case DCD_EVENT_SOF:
      return;   // skip SOF event for now
    break;

    case DCD_EVENT_SUSPEND:
      // NOTE: When plugging/unplugging device, the D+/D- state are unstable and
      // can accidentally meet the SUSPEND condition ( Bus Idle for 3ms ).
      // In addition, some MCUs such as SAMD or boards that haven no VBUS detection cannot distinguish
      // suspended vs disconnected. We will skip handling SUSPEND/RESUME event if not currently connected
      if ( _usbd_dev.connected )
      {
        _usbd_dev.suspended = 1;
        osal_queue_send(_usbd_q, event, in_isr);
      }
    break;

    case DCD_EVENT_RESUME:
      // skip event if not connected (especially required for SAMD)
      if ( _usbd_dev.connected )
      {
        _usbd_dev.suspended = 0;
        osal_queue_send(_usbd_q, event, in_isr);
      }
    break;

    default:
      osal_queue_send(_usbd_q, event, in_isr);
    break;
  }
}

void dcd_event_bus_signal (uint8_t rhport, dcd_eventid_t eid, bool in_isr)
{
  dcd_event_t event = { .rhport = rhport, .event_id = eid };
  dcd_event_handler(&event, in_isr);
}

void dcd_event_bus_reset (uint8_t rhport, tusb_speed_t speed, bool in_isr)
{
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_BUS_RESET };
  event.bus_reset.speed = speed;
  dcd_event_handler(&event, in_isr);
}

void dcd_event_setup_received(uint8_t rhport, uint8_t const * setup, bool in_isr)
{
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_SETUP_RECEIVED };
  memcpy(&event.setup_received, setup, 8);

  dcd_event_handler(&event, in_isr);
}

void dcd_event_xfer_complete (uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, uint8_t result, bool in_isr)
{
  dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_XFER_COMPLETE };

  event.xfer_complete.ep_addr = ep_addr;
  event.xfer_complete.len     = xferred_bytes;
  event.xfer_complete.result  = result;

  dcd_event_handler(&event, in_isr);
}

//--------------------------------------------------------------------+
// USBD API For Class Driver
//--------------------------------------------------------------------+

// Parse consecutive endpoint descriptors (IN & OUT)
bool usbd_open_edpt_pair(uint8_t rhport, uint8_t const* p_desc, uint8_t ep_count, uint8_t xfer_type, uint8_t* ep_out, uint8_t* ep_in)
{
  for(int i=0; i<ep_count; i++)
  {
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType && xfer_type == desc_ep->bmAttributes.xfer);
    TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

    if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN )
    {
      (*ep_in) = desc_ep->bEndpointAddress;
    }else
    {
      (*ep_out) = desc_ep->bEndpointAddress;
    }

    p_desc = tu_desc_next(p_desc);
  }

  return true;
}

// Helper to defer an isr function
void usbd_defer_func(osal_task_func_t func, void* param, bool in_isr)
{
  dcd_event_t event =
  {
      .rhport   = 0,
      .event_id = USBD_EVENT_FUNC_CALL,
  };

  event.func_call.func  = func;
  event.func_call.param = param;

  dcd_event_handler(&event, in_isr);
}

//--------------------------------------------------------------------+
// USBD Endpoint API
//--------------------------------------------------------------------+

bool usbd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * desc_ep)
{
  TU_LOG2("  Open EP %02X with Size = %u\r\n", desc_ep->bEndpointAddress, desc_ep->wMaxPacketSize.size);

  switch (desc_ep->bmAttributes.xfer)
  {
    case TUSB_XFER_ISOCHRONOUS:
    {
      uint16_t const max_epsize = (_usbd_dev.speed == TUSB_SPEED_HIGH ? 1024 : 1023);
      TU_ASSERT(desc_ep->wMaxPacketSize.size <= max_epsize);
    }
    break;

    case TUSB_XFER_BULK:
      if (_usbd_dev.speed == TUSB_SPEED_HIGH)
      {
        // Bulk highspeed must be EXACTLY 512
        TU_ASSERT(desc_ep->wMaxPacketSize.size == 512);
      }else
      {
        // TODO Bulk fullspeed can only be 8, 16, 32, 64
        TU_ASSERT(desc_ep->wMaxPacketSize.size <= 64);
      }
    break;

    case TUSB_XFER_INTERRUPT:
    {
      uint16_t const max_epsize = (_usbd_dev.speed == TUSB_SPEED_HIGH ? 1024 : 64);
      TU_ASSERT(desc_ep->wMaxPacketSize.size <= max_epsize);
    }
    break;

    default: return false;
  }

  return dcd_edpt_open(rhport, desc_ep);
}

bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

#if CFG_TUSB_OS != OPT_OS_NONE
  // pre-check to help reducing mutex lock
  TU_VERIFY((_usbd_dev.ep_status[epnum][dir].busy == 0) && (_usbd_dev.ep_status[epnum][dir].claimed == 0));
  osal_mutex_lock(_usbd_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
#endif

  // can only claim the endpoint if it is not busy and not claimed yet.
  bool const ret = (_usbd_dev.ep_status[epnum][dir].busy == 0) && (_usbd_dev.ep_status[epnum][dir].claimed == 0);
  if (ret)
  {
    _usbd_dev.ep_status[epnum][dir].claimed = 1;
  }

#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_unlock(_usbd_mutex);
#endif

  return ret;
}

bool usbd_edpt_release(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_lock(_usbd_mutex, OSAL_TIMEOUT_WAIT_FOREVER);
#endif

  // can only release the endpoint if it is claimed and not busy
  bool const ret = (_usbd_dev.ep_status[epnum][dir].busy == 0) && (_usbd_dev.ep_status[epnum][dir].claimed == 1);
  if (ret)
  {
    _usbd_dev.ep_status[epnum][dir].claimed = 0;
  }

#if CFG_TUSB_OS != OPT_OS_NONE
  osal_mutex_unlock(_usbd_mutex);
#endif

  return ret;
}

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  TU_LOG2("  Queue EP %02X with %u bytes ... ", ep_addr, total_bytes);

  // Attempt to transfer on a busy endpoint, sound like an race condition !
  TU_ASSERT(_usbd_dev.ep_status[epnum][dir].busy == 0);

  // Set busy first since the actual transfer can be complete before dcd_edpt_xfer()
  // could return and USBD task can preempt and clear the busy
  _usbd_dev.ep_status[epnum][dir].busy = true;

  if ( dcd_edpt_xfer(rhport, ep_addr, buffer, total_bytes) )
  {
    TU_LOG2("OK\r\n");
    return true;
  }else
  {
    // DCD error, mark endpoint as ready to allow next transfer
    _usbd_dev.ep_status[epnum][dir].busy = false;
    _usbd_dev.ep_status[epnum][dir].claimed = 0;
    TU_LOG2("failed\r\n");
    TU_BREAKPOINT();
    return false;
  }
}

// The number of bytes has to be given explicitly to allow more flexible control of how many
// bytes should be written and second to keep the return value free to give back a boolean
// success message. If total_bytes is too big, the FIFO will copy only what is available
// into the USB buffer!
bool usbd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  TU_LOG2("  Queue ISO EP %02X with %u bytes ... ", ep_addr, total_bytes);

  // Attempt to transfer on a busy endpoint, sound like an race condition !
  TU_ASSERT(_usbd_dev.ep_status[epnum][dir].busy == 0);

  // Set busy first since the actual transfer can be complete before dcd_edpt_xfer() could return
  // and usbd task can preempt and clear the busy
  _usbd_dev.ep_status[epnum][dir].busy = true;

  if (dcd_edpt_xfer_fifo(rhport, ep_addr, ff, total_bytes))
  {
    TU_LOG2("OK\r\n");
    return true;
  }else
  {
    // DCD error, mark endpoint as ready to allow next transfer
    _usbd_dev.ep_status[epnum][dir].busy = false;
    _usbd_dev.ep_status[epnum][dir].claimed = 0;
    TU_LOG2("failed\r\n");
    TU_BREAKPOINT();
    return false;
  }
}

bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  return _usbd_dev.ep_status[epnum][dir].busy;
}

void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  dcd_edpt_stall(rhport, ep_addr);
  _usbd_dev.ep_status[epnum][dir].stalled = true;
  _usbd_dev.ep_status[epnum][dir].busy = true;
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  dcd_edpt_clear_stall(rhport, ep_addr);
  _usbd_dev.ep_status[epnum][dir].stalled = false;
  _usbd_dev.ep_status[epnum][dir].busy = false;
}

bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  return _usbd_dev.ep_status[epnum][dir].stalled;
}

/**
 * usbd_edpt_close will disable an endpoint.
 *
 * In progress transfers on this EP may be delivered after this call.
 *
 */
void usbd_edpt_close(uint8_t rhport, uint8_t ep_addr)
{
  TU_ASSERT(dcd_edpt_close, /**/);
  TU_LOG2("  CLOSING Endpoint: 0x%02X\r\n", ep_addr);

  dcd_edpt_close(rhport, ep_addr);

  return;
}

#endif
