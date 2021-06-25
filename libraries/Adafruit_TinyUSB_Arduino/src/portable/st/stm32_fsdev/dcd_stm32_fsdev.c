/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
 *
 * Portions:
 * Copyright (c) 2016 STMicroelectronics
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

/**********************************************
 * This driver has been tested with the following MCUs:
 *  - F070, F072, L053, F042F6
 *
 * It also should work with minimal changes for any ST MCU with an "USB A"/"PCD"/"HCD" peripheral. This
 *  covers:
 *
 * F04x, F072, F078, 070x6/B      1024 byte buffer
 * F102, F103                      512 byte buffer; no internal D+ pull-up (maybe many more changes?)
 * F302xB/C, F303xB/C, F373        512 byte buffer; no internal D+ pull-up
 * F302x6/8, F302xD/E2, F303xD/E  1024 byte buffer; no internal D+ pull-up
 * L0x2, L0x3                     1024 byte buffer
 * L1                              512 byte buffer
 * L4x2, L4x3                     1024 byte buffer
 *
 * To use this driver, you must:
 * - If you are using a device with crystal-less USB, set up the clock recovery system (CRS)
 * - Remap pins to be D+/D- on devices that they are shared (for example: F042Fx)
 *   - This is different to the normal "alternate function" GPIO interface, needs to go through SYSCFG->CFGRx register
 * - Enable USB clock; Perhaps use __HAL_RCC_USB_CLK_ENABLE();
 * - (Optionally configure GPIO HAL to tell it the USB driver is using the USB pins)
 * - call tusb_init();
 * - periodically call tusb_task();
 *
 * Assumptions of the driver:
 * - You are not using CAN (it must share the packet buffer)
 * - APB clock is >= 10 MHz
 * - On some boards, series resistors are required, but not on others.
 * - On some boards, D+ pull up resistor (1.5kohm) is required, but not on others.
 * - You don't have long-running interrupts; some USB packets must be quickly responded to.
 * - You have the ST CMSIS library linked into the project. HAL is not used.
 *
 * Current driver limitations (i.e., a list of features for you to add):
 * - STALL handled, but not tested.
 *   - Does it work? No clue.
 * - All EP BTABLE buffers are created based on max packet size of first EP opened with that address.
 * - No isochronous endpoints
 * - Endpoint index is the ID of the endpoint
 *   - This means that priority is given to endpoints with lower ID numbers
 *   - Code is mixing up EP IX with EP ID. Everywhere.
 * - Packet buffer memory is copied in the interrupt.
 *   - This is better for performance, but means interrupts are disabled for longer
 *   - DMA may be the best choice, but it could also be pushed to the USBD task.
 * - No double-buffering
 * - No DMA
 * - Minimal error handling
 *   - Perhaps error interrupts should be reported to the stack, or cause a device reset?
 * - Assumes a single USB peripheral; I think that no hardware has multiple so this is fine.
 * - Add a callback for enabling/disabling the D+ PU on devices without an internal PU.
 * - F3 models use three separate interrupts. I think we could only use the LP interrupt for
 *     everything?  However, the interrupts are configurable so the DisableInt and EnableInt
 *     below functions could be adjusting the wrong interrupts (if they had been reconfigured)
 * - LPM is not used correctly, or at all?
 *
 * USB documentation and Reference implementations
 * - STM32 Reference manuals
 * - STM32 USB Hardware Guidelines AN4879
 *
 * - STM32 HAL (much of this driver is based on this)
 * - libopencm3/lib/stm32/common/st_usbfs_core.c
 * - Keil USB Device http://www.keil.com/pack/doc/mw/USB/html/group__usbd.html
 *
 * - YouTube OpenTechLab 011; https://www.youtube.com/watch?v=4FOkJLp_PUw
 *
 * Advantages over HAL driver:
 * - Tiny (saves RAM, assumes a single USB peripheral)
 *
 * Notes:
 * - The buffer table is allocated as endpoints are opened. The allocation is only
 *   cleared when the device is reset. This may be bad if the USB device needs
 *   to be reconfigured.
 */

#include "tusb_option.h"

#if defined(STM32F102x6) || defined(STM32F102xB) || \
    defined(STM32F103x6) || defined(STM32F103xB) || \
    defined(STM32F103xE) || defined(STM32F103xG)
#define STM32F1_FSDEV
#endif

#if (TUSB_OPT_DEVICE_ENABLED) && ( \
      (CFG_TUSB_MCU == OPT_MCU_STM32F0                          ) || \
      (CFG_TUSB_MCU == OPT_MCU_STM32F1 && defined(STM32F1_FSDEV)) || \
      (CFG_TUSB_MCU == OPT_MCU_STM32F3                          ) || \
      (CFG_TUSB_MCU == OPT_MCU_STM32L0                          ) \
    )

// In order to reduce the dependance on HAL, we undefine this.
// Some definitions are copied to our private include file.
#undef USE_HAL_DRIVER

#include "device/dcd.h"
#include "portable/st/stm32_fsdev/dcd_stm32_fsdev_pvt_st.h"


/*****************************************************
 * Configuration
 *****************************************************/

// HW supports max of 8 bidirectional endpoints, but this can be reduced to save RAM
// (8u here would mean 8 IN and 8 OUT)
#ifndef MAX_EP_COUNT
#  define MAX_EP_COUNT 8U
#endif

// If sharing with CAN, one can set this to be non-zero to give CAN space where it wants it
// Both of these MUST be a multiple of 2, and are in byte units.
#ifndef DCD_STM32_BTABLE_BASE
#  define DCD_STM32_BTABLE_BASE 0U
#endif

#ifndef DCD_STM32_BTABLE_LENGTH
#  define DCD_STM32_BTABLE_LENGTH (PMA_LENGTH - DCD_STM32_BTABLE_BASE)
#endif

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#ifndef USE_SOF
#  define USE_SOF     0
#endif

/***************************************************
 * Checks, structs, defines, function definitions, etc.
 */

TU_VERIFY_STATIC((MAX_EP_COUNT) <= STFSDEV_EP_COUNT, "Only 8 endpoints supported on the hardware");

TU_VERIFY_STATIC(((DCD_STM32_BTABLE_BASE) + (DCD_STM32_BTABLE_LENGTH))<=(PMA_LENGTH),
    "BTABLE does not fit in PMA RAM");

TU_VERIFY_STATIC(((DCD_STM32_BTABLE_BASE) % 8) == 0, "BTABLE base must be aligned to 8 bytes");

// One of these for every EP IN & OUT, uses a bit of RAM....
typedef struct
{
  uint8_t * buffer;
  // tu_fifo_t * ff;  // TODO support dcd_edpt_xfer_fifo API
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t pma_ptr;
  uint8_t max_packet_size;
  uint8_t pma_alloc_size;
} xfer_ctl_t;

static xfer_ctl_t xfer_status[MAX_EP_COUNT][2];

static inline xfer_ctl_t* xfer_ctl_ptr(uint32_t epnum, uint32_t dir)
{
  return &xfer_status[epnum][dir];
}

static TU_ATTR_ALIGNED(4) uint32_t _setup_packet[6];

static uint8_t remoteWakeCountdown; // When wake is requested

// into the stack.
static void dcd_handle_bus_reset(void);
static void dcd_transmit_packet(xfer_ctl_t * xfer, uint16_t ep_ix);
static void dcd_ep_ctr_handler(void);

// PMA allocation/access 
static uint8_t open_ep_count;
static uint16_t ep_buf_ptr; ///< Points to first free memory location
static void dcd_pma_alloc_reset(void);
static uint16_t dcd_pma_alloc(uint8_t ep_addr, size_t length);
static void dcd_pma_free(uint8_t ep_addr);
static bool dcd_write_packet_memory(uint16_t dst, const void *__restrict src, size_t wNBytes);
static bool dcd_read_packet_memory(void *__restrict dst, uint16_t src, size_t wNBytes);

//static bool dcd_write_packet_memory_ff(tu_fifo_t * ff, uint16_t dst, uint16_t wNBytes);
//static bool dcd_read_packet_memory_ff(tu_fifo_t * ff, uint16_t src, uint16_t wNBytes);

// Using a function due to better type checks
// This seems better than having to do type casts everywhere else
static inline void reg16_clear_bits(__IO uint16_t *reg, uint16_t mask) {
  *reg = (uint16_t)(*reg & ~mask);
}

// Bits in ISTR are cleared upon writing 0
static inline void clear_istr_bits(uint16_t mask) {
  USB->ISTR = ~mask;
}

void dcd_init (uint8_t rhport)
{
  /* Clocks should already be enabled */
  /* Use __HAL_RCC_USB_CLK_ENABLE(); to enable the clocks before calling this function */

  /* The RM mentions to use a special ordering of PDWN and FRES, but this isn't done in HAL.
   * Here, the RM is followed. */

  for(uint32_t i = 0; i<200; i++) // should be a few us
  {
    asm("NOP");
  }
	// Perform USB peripheral reset
  USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
  for(uint32_t i = 0; i<200; i++) // should be a few us
  {
    asm("NOP");
  }
  reg16_clear_bits(&USB->CNTR, USB_CNTR_PDWN);// Remove powerdown
  // Wait startup time, for F042 and F070, this is <= 1 us.
  for(uint32_t i = 0; i<200; i++) // should be a few us
  {
    asm("NOP");
  }
  USB->CNTR = 0; // Enable USB
  
  USB->BTABLE = DCD_STM32_BTABLE_BASE;

  USB->ISTR = 0; // Clear pending interrupts

  // Reset endpoints to disabled
  for(uint32_t i=0; i<STFSDEV_EP_COUNT; i++)
  {
    // This doesn't clear all bits since some bits are "toggle", but does set the type to DISABLED.
    pcd_set_endpoint(USB,i,0u);
  }

  USB->CNTR |= USB_CNTR_RESETM | (USE_SOF ? USB_CNTR_SOFM : 0) | USB_CNTR_ESOFM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
  dcd_handle_bus_reset();
  
  // Enable pull-up if supported
  if ( dcd_connect ) dcd_connect(rhport);
}

// Define only on MCU with internal pull-up. BSP can define on MCU without internal PU.
#if defined(USB_BCDR_DPPU)

// Disable internal D+ PU
void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  USB->BCDR &= ~(USB_BCDR_DPPU);
}

// Enable internal D+ PU
void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  USB->BCDR |= USB_BCDR_DPPU;
}

#endif

// Enable device interrupt
void dcd_int_enable (uint8_t rhport)
{
  (void)rhport;
  // Member here forces write to RAM before allowing ISR to execute
  __DSB();
  __ISB();
#if CFG_TUSB_MCU == OPT_MCU_STM32F0 || CFG_TUSB_MCU == OPT_MCU_STM32L0
  NVIC_EnableIRQ(USB_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
  // Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to enable the right IRQs.
  #ifdef SYSCFG_CFGR1_USB_IT_RMP
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP)
  {
    NVIC_EnableIRQ(USB_HP_IRQn);
    NVIC_EnableIRQ(USB_LP_IRQn);
    NVIC_EnableIRQ(USBWakeUp_RMP_IRQn);
  }
  else
  #endif
  {
    NVIC_EnableIRQ(USB_HP_CAN_TX_IRQn);
    NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);
    NVIC_EnableIRQ(USBWakeUp_IRQn);
  }
#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
  NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_EnableIRQ(USBWakeUp_IRQn);
#else
  #error Unknown arch in USB driver
#endif
}

// Disable device interrupt
void dcd_int_disable(uint8_t rhport)
{
  (void)rhport;

#if CFG_TUSB_MCU == OPT_MCU_STM32F0 || CFG_TUSB_MCU == OPT_MCU_STM32L0
  NVIC_DisableIRQ(USB_IRQn);
#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
  // Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to disable the right IRQs.
  #ifdef SYSCFG_CFGR1_USB_IT_RMP
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP)
  {
    NVIC_DisableIRQ(USB_HP_IRQn);
    NVIC_DisableIRQ(USB_LP_IRQn);
    NVIC_DisableIRQ(USBWakeUp_RMP_IRQn);
  }
  else
  #endif
  {
    NVIC_DisableIRQ(USB_HP_CAN_TX_IRQn);
    NVIC_DisableIRQ(USB_LP_CAN_RX0_IRQn);
    NVIC_DisableIRQ(USBWakeUp_IRQn);
  }
#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  NVIC_DisableIRQ(USB_HP_CAN1_TX_IRQn);
  NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_DisableIRQ(USBWakeUp_IRQn);
#else
  #error Unknown arch in USB driver
#endif

  // CMSIS has a membar after disabling interrupts
}

// Receive Set Address request, mcu port must also include status IN response
void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  (void) dev_addr;

  // Respond with status
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);

  // DCD can only set address after status for this request is complete.
  // do it at dcd_edpt0_status_complete()
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;

  USB->CNTR |= (uint16_t) USB_CNTR_RESUME;
  remoteWakeCountdown = 4u; // required to be 1 to 15 ms, ESOF should trigger every 1ms.
}

static const tusb_desc_endpoint_t ep0OUT_desc =
{
  .bLength          = sizeof(tusb_desc_endpoint_t),
  .bDescriptorType  = TUSB_DESC_ENDPOINT,

  .bEndpointAddress = 0x00,
  .bmAttributes     = { .xfer = TUSB_XFER_CONTROL },
  .wMaxPacketSize   = { .size = CFG_TUD_ENDPOINT0_SIZE },
  .bInterval        = 0
};

static const tusb_desc_endpoint_t ep0IN_desc =
{
  .bLength          = sizeof(tusb_desc_endpoint_t),
  .bDescriptorType  = TUSB_DESC_ENDPOINT,

  .bEndpointAddress = 0x80,
  .bmAttributes     = { .xfer = TUSB_XFER_CONTROL },
  .wMaxPacketSize   = { .size = CFG_TUD_ENDPOINT0_SIZE },
  .bInterval        = 0
};

static void dcd_handle_bus_reset(void)
{
  //__IO uint16_t * const epreg = &(EPREG(0));
  USB->DADDR = 0u; // disable USB peripheral by clearing the EF flag

  // Clear all EPREG (or maybe this is automatic? I'm not sure)
  for(uint32_t i=0; i<STFSDEV_EP_COUNT; i++)
  {
    pcd_set_endpoint(USB,i,0u);
  }

  dcd_pma_alloc_reset();
  dcd_edpt_open (0, &ep0OUT_desc);
  dcd_edpt_open (0, &ep0IN_desc);

  USB->DADDR = USB_DADDR_EF; // Set enable flag, and leaving the device address as zero.
}

// Handle CTR interrupt for the TX/IN direction
//
// Upon call, (wIstr & USB_ISTR_DIR) == 0U
static void dcd_ep_ctr_tx_handler(uint32_t wIstr)
{
  uint32_t EPindex = wIstr & USB_ISTR_EP_ID;
  uint32_t wEPRegVal = pcd_get_endpoint(USB, EPindex);

  // Verify the CTR_TX bit is set. This was in the ST Micro code,
  // but I'm not sure it's actually necessary?
  if((wEPRegVal & USB_EP_CTR_TX) == 0U)
  {
    return;
  }

  /* clear int flag */
  pcd_clear_tx_ep_ctr(USB, EPindex);

  xfer_ctl_t * xfer = xfer_ctl_ptr(EPindex,TUSB_DIR_IN);
  if((xfer->total_len != xfer->queued_len)) /* TX not complete */
  {
      dcd_transmit_packet(xfer, EPindex);
  }
  else /* TX Complete */
  {
    dcd_event_xfer_complete(0, (uint8_t)(0x80 + EPindex), xfer->total_len, XFER_RESULT_SUCCESS, true);
  }
}

// Handle CTR interrupt for the RX/OUT direction
//
// Upon call, (wIstr & USB_ISTR_DIR) == 0U
static void dcd_ep_ctr_rx_handler(uint32_t wIstr)
{
  uint32_t EPindex = wIstr & USB_ISTR_EP_ID;
  uint32_t wEPRegVal = pcd_get_endpoint(USB, EPindex);
  uint32_t count = pcd_get_ep_rx_cnt(USB,EPindex);

  xfer_ctl_t *xfer = xfer_ctl_ptr(EPindex,TUSB_DIR_OUT);

  // Verify the CTR_RX bit is set. This was in the ST Micro code,
  // but I'm not sure it's actually necessary?
  if((wEPRegVal & USB_EP_CTR_RX) == 0U)
  {
    return;
  }
  
  if((EPindex == 0U) && ((wEPRegVal & USB_EP_SETUP) != 0U)) /* Setup packet */
  {
    // The setup_received function uses memcpy, so this must first copy the setup data into
    // user memory, to allow for the 32-bit access that memcpy performs.
    uint8_t userMemBuf[8];
    /* Get SETUP Packet*/
    if(count == 8) // Setup packet should always be 8 bytes. If not, ignore it, and try again.
    {
      // Must reset EP to NAK (in case it had been stalling) (though, maybe too late here)
      pcd_set_ep_rx_status(USB,0u,USB_EP_RX_NAK);
      pcd_set_ep_tx_status(USB,0u,USB_EP_TX_NAK);
      dcd_read_packet_memory(userMemBuf, *pcd_ep_rx_address_ptr(USB,EPindex), 8);
      dcd_event_setup_received(0, (uint8_t*)userMemBuf, true);
    }
  }
  else
  {
    // Clear RX CTR interrupt flag
    if(EPindex != 0u)
    {
      pcd_clear_rx_ep_ctr(USB, EPindex);
    }

    if (count != 0U)
    {
#if 0 // TODO support dcd_edpt_xfer_fifo API
      if (xfer->ff)
      {
        dcd_read_packet_memory_ff(xfer->ff, *pcd_ep_rx_address_ptr(USB,EPindex), count);
      }
      else
#endif
      {
        dcd_read_packet_memory(&(xfer->buffer[xfer->queued_len]), *pcd_ep_rx_address_ptr(USB,EPindex), count);
      }

      xfer->queued_len = (uint16_t)(xfer->queued_len + count);
    }

    if ((count < xfer->max_packet_size) || (xfer->queued_len == xfer->total_len))
    {
      /* RX COMPLETE */
      dcd_event_xfer_complete(0, EPindex, xfer->queued_len, XFER_RESULT_SUCCESS, true);
      // Though the host could still send, we don't know.
      // Does the bulk pipe need to be reset to valid to allow for a ZLP?
    }
    else
    {
      uint32_t remaining = (uint32_t)xfer->total_len - (uint32_t)xfer->queued_len;
      if(remaining >= xfer->max_packet_size) {
        pcd_set_ep_rx_cnt(USB, EPindex,xfer->max_packet_size);
      } else {
        pcd_set_ep_rx_cnt(USB, EPindex,remaining);
      }
      pcd_set_ep_rx_status(USB, EPindex, USB_EP_RX_VALID);
    }
  }

  // For EP0, prepare to receive another SETUP packet.
  // Clear CTR last so that a new packet does not overwrite the packing being read.
  // (Based on the docs, it seems SETUP will always be accepted after CTR is cleared)
  if(EPindex == 0u)
  {
      // Always be prepared for a status packet...
    pcd_set_ep_rx_cnt(USB, EPindex, CFG_TUD_ENDPOINT0_SIZE);
    pcd_clear_rx_ep_ctr(USB, EPindex);
  }
}

static void dcd_ep_ctr_handler(void)
{
  uint32_t wIstr;

  /* stay in loop while pending interrupts */
  while (((wIstr = USB->ISTR) & USB_ISTR_CTR) != 0U)
  {

    if ((wIstr & USB_ISTR_DIR) == 0U) /* TX/IN */
    {
      dcd_ep_ctr_tx_handler(wIstr);
    }
    else /* RX/OUT*/
    {
      dcd_ep_ctr_rx_handler(wIstr);
    }
  }
}

void dcd_int_handler(uint8_t rhport) {

  (void) rhport;

  uint32_t int_status = USB->ISTR;
  //const uint32_t handled_ints = USB_ISTR_CTR | USB_ISTR_RESET | USB_ISTR_WKUP
  //    | USB_ISTR_SUSP | USB_ISTR_SOF | USB_ISTR_ESOF;
  // unused IRQs: (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_L1REQ )

  // The ST driver loops here on the CTR bit, but that loop has been moved into the
  // dcd_ep_ctr_handler(), so less need to loop here. The other interrupts shouldn't
  // be triggered repeatedly.

  if(int_status & USB_ISTR_RESET) {
    // USBRST is start of reset.
    clear_istr_bits(USB_ISTR_RESET);
    dcd_handle_bus_reset();
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
    return; // Don't do the rest of the things here; perhaps they've been cleared?
  }

  if (int_status & USB_ISTR_CTR)
  {
    /* servicing of the endpoint correct transfer interrupt */
    /* clear of the CTR flag into the sub */
    dcd_ep_ctr_handler();
  }

  if (int_status & USB_ISTR_WKUP)
  {
    reg16_clear_bits(&USB->CNTR, USB_CNTR_LPMODE);
    reg16_clear_bits(&USB->CNTR, USB_CNTR_FSUSP);
    clear_istr_bits(USB_ISTR_WKUP);
    dcd_event_bus_signal(0, DCD_EVENT_RESUME, true);
  }

  if (int_status & USB_ISTR_SUSP)
  {
    /* Suspend is asserted for both suspend and unplug events. without Vbus monitoring,
     * these events cannot be differentiated, so we only trigger suspend. */

    /* Force low-power mode in the macrocell */
    USB->CNTR |= USB_CNTR_FSUSP;
    USB->CNTR |= USB_CNTR_LPMODE;

    /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
    clear_istr_bits(USB_ISTR_SUSP);
    dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);
  }

#if USE_SOF
  if(int_status & USB_ISTR_SOF) {
    clear_istr_bits(USB_ISTR_SOF);
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }
#endif 

  if(int_status & USB_ISTR_ESOF) {
    if(remoteWakeCountdown == 1u)
    {
      USB->CNTR &= (uint16_t)(~USB_CNTR_RESUME);
    }
    if(remoteWakeCountdown > 0u)
    {
      remoteWakeCountdown--;
    }
    clear_istr_bits(USB_ISTR_ESOF);
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

// Invoked when a control transfer's status stage is complete.
// May help DCD to prepare for next control transfer, this API is optional.
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS )
  {
    uint8_t const dev_addr = (uint8_t) request->wValue;

    // Setting new address after the whole request is complete
    reg16_clear_bits(&USB->DADDR, USB_DADDR_ADD);
    USB->DADDR = (uint16_t)(USB->DADDR | dev_addr); // leave the enable bit set
  }
}

static void dcd_pma_alloc_reset(void)
{
  ep_buf_ptr = DCD_STM32_BTABLE_BASE + 8*MAX_EP_COUNT; // 8 bytes per endpoint (two TX and two RX words, each)
  //TU_LOG2("dcd_pma_alloc_reset()\r\n");
  for(uint32_t i=0; i<MAX_EP_COUNT; i++)
  {
    xfer_ctl_ptr(i,TUSB_DIR_OUT)->pma_alloc_size = 0U;
    xfer_ctl_ptr(i,TUSB_DIR_IN)->pma_alloc_size = 0U;
    xfer_ctl_ptr(i,TUSB_DIR_OUT)->pma_ptr = 0U;
    xfer_ctl_ptr(i,TUSB_DIR_IN)->pma_ptr = 0U;
  }
}

/***
 * Allocate a section of PMA
 * 
 * If the EP number has already been allocated, and the new allocation
 * is larger than the old allocation, then this will fail with a TU_ASSERT.
 * (This is done to simplify the code. More complicated algorithms could be used)
 * 
 * During failure, TU_ASSERT is used. If this happens, rework/reallocate memory manually.
 */
static uint16_t dcd_pma_alloc(uint8_t ep_addr, size_t length)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);
  xfer_ctl_t* epXferCtl = xfer_ctl_ptr(epnum,dir);

  if(epXferCtl->pma_alloc_size != 0U)
  {
    //TU_LOG2("dcd_pma_alloc(%x,%x)=%x (cached)\r\n",ep_addr,length,epXferCtl->pma_ptr);
    // Previously allocated
    TU_ASSERT(length <= epXferCtl->pma_alloc_size, 0xFFFF);  // Verify no larger than previous alloc
    return epXferCtl->pma_ptr;
  }
  
  uint16_t addr = ep_buf_ptr; 
  ep_buf_ptr = (uint16_t)(ep_buf_ptr + length); // increment buffer pointer
  
  // Verify no overflow
  TU_ASSERT(ep_buf_ptr <= PMA_LENGTH, 0xFFFF);
  
  epXferCtl->pma_ptr = addr;
  epXferCtl->pma_alloc_size = length;
  //TU_LOG2("dcd_pma_alloc(%x,%x)=%x\r\n",ep_addr,length,addr);

  return addr;
}

/***
 * Free a block of PMA space
 */
static void dcd_pma_free(uint8_t ep_addr)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  // Presently, this should never be called for EP0 IN/OUT
  TU_ASSERT(open_ep_count > 2, /**/);
  TU_ASSERT(xfer_ctl_ptr(epnum,dir)->max_packet_size != 0, /**/);
  open_ep_count--;

  // If count is 2, only EP0 should be open, so allocations can be mostly reset.

  if(open_ep_count == 2)
  {
    ep_buf_ptr = DCD_STM32_BTABLE_BASE + 8*MAX_EP_COUNT + 2*CFG_TUD_ENDPOINT0_SIZE; // 8 bytes per endpoint (two TX and two RX words, each), and EP0

    // Skip EP0
    for(uint32_t i=1; i<MAX_EP_COUNT; i++)
    {
      xfer_ctl_ptr(i,TUSB_DIR_OUT)->pma_alloc_size = 0U;
      xfer_ctl_ptr(i,TUSB_DIR_IN)->pma_alloc_size = 0U;
      xfer_ctl_ptr(i,TUSB_DIR_OUT)->pma_ptr = 0U;
      xfer_ctl_ptr(i,TUSB_DIR_IN)->pma_ptr = 0U;
    }
  }
}

// The STM32F0 doesn't seem to like |= or &= to manipulate the EP#R registers,
// so I'm using the #define from HAL here, instead.

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc)
{
  (void)rhport;
  uint8_t const epnum = tu_edpt_number(p_endpoint_desc->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(p_endpoint_desc->bEndpointAddress);
  const uint16_t epMaxPktSize = p_endpoint_desc->wMaxPacketSize.size;
  uint16_t pma_addr;
  uint32_t wType;
  
  // Isochronous not supported (yet), and some other driver assumptions.
  TU_ASSERT(p_endpoint_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);
  TU_ASSERT(epnum < MAX_EP_COUNT);

  // Set type
  switch(p_endpoint_desc->bmAttributes.xfer) {
  case TUSB_XFER_CONTROL:
    wType = USB_EP_CONTROL;
    break;
#if (0)
  case TUSB_XFER_ISOCHRONOUS: // FIXME: Not yet supported
    wType = USB_EP_ISOCHRONOUS;
    break;
#endif

  case TUSB_XFER_BULK:
    wType = USB_EP_CONTROL;
    break;

  case TUSB_XFER_INTERRUPT:
    wType = USB_EP_INTERRUPT;
    break;

  default:
    TU_ASSERT(false);
  }

  pcd_set_eptype(USB, epnum, wType);
  pcd_set_ep_address(USB, epnum, epnum);
  // Be normal, for now, instead of only accepting zero-byte packets (on control endpoint)
  // or being double-buffered (bulk endpoints)
  pcd_clear_ep_kind(USB,0);

  pma_addr = dcd_pma_alloc(p_endpoint_desc->bEndpointAddress, p_endpoint_desc->wMaxPacketSize.size);

  if(dir == TUSB_DIR_IN)
  {
    *pcd_ep_tx_address_ptr(USB, epnum) = pma_addr;
    pcd_set_ep_tx_cnt(USB, epnum, p_endpoint_desc->wMaxPacketSize.size);
    pcd_clear_tx_dtog(USB, epnum);
    pcd_set_ep_tx_status(USB,epnum,USB_EP_TX_NAK);
  }
  else
  {
    *pcd_ep_rx_address_ptr(USB, epnum) = pma_addr;
    pcd_set_ep_rx_cnt(USB, epnum, p_endpoint_desc->wMaxPacketSize.size);
    pcd_clear_rx_dtog(USB, epnum);
    pcd_set_ep_rx_status(USB, epnum, USB_EP_RX_NAK);
  }

  xfer_ctl_ptr(epnum, dir)->max_packet_size = epMaxPktSize;

  return true;
}

/**
 * Close an endpoint.
 * 
 * This function may be called with interrupts enabled or disabled.
 * 
 * This also clears transfers in progress, should there be any.
 */
void dcd_edpt_close (uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;
  uint32_t const epnum = tu_edpt_number(ep_addr);
  uint32_t const dir   = tu_edpt_dir(ep_addr);
  
  if(dir == TUSB_DIR_IN)
  {
    pcd_set_ep_tx_status(USB,epnum,USB_EP_TX_DIS);
  }
  else
  {
    pcd_set_ep_rx_status(USB, epnum, USB_EP_RX_DIS);
  }

  dcd_pma_free(ep_addr);
}

// Currently, single-buffered, and only 64 bytes at a time (max)

static void dcd_transmit_packet(xfer_ctl_t * xfer, uint16_t ep_ix)
{
  uint16_t len = (uint16_t)(xfer->total_len - xfer->queued_len);

  if(len > xfer->max_packet_size) // max packet size for FS transfer
  {
    len = xfer->max_packet_size;
  }
  uint16_t oldAddr = *pcd_ep_tx_address_ptr(USB,ep_ix);

#if 0 // TODO support dcd_edpt_xfer_fifo API
  if (xfer->ff)
  {
    dcd_write_packet_memory_ff(xfer->ff, oldAddr, len);
  }
  else
#endif
  {
    dcd_write_packet_memory(oldAddr, &(xfer->buffer[xfer->queued_len]), len);
  }
  xfer->queued_len = (uint16_t)(xfer->queued_len + len);

  pcd_set_ep_tx_cnt(USB,ep_ix,len);
  pcd_set_ep_tx_status(USB, ep_ix, USB_EP_TX_VALID);
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = xfer_ctl_ptr(epnum,dir);

  xfer->buffer = buffer;
  // xfer->ff     = NULL; // TODO support dcd_edpt_xfer_fifo API
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;

  if ( dir == TUSB_DIR_OUT )
  {
    // A setup token can occur immediately after an OUT STATUS packet so make sure we have a valid
    // buffer for the control endpoint.
    if (epnum == 0 && buffer == NULL)
    {
        xfer->buffer = (uint8_t*)_setup_packet;
    }
    if(total_bytes > xfer->max_packet_size)
    {
      pcd_set_ep_rx_cnt(USB,epnum,xfer->max_packet_size);
    } else {
      pcd_set_ep_rx_cnt(USB,epnum,total_bytes);
    }
    pcd_set_ep_rx_status(USB, epnum, USB_EP_RX_VALID);
  }
  else // IN
  {
    dcd_transmit_packet(xfer,epnum);
  }
  return true;
}

#if 0 // TODO support dcd_edpt_xfer_fifo API
bool dcd_edpt_xfer_fifo (uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = xfer_ctl_ptr(epnum,dir);

  xfer->buffer = NULL;
  // xfer->ff     = ff; // TODO support dcd_edpt_xfer_fifo API
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;

  if ( dir == TUSB_DIR_OUT )
  {
    if(total_bytes > xfer->max_packet_size)
    {
      pcd_set_ep_rx_cnt(USB,epnum,xfer->max_packet_size);
    } else {
      pcd_set_ep_rx_cnt(USB,epnum,total_bytes);
    }
    pcd_set_ep_rx_status(USB, epnum, USB_EP_RX_VALID);
  }
  else // IN
  {
    dcd_transmit_packet(xfer,epnum);
  }
  return true;
}
#endif

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  if (ep_addr & 0x80)
  { // IN
    pcd_set_ep_tx_status(USB, ep_addr & 0x7F, USB_EP_TX_STALL);
  }
  else
  { // OUT
    pcd_set_ep_rx_status(USB, ep_addr, USB_EP_RX_STALL);
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void)rhport;

  if (ep_addr & 0x80)
  { // IN
    ep_addr &= 0x7F;

    pcd_set_ep_tx_status(USB,ep_addr, USB_EP_TX_NAK);

    /* Reset to DATA0 if clearing stall condition. */
    pcd_clear_tx_dtog(USB,ep_addr);
  }
  else
  { // OUT
    /* Reset to DATA0 if clearing stall condition. */
    pcd_clear_rx_dtog(USB,ep_addr);

    pcd_set_ep_rx_status(USB,ep_addr, USB_EP_RX_NAK);
  }
}

// Packet buffer access can only be 8- or 16-bit.
/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA).
  *        This uses byte-access for user memory (so support non-aligned buffers)
  *        and 16-bit access for packet memory.
  * @param   dst, byte address in PMA; must be 16-bit aligned
  * @param   src pointer to user memory area.
  * @param   wPMABufAddr address into PMA.
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */
static bool dcd_write_packet_memory(uint16_t dst, const void *__restrict src, size_t wNBytes)
{
  uint32_t n =  ((uint32_t)wNBytes + 1U) >> 1U;
  uint32_t i;
  uint16_t temp1, temp2;
  const uint8_t * srcVal;

  // The GCC optimizer will combine access to 32-bit sizes if we let it. Force
  // it volatile so that it won't do that.
  __IO uint16_t *pdwVal;

  srcVal = src;
  pdwVal = &pma[PMA_STRIDE*(dst>>1)];

  for (i = n; i != 0; i--)
  {
    temp1 = (uint16_t) *srcVal;
    srcVal++;
    temp2 = temp1 | ((uint16_t)((uint16_t) ((*srcVal) << 8U))) ;
    *pdwVal = temp2;
    pdwVal += PMA_STRIDE;
    srcVal++;
  }
  return true;
}

#if 0 // TODO support dcd_edpt_xfer_fifo API
/**
  * @brief Copy from FIFO to packet memory area (PMA).
  *        Uses byte-access of system memory and 16-bit access of packet memory
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */

// THIS FUNCTION IS UNTESTED

static bool dcd_write_packet_memory_ff(tu_fifo_t * ff, uint16_t dst, uint16_t wNBytes)
{
  // Since we copy from a ring buffer FIFO, a wrap might occur making it necessary to conduct two copies
  // Check for first linear part
  void * src;
  uint16_t len = tu_fifo_get_linear_read_info(ff, 0, &src, wNBytes);  // We want to read from the FIFO        - THIS FUNCTION CHANGED!!!
  TU_VERIFY(len && dcd_write_packet_memory(dst, src, len));           // and write it into the PMA
  tu_fifo_advance_read_pointer(ff, len);

  // Check for wrapped part
  if (len < wNBytes)
  {
    // Get remaining wrapped length
    uint16_t len2 = tu_fifo_get_linear_read_info(ff, 0, &src, wNBytes - len);
    TU_VERIFY(len2);

    // Update destination pointer
    dst += len;

    // Since PMA is accessed 16-bit wise we need to handle the case when a 16 bit value was split
    if (len % 2)    // If len is uneven there is a byte left to copy
    {
      // Since PMA can accessed only 16 bit-wise we copy the last byte again
      tu_fifo_backward_read_pointer(ff, 1);                 // Move one byte back and copy two bytes for the PMA
      tu_fifo_read_n(ff, (void *) &pma[PMA_STRIDE*(dst>>1)], 2);     // Since EP FIFOs must be of item size 1 this is safe to do
      dst++;
      len2--;
    }

    TU_VERIFY(dcd_write_packet_memory(dst, src, len2));
    tu_fifo_advance_write_pointer(ff, len2);
  }

  return true;
}
#endif

/**
  * @brief Copy a buffer from packet memory area (PMA) to user memory area.
  *        Uses byte-access of system memory and 16-bit access of packet memory
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */
static bool dcd_read_packet_memory(void *__restrict dst, uint16_t src, size_t wNBytes)
{
  uint32_t n = (uint32_t)wNBytes >> 1U;
  uint32_t i;
  // The GCC optimizer will combine access to 32-bit sizes if we let it. Force
  // it volatile so that it won't do that.
  __IO const uint16_t *pdwVal;
  uint32_t temp;

  pdwVal = &pma[PMA_STRIDE*(src>>1)];
  uint8_t *dstVal = (uint8_t*)dst;

  for (i = n; i != 0U; i--)
  {
    temp = *pdwVal;
    pdwVal += PMA_STRIDE;
    *dstVal++ = ((temp >> 0) & 0xFF);
    *dstVal++ = ((temp >> 8) & 0xFF);
  }

  if (wNBytes % 2)
  {
    temp = *pdwVal;
    pdwVal += PMA_STRIDE;
    *dstVal++ = ((temp >> 0) & 0xFF);
  }
  return true;
}

#if 0 // TODO support dcd_edpt_xfer_fifo API
/**
  * @brief Copy a buffer from user packet memory area (PMA) to FIFO.
  *        Uses byte-access of system memory and 16-bit access of packet memory
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */

// THIS FUNCTION IS UNTESTED

static bool dcd_read_packet_memory_ff(tu_fifo_t * ff, uint16_t src, uint16_t wNBytes)
{
  // Since we copy into a ring buffer FIFO, a wrap might occur making it necessary to conduct two copies
  // Check for first linear part
  void * dst;
  uint16_t len = tu_fifo_get_linear_write_info(ff, 0, &dst, wNBytes);           // THIS FUNCTION CHANGED!!!!
  TU_VERIFY(len && dcd_read_packet_memory(dst, src, len));
  tu_fifo_advance_write_pointer(ff, len);

  // Check for wrapped part
  if (len < wNBytes)
  {
    // Get remaining wrapped length
    uint16_t len2 = tu_fifo_get_linear_write_info(ff, 0, &dst, wNBytes - len);
    TU_VERIFY(len2);

    // Update source pointer
    src += len;

    // Since PMA is accessed 16-bit wise we need to handle the case when a 16 bit value was split
    if (len % 2)    // If len is uneven there is a byte left to copy
    {
      uint32_t temp = pma[PMA_STRIDE*(src>>1)];
      *((uint8_t *)dst++) = ((temp >> 8) & 0xFF);
      src++;
      len2--;
    }

    TU_VERIFY(dcd_read_packet_memory(dst, src, len2));
    tu_fifo_advance_write_pointer(ff, len2);
  }

  return true;
}

#endif

#endif

