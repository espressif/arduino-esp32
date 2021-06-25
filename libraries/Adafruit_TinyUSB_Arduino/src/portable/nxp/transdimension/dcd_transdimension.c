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

#if TUSB_OPT_DEVICE_ENABLED && \
    (CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#if CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  #include "fsl_device_registers.h"
  #define INCLUDE_FSL_DEVICE_REGISTERS
#else
  // LPCOpen for 18xx & 43xx
  #include "chip.h"
#endif

#include "common/tusb_common.h"
#include "device/dcd.h"
#include "common_transdimension.h"

#if defined(__CORTEX_M) && __CORTEX_M == 7 && __DCACHE_PRESENT == 1
  #define CleanInvalidateDCache_by_Addr   SCB_CleanInvalidateDCache_by_Addr
#else
  #define CleanInvalidateDCache_by_Addr(_addr, _dsize)
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// ENDPTCTRL
enum {
  ENDPTCTRL_STALL          = TU_BIT(0),
  ENDPTCTRL_TOGGLE_INHIBIT = TU_BIT(5), ///< used for test only
  ENDPTCTRL_TOGGLE_RESET   = TU_BIT(6),
  ENDPTCTRL_ENABLE         = TU_BIT(7)
};

// USBSTS, USBINTR
enum {
  INTR_USB         = TU_BIT(0),
  INTR_ERROR       = TU_BIT(1),
  INTR_PORT_CHANGE = TU_BIT(2),
  INTR_RESET       = TU_BIT(6),
  INTR_SOF         = TU_BIT(7),
  INTR_SUSPEND     = TU_BIT(8),
  INTR_NAK         = TU_BIT(16)
};

// Queue Transfer Descriptor
typedef struct
{
  // Word 0: Next QTD Pointer
  uint32_t next; ///< Next link pointer This field contains the physical memory address of the next dTD to be processed

  // Word 1: qTQ Token
  uint32_t                      : 3  ;
  volatile uint32_t xact_err    : 1  ;
  uint32_t                      : 1  ;
  volatile uint32_t buffer_err  : 1  ;
  volatile uint32_t halted      : 1  ;
  volatile uint32_t active      : 1  ;
  uint32_t                      : 2  ;
  uint32_t iso_mult_override    : 2  ; ///< This field can be used for transmit ISOs to override the MULT field in the dQH. This field must be zero for all packet types that are not transmit-ISO.
  uint32_t                      : 3  ;
  uint32_t int_on_complete      : 1  ;
  volatile uint32_t total_bytes : 15 ;
  uint32_t                      : 0  ;

  // Word 2-6: Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the start of a 4K page
  uint32_t buffer[5]; ///< buffer1 has frame_n for TODO Isochronous

  //------------- DCD Area -------------//
  uint16_t expected_bytes;
  uint8_t reserved[2];
} dcd_qtd_t;

TU_VERIFY_STATIC( sizeof(dcd_qtd_t) == 32, "size is not correct");

// Queue Head
typedef struct
{
  // Word 0: Capabilities and Characteristics
  uint32_t                         : 15 ; ///< Number of packets executed per transaction descriptor 00 - Execute N transactions as demonstrated by the USB variable length protocol where N is computed using Max_packet_length and the Total_bytes field in the dTD. 01 - Execute one transaction 10 - Execute two transactions 11 - Execute three transactions Remark: Non-isochronous endpoints must set MULT = 00. Remark: Isochronous endpoints must set MULT = 01, 10, or 11 as needed.
  uint32_t int_on_setup            : 1  ; ///< Interrupt on setup This bit is used on control type endpoints to indicate if USBINT is set in response to a setup being received.
  uint32_t max_package_size        : 11 ; ///< This directly corresponds to the maximum packet size of the associated endpoint (wMaxPacketSize)
  uint32_t                         : 2  ;
  uint32_t zero_length_termination : 1  ; ///< This bit is used for non-isochronous endpoints to indicate when a zero-length packet is received to terminate transfers in case the total transfer length is “multiple”. 0 - Enable zero-length packet to terminate transfers equal to a multiple of Max_packet_length (default). 1 - Disable zero-length packet on transfers that are equal in length to a multiple Max_packet_length.
  uint32_t iso_mult                : 2  ; ///<
  uint32_t                         : 0  ;

  // Word 1: Current qTD Pointer
  volatile uint32_t qtd_addr;

  // Word 2-9: Transfer Overlay
  volatile dcd_qtd_t qtd_overlay;

  // Word 10-11: Setup request (control OUT only)
  volatile tusb_control_request_t setup_request;

  //--------------------------------------------------------------------+
  /// Due to the fact QHD is 64 bytes aligned but occupies only 48 bytes
  /// thus there are 16 bytes padding free that we can make use of.
  //--------------------------------------------------------------------+
  uint8_t reserved[16];
} dcd_qhd_t;

TU_VERIFY_STATIC( sizeof(dcd_qhd_t) == 64, "size is not correct");

//--------------------------------------------------------------------+
// Variables
//--------------------------------------------------------------------+

typedef struct
{
  dcd_registers_t* regs;  // registers
  const IRQn_Type irqnum; // IRQ number
  const uint8_t ep_count; // Max bi-directional Endpoints
}dcd_controller_t;

#if CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  // Each endpoint with direction (IN/OUT) occupies a queue head
  // Therefore QHD_MAX is 2 x max endpoint count
  #define QHD_MAX  (8*2)

  static const dcd_controller_t _dcd_controller[] =
  {
    // RT1010 and RT1020 only has 1 USB controller
    #if FSL_FEATURE_SOC_USBHS_COUNT == 1
      { .regs = (dcd_registers_t*) USB_BASE , .irqnum = USB_OTG1_IRQn, .ep_count = 8 }
    #else
      { .regs = (dcd_registers_t*) USB1_BASE, .irqnum = USB_OTG1_IRQn, .ep_count = 8 },
      { .regs = (dcd_registers_t*) USB2_BASE, .irqnum = USB_OTG2_IRQn, .ep_count = 8 }
    #endif
  };

#else
  #define QHD_MAX (6*2)

  static const dcd_controller_t _dcd_controller[] =
  {
    { .regs = (dcd_registers_t*) LPC_USB0_BASE, .irqnum = USB0_IRQn, .ep_count = 6 },
    { .regs = (dcd_registers_t*) LPC_USB1_BASE, .irqnum = USB1_IRQn, .ep_count = 4 }
  };
#endif

#define QTD_NEXT_INVALID 0x01

typedef struct {
  // Must be at 2K alignment
  dcd_qhd_t qhd[QHD_MAX] TU_ATTR_ALIGNED(64);
  dcd_qtd_t qtd[QHD_MAX] TU_ATTR_ALIGNED(32); // for portability, TinyUSB only queue 1 TD for each Qhd
}dcd_data_t;

CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(2048)
static dcd_data_t _dcd_data;

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

/// follows LPC43xx User Manual 23.10.3
static void bus_reset(uint8_t rhport)
{
  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  // direction is enabled and the paired endpoint of opposite direction is disabled, then the
  // endpoint type of the unused direction must be changed from the control type to any other
  // type (e.g. bulk). Leaving an un-configured endpoint control will cause undefined behavior
  // for the data PID tracking on the active endpoint.
  for( int i=1; i < _dcd_controller[rhport].ep_count; i++)
  {
    dcd_reg->ENDPTCTRL[i] = (TUSB_XFER_BULK << 2) | (TUSB_XFER_BULK << 18);
  }

  //------------- Clear All Registers -------------//
  dcd_reg->ENDPTNAK       = dcd_reg->ENDPTNAK;
  dcd_reg->ENDPTNAKEN     = 0;
  dcd_reg->USBSTS         = dcd_reg->USBSTS;
  dcd_reg->ENDPTSETUPSTAT = dcd_reg->ENDPTSETUPSTAT;
  dcd_reg->ENDPTCOMPLETE  = dcd_reg->ENDPTCOMPLETE;

  while (dcd_reg->ENDPTPRIME) {}
  dcd_reg->ENDPTFLUSH = 0xFFFFFFFF;
  while (dcd_reg->ENDPTFLUSH) {}

  // read reset bit in portsc

  //------------- Queue Head & Queue TD -------------//
  tu_memclr(&_dcd_data, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
  _dcd_data.qhd[0].zero_length_termination = _dcd_data.qhd[1].zero_length_termination = 1;
  _dcd_data.qhd[0].max_package_size = _dcd_data.qhd[1].max_package_size = CFG_TUD_ENDPOINT0_SIZE;
  _dcd_data.qhd[0].qtd_overlay.next = _dcd_data.qhd[1].qtd_overlay.next = QTD_NEXT_INVALID;

  _dcd_data.qhd[0].int_on_setup = 1; // OUT only
}

void dcd_init(uint8_t rhport)
{
  tu_memclr(&_dcd_data, sizeof(dcd_data_t));

  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;

  // Reset controller
  dcd_reg->USBCMD |= USBCMD_RESET;
  while( dcd_reg->USBCMD & USBCMD_RESET ) {}

  // Set mode to device, must be set immediately after reset
  dcd_reg->USBMODE = USBMODE_CM_DEVICE;
  dcd_reg->OTGSC = OTGSC_VBUS_DISCHARGE | OTGSC_OTG_TERMINATION;

  // TODO Force fullspeed on non-highspeed port
  // dcd_reg->PORTSC1 = PORTSC1_FORCE_FULL_SPEED;

  CleanInvalidateDCache_by_Addr((uint32_t*) &_dcd_data, sizeof(dcd_data_t));

  dcd_reg->ENDPTLISTADDR = (uint32_t) _dcd_data.qhd; // Endpoint List Address has to be 2K alignment
  dcd_reg->USBSTS  = dcd_reg->USBSTS;
  dcd_reg->USBINTR = INTR_USB | INTR_ERROR | INTR_PORT_CHANGE | INTR_RESET | INTR_SUSPEND /*| INTR_SOF*/;

  dcd_reg->USBCMD &= ~0x00FF0000;     // Interrupt Threshold Interval = 0
  dcd_reg->USBCMD |= USBCMD_RUN_STOP; // Connect
}

void dcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(_dcd_controller[rhport].irqnum);
}

void dcd_int_disable(uint8_t rhport)
{
  NVIC_DisableIRQ(_dcd_controller[rhport].irqnum);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  // Response with status first before changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);

  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;
  dcd_reg->DEVICEADDR = (dev_addr << 25) | TU_BIT(24);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

void dcd_connect(uint8_t rhport)
{
  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;
  dcd_reg->USBCMD |= USBCMD_RUN_STOP;
}

void dcd_disconnect(uint8_t rhport)
{
  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;
  dcd_reg->USBCMD &= ~USBCMD_RUN_STOP;
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
// index to bit position in register
static inline uint8_t ep_idx2bit(uint8_t ep_idx)
{
  return ep_idx/2 + ( (ep_idx%2) ? 16 : 0);
}

static void qtd_init(dcd_qtd_t* p_qtd, void * data_ptr, uint16_t total_bytes)
{
  tu_memclr(p_qtd, sizeof(dcd_qtd_t));

  p_qtd->next        = QTD_NEXT_INVALID;
  p_qtd->active      = 1;
  p_qtd->total_bytes = p_qtd->expected_bytes = total_bytes;

  if (data_ptr != NULL)
  {
    p_qtd->buffer[0]   = (uint32_t) data_ptr;
    for(uint8_t i=1; i<5; i++)
    {
      p_qtd->buffer[i] |= tu_align4k( p_qtd->buffer[i-1] ) + 4096;
    }
  }
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum  = tu_edpt_number(ep_addr);
  uint8_t const dir    = tu_edpt_dir(ep_addr);

  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;
  dcd_reg->ENDPTCTRL[epnum] |= ENDPTCTRL_STALL << (dir ? 16 : 0);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  uint8_t const epnum  = tu_edpt_number(ep_addr);
  uint8_t const dir    = tu_edpt_dir(ep_addr);

  // data toggle also need to be reset
  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;
  dcd_reg->ENDPTCTRL[epnum] |= ENDPTCTRL_TOGGLE_RESET << ( dir ? 16 : 0 );
  dcd_reg->ENDPTCTRL[epnum] &= ~(ENDPTCTRL_STALL << ( dir  ? 16 : 0));
}

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  // TODO not support ISO yet
  TU_VERIFY ( p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  uint8_t const epnum  = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const dir    = tu_edpt_dir(p_endpoint_desc->bEndpointAddress);
  uint8_t const ep_idx = 2*epnum + dir;

  // Must not exceed max endpoint number
  TU_ASSERT( epnum < _dcd_controller[rhport].ep_count );

  //------------- Prepare Queue Head -------------//
  dcd_qhd_t * p_qhd = &_dcd_data.qhd[ep_idx];
  tu_memclr(p_qhd, sizeof(dcd_qhd_t));

  p_qhd->zero_length_termination = 1;
  p_qhd->max_package_size        = p_endpoint_desc->wMaxPacketSize.size;
  p_qhd->qtd_overlay.next        = QTD_NEXT_INVALID;

  CleanInvalidateDCache_by_Addr((uint32_t*) &_dcd_data, sizeof(dcd_data_t));

  // Enable EP Control
  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;
  dcd_reg->ENDPTCTRL[epnum] |= ((p_endpoint_desc->bmAttributes.xfer << 2) | ENDPTCTRL_ENABLE | ENDPTCTRL_TOGGLE_RESET) << (dir ? 16 : 0);

  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  dcd_registers_t* dcd_reg = _dcd_controller[rhport].regs;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);
  uint8_t const ep_idx = 2*epnum + dir;

  if ( epnum == 0 )
  {
    // follows UM 24.10.8.1.1 Setup packet handling using setup lockout mechanism
    // wait until ENDPTSETUPSTAT before priming data/status in response TODO add time out
    while(dcd_reg->ENDPTSETUPSTAT & TU_BIT(0)) {}
  }

  dcd_qhd_t * p_qhd = &_dcd_data.qhd[ep_idx];
  dcd_qtd_t * p_qtd = &_dcd_data.qtd[ep_idx];

  // Force the CPU to flush the buffer. We increase the size by 32 because the call aligns the
  // address to 32-byte boundaries.
  // void* cast to suppress cast-align warning, buffer must be
  CleanInvalidateDCache_by_Addr((uint32_t*) tu_align((uint32_t) buffer, 4), total_bytes + 31);

  //------------- Prepare qtd -------------//
  qtd_init(p_qtd, buffer, total_bytes);
  p_qtd->int_on_complete = true;
  p_qhd->qtd_overlay.next = (uint32_t) p_qtd; // link qtd to qhd

  CleanInvalidateDCache_by_Addr((uint32_t*) &_dcd_data, sizeof(dcd_data_t));

  // start transfer
  dcd_reg->ENDPTPRIME = TU_BIT( ep_idx2bit(ep_idx) ) ;

  return true;
}

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+
void dcd_int_handler(uint8_t rhport)
{
  dcd_registers_t* const dcd_reg = _dcd_controller[rhport].regs;

  uint32_t const int_enable = dcd_reg->USBINTR;
  uint32_t const int_status = dcd_reg->USBSTS & int_enable;
  dcd_reg->USBSTS = int_status; // Acknowledge handled interrupt

  // disabled interrupt sources
  if (int_status == 0) return;

  if (int_status & INTR_RESET)
  {
    bus_reset(rhport);
    uint32_t speed = (dcd_reg->PORTSC1 & PORTSC1_PORT_SPEED) >> PORTSC1_PORT_SPEED_POS;
    dcd_event_bus_reset(rhport, (tusb_speed_t) speed, true);
  }

  if (int_status & INTR_SUSPEND)
  {
    if (dcd_reg->PORTSC1 & PORTSC1_SUSPEND)
    {
      // Note: Host may delay more than 3 ms before and/or after bus reset before doing enumeration.
      if ((dcd_reg->DEVICEADDR >> 25) & 0x0f)
      {
        dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
      }
    }
  }

  // Make sure we read the latest version of _dcd_data.
  CleanInvalidateDCache_by_Addr((uint32_t*) &_dcd_data, sizeof(dcd_data_t));

  // TODO disconnection does not generate interrupt !!!!!!
//	if (int_status & INTR_PORT_CHANGE)
//	{
//	  if ( !(dcd_reg->PORTSC1 & PORTSC1_CURRENT_CONNECT_STATUS) )
//	  {
//      dcd_event_t event = { .rhport = rhport, .event_id = DCD_EVENT_UNPLUGGED };
//      dcd_event_handler(&event, true);
//	  }
//	}

  if (int_status & INTR_USB)
  {
    uint32_t const edpt_complete = dcd_reg->ENDPTCOMPLETE;
    dcd_reg->ENDPTCOMPLETE = edpt_complete; // acknowledge

    if (dcd_reg->ENDPTSETUPSTAT)
    {
      //------------- Set up Received -------------//
      // 23.10.10.2 Operational model for setup transfers
      dcd_reg->ENDPTSETUPSTAT = dcd_reg->ENDPTSETUPSTAT;// acknowledge

      dcd_event_setup_received(rhport, (uint8_t*) &_dcd_data.qhd[0].setup_request, true);
    }

    if ( edpt_complete )
    {
      for(uint8_t ep_idx = 0; ep_idx < QHD_MAX; ep_idx++)
      {
        if ( tu_bit_test(edpt_complete, ep_idx2bit(ep_idx)) )
        {
          // 23.10.12.3 Failed QTD also get ENDPTCOMPLETE set
          dcd_qtd_t * p_qtd = &_dcd_data.qtd[ep_idx];

          uint8_t result = p_qtd->halted  ? XFER_RESULT_STALLED :
              ( p_qtd->xact_err ||p_qtd->buffer_err ) ? XFER_RESULT_FAILED : XFER_RESULT_SUCCESS;

          uint8_t const ep_addr = (ep_idx/2) | ( (ep_idx & 0x01) ? TUSB_DIR_IN_MASK : 0 );
          dcd_event_xfer_complete(rhport, ep_addr, p_qtd->expected_bytes - p_qtd->total_bytes, result, true); // only number of bytes in the IOC qtd
        }
      }
    }
  }

  if (int_status & INTR_SOF)
  {
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }

  if (int_status & INTR_NAK) {}
  if (int_status & INTR_ERROR) TU_ASSERT(false, );
}

#endif
