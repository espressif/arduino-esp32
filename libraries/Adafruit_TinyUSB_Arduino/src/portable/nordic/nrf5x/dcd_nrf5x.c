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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUSB_MCU == OPT_MCU_NRF5X

#include "nrf.h"
#include "nrf_clock.h"
#include "nrf_power.h"
#include "nrfx_usbd_errata.h"
#include "device/dcd.h"

// TODO remove later
#include "device/usbd.h"
#include "device/usbd_pvt.h" // to use defer function helper

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/
enum
{
  // Max allowed by USB specs
  MAX_PACKET_SIZE   = 64,

  // Mask of all END event (IN & OUT) for all endpoints. ENDEPIN0-7, ENDEPOUT0-7, ENDISOIN, ENDISOOUT
  EDPT_END_ALL_MASK = (0xff << USBD_INTEN_ENDEPIN0_Pos) | (0xff << USBD_INTEN_ENDEPOUT0_Pos) |
                      USBD_INTENCLR_ENDISOIN_Msk | USBD_INTEN_ENDISOOUT_Msk
};

enum
{
  EP_ISO_NUM   = 8, // Endpoint number is fixed (8) for ISOOUT and ISOIN
  EP_CBI_COUNT = 8  // Control Bulk Interrupt endpoints count
};

// Transfer Descriptor
typedef struct
{
  uint8_t* buffer;
  uint16_t total_len;
  volatile uint16_t actual_len;
  uint16_t  mps; // max packet size

  // nRF will auto accept OUT packet after DMA is done
  // indicate packet is already ACK
  volatile bool data_received;

  // Set to true when data was transferred from RAM to ISO IN output buffer.
  // New data can be put in ISO IN output buffer after SOF.
  bool iso_in_transfer_ready;

} xfer_td_t;

// Data for managing dcd
static struct
{
  // All 8 endpoints including control IN & OUT (offset 1)
  // +1 for ISO endpoints
  xfer_td_t xfer[EP_CBI_COUNT + 1][2];

  // Number of pending DMA that is started but not handled yet by dcd_int_handler().
  // Since nRF can only carry one DMA can run at a time, this value is normally be either 0 or 1.
  // However, in critical section with interrupt disabled, the DMA can be finished and added up
  // until handled by dcd_init_handler() when exiting critical section.
  volatile uint8_t dma_pending;
}_dcd;

/*------------------------------------------------------------------*/
/* Control / Bulk / Interrupt (CBI) Transfer
 *------------------------------------------------------------------*/

// NVIC_GetEnableIRQ is only available in CMSIS v5
#ifndef NVIC_GetEnableIRQ
static inline uint32_t NVIC_GetEnableIRQ(IRQn_Type IRQn)
{
  if ((int32_t)(IRQn) >= 0)
  {
    return((uint32_t)(((NVIC->ISER[(((uint32_t)(int32_t)IRQn) >> 5UL)] & (1UL << (((uint32_t)(int32_t)IRQn) & 0x1FUL))) != 0UL) ? 1UL : 0UL));
  }
  else
  {
    return(0U);
  }
}
#endif

// check if we are in ISR
TU_ATTR_ALWAYS_INLINE static inline bool is_in_isr(void)
{
  return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) ? true : false;
}

// helper to start DMA
// TODO use Cortex M4 LDREX and STREX command (atomic) to have better mutex access to EasyDMA
// since current implementation does not 100% guarded against race condition
static void edpt_dma_start(volatile uint32_t* reg_startep)
{
  // Only one dma can be active
  if ( _dcd.dma_pending )
  {
    if (is_in_isr())
    {
      // Called within ISR, use usbd task to defer later
      usbd_defer_func( (osal_task_func_t) edpt_dma_start, (void*) reg_startep, true );
      return;
    }
    else
    {
      if ( __get_PRIMASK() || !NVIC_GetEnableIRQ(USBD_IRQn) )
      {
        // Called in critical section with interrupt disabled. We have to manually check
        // for the DMA complete by comparing current pending DMA with number of ENDED Events
        uint32_t ended = 0;

        while ( _dcd.dma_pending > ((uint8_t) ended) )
        {
          ended = NRF_USBD->EVENTS_ENDISOIN + NRF_USBD->EVENTS_ENDISOOUT;

          for (uint8_t i=0; i<EP_CBI_COUNT; i++)
          {
            ended += NRF_USBD->EVENTS_ENDEPIN[i] + NRF_USBD->EVENTS_ENDEPOUT[i];
          }
        }
      }else
      {
        // Called in non-critical thread-mode, should be 99% of the time.
        // Should be safe to blocking wait until previous DMA transfer complete
        while ( _dcd.dma_pending ) { }
      }
    }
  }

  _dcd.dma_pending++;

  (*reg_startep) = 1;
  __ISB(); __DSB();
}

// DMA is complete
static void edpt_dma_end(void)
{
  TU_ASSERT(_dcd.dma_pending, );
  _dcd.dma_pending = 0;
}

// helper to set TASKS_EP0STATUS / TASKS_EP0RCVOUT since they also need EasyDMA
// However TASKS_EP0STATUS doesn't trigger any DMA transfer and got ENDED event subsequently
// Therefore dma_running state will be corrected right away
void start_ep0_task(volatile uint32_t* reg_task)
{
  edpt_dma_start(reg_task);

  // correct the dma_running++ in dma start
  if (_dcd.dma_pending) _dcd.dma_pending--;
}

// helper getting td
static inline xfer_td_t* get_td(uint8_t epnum, uint8_t dir)
{
  return &_dcd.xfer[epnum][dir];
}

// Start DMA to move data from Endpoint -> RAM
static void xact_out_dma(uint8_t epnum)
{
  xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);
  uint32_t xact_len;

  if (epnum == EP_ISO_NUM)
  {
    xact_len = NRF_USBD->SIZE.ISOOUT;
    // If ZERO bit is set, ignore ISOOUT length
    if (xact_len & USBD_SIZE_ISOOUT_ZERO_Msk) xact_len = 0;
    else
    {
      // Trigger DMA move data from Endpoint -> SRAM
      NRF_USBD->ISOOUT.PTR = (uint32_t) xfer->buffer;
      NRF_USBD->ISOOUT.MAXCNT = xact_len;

      edpt_dma_start(&NRF_USBD->TASKS_STARTISOOUT);
    }
  }
  else
  {
    xact_len = (uint8_t)NRF_USBD->SIZE.EPOUT[epnum];

    // Trigger DMA move data from Endpoint -> SRAM
    NRF_USBD->EPOUT[epnum].PTR = (uint32_t) xfer->buffer;
    NRF_USBD->EPOUT[epnum].MAXCNT = xact_len;

    edpt_dma_start(&NRF_USBD->TASKS_STARTEPOUT[epnum]);
  }

  xfer->buffer     += xact_len;
  xfer->actual_len += xact_len;
}

// Prepare for a CBI transaction IN, call at the start
// it start DMA to transfer data from RAM -> Endpoint
static void xact_in_dma(uint8_t epnum)
{
  xfer_td_t* xfer = get_td(epnum, TUSB_DIR_IN);

  // Each transaction is up to Max Packet Size
  uint16_t const xact_len = tu_min16(xfer->total_len - xfer->actual_len, xfer->mps);

  NRF_USBD->EPIN[epnum].PTR    = (uint32_t) xfer->buffer;
  NRF_USBD->EPIN[epnum].MAXCNT = xact_len;

  xfer->buffer += xact_len;

  edpt_dma_start(&NRF_USBD->TASKS_STARTEPIN[epnum]);
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
void dcd_init (uint8_t rhport)
{
  (void) rhport;
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(USBD_IRQn);
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(USBD_IRQn);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  (void) rhport;
  (void) dev_addr;
  // Set Address is automatically update by hw controller, nothing to do

  // Enable usbevent for suspend and resume detection
  // Since the bus signal D+/D- are stable now.

  // Clear current pending first
  NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;
  NRF_USBD->EVENTS_USBEVENT = 0;

  NRF_USBD->INTENSET = USBD_INTEN_USBEVENT_Msk;
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;

  // Bring controller out of low power mode
  NRF_USBD->LOWPOWER = 0;

  // Initiate RESUME signal
  NRF_USBD->DPDMVALUE = USBD_DPDMVALUE_STATE_Resume;
  NRF_USBD->TASKS_DPDMDRIVE = 1;

  // TODO There is no USBEVENT Resume interrupt
  // We may manually raise DCD_EVENT_RESUME event here
}

// disconnect by disabling internal pull-up resistor on D+/D-
void dcd_disconnect(uint8_t rhport)
{
  (void) rhport;
  NRF_USBD->USBPULLUP = 0;

  // Disable Pull-up does not trigger Power USB Removed, in fact it have no
  // impact on the USB Power status at all -> need to submit unplugged event to the stack.
  dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, false);
}

// connect by enabling internal pull-up resistor on D+/D-
void dcd_connect(uint8_t rhport)
{
  (void) rhport;
  NRF_USBD->USBPULLUP = 1;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  _dcd.xfer[epnum][dir].mps = desc_edpt->wMaxPacketSize.size;

  if (desc_edpt->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS)
  {
    if (dir == TUSB_DIR_OUT)
    {
      NRF_USBD->INTENSET = TU_BIT(USBD_INTEN_ENDEPOUT0_Pos + epnum);
      NRF_USBD->EPOUTEN |= TU_BIT(epnum);

      // Write any value to SIZE register will allow nRF to ACK/accept data
      NRF_USBD->SIZE.EPOUT[epnum] = 0;
    }else
    {
      NRF_USBD->INTENSET = TU_BIT(USBD_INTEN_ENDEPIN0_Pos + epnum);
      NRF_USBD->EPINEN  |= TU_BIT(epnum);
    }
  }
  else
  {
    TU_ASSERT(epnum == EP_ISO_NUM);
    if (dir == TUSB_DIR_OUT)
    {
      // SPLIT ISO buffer when ISO IN endpoint is already opened.
      if (_dcd.xfer[EP_ISO_NUM][TUSB_DIR_IN].mps) NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;
      // Clear old events
      NRF_USBD->EVENTS_ENDISOOUT = 0;
      // Clear SOF event in case interrupt was not enabled yet.
      if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0) NRF_USBD->EVENTS_SOF = 0;
      // Enable SOF and ISOOUT interrupts, and ISOOUT endpoint.
      NRF_USBD->INTENSET = USBD_INTENSET_ENDISOOUT_Msk | USBD_INTENSET_SOF_Msk;
      NRF_USBD->EPOUTEN |= USBD_EPOUTEN_ISOOUT_Msk;
    }
    else
    {
      NRF_USBD->EVENTS_ENDISOIN = 0;
      // SPLIT ISO buffer when ISO OUT endpoint is already opened.
      if (_dcd.xfer[EP_ISO_NUM][TUSB_DIR_OUT].mps) NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;
      // Clear SOF event in case interrupt was not enabled yet.
      if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0) NRF_USBD->EVENTS_SOF = 0;
      // Enable SOF and ISOIN interrupts, and ISOIN endpoint.
      NRF_USBD->INTENSET = USBD_INTENSET_ENDISOIN_Msk | USBD_INTENSET_SOF_Msk;
      NRF_USBD->EPINEN  |= USBD_EPINEN_ISOIN_Msk;
    }
  }
  __ISB(); __DSB();

  return true;
}

void dcd_edpt_close (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if (epnum != EP_ISO_NUM)
  {
    // CBI
    if (dir == TUSB_DIR_OUT)
    {
      NRF_USBD->INTENCLR = TU_BIT(USBD_INTEN_ENDEPOUT0_Pos + epnum);
      NRF_USBD->EPOUTEN &= ~TU_BIT(epnum);
    }
    else
    {
      NRF_USBD->INTENCLR = TU_BIT(USBD_INTEN_ENDEPIN0_Pos + epnum);
      NRF_USBD->EPINEN &= ~TU_BIT(epnum);
    }
  }
  else
  {
    _dcd.xfer[EP_ISO_NUM][dir].mps = 0;
    // ISO
    if (dir == TUSB_DIR_OUT)
    {
      NRF_USBD->INTENCLR = USBD_INTENCLR_ENDISOOUT_Msk;
      NRF_USBD->EPOUTEN &= ~USBD_EPOUTEN_ISOOUT_Msk;
      NRF_USBD->EVENTS_ENDISOOUT = 0;
    }
    else
    {
      NRF_USBD->INTENCLR = USBD_INTENCLR_ENDISOIN_Msk;
      NRF_USBD->EPINEN &= ~USBD_EPINEN_ISOIN_Msk;
    }
    // One of the ISO endpoints closed, no need to split buffers any more.
    NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_OneDir;
    // When both ISO endpoint are close there is no need for SOF any more.
    if (_dcd.xfer[EP_ISO_NUM][TUSB_DIR_IN].mps + _dcd.xfer[EP_ISO_NUM][TUSB_DIR_OUT].mps == 0) NRF_USBD->INTENCLR = USBD_INTENCLR_SOF_Msk;
  }
  __ISB(); __DSB();
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_td_t* xfer = get_td(epnum, dir);

  xfer->buffer     = buffer;
  xfer->total_len  = total_bytes;
  xfer->actual_len = 0;

  // Control endpoint with zero-length packet and opposite direction to 1st request byte --> status stage
  bool const control_status = (epnum == 0 && total_bytes == 0 && dir != tu_edpt_dir(NRF_USBD->BMREQUESTTYPE));

  if ( control_status )
  {
    // Status Phase also requires EasyDMA has to be available as well !!!!
    start_ep0_task(&NRF_USBD->TASKS_EP0STATUS);

    // The nRF doesn't interrupt on status transmit so we queue up a success response.
    dcd_event_xfer_complete(0, ep_addr, 0, XFER_RESULT_SUCCESS, is_in_isr());
  }
  else if ( dir == TUSB_DIR_OUT )
  {
    if ( epnum == 0 )
    {
      // Accept next Control Out packet. TASKS_EP0RCVOUT also require EasyDMA
      start_ep0_task(&NRF_USBD->TASKS_EP0RCVOUT);
    }else
    {
      if ( xfer->data_received )
      {
        // Data may already be received previously
        xfer->data_received = false;

        // start DMA to copy to SRAM
        xact_out_dma(epnum);
      }
      else
      {
        // nRF auto accept next Bulk/Interrupt OUT packet
        // nothing to do
      }
    }
  }
  else
  {
    // Start DMA to copy data from RAM -> Endpoint
    xact_in_dma(epnum);
  }

  return true;
}

void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);

  if ( epnum == 0 )
  {
    NRF_USBD->TASKS_EP0STALL = 1;
  }else if (epnum != EP_ISO_NUM)
  {
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_Stall << USBD_EPSTALL_STALL_Pos) | ep_addr;
  }

  __ISB(); __DSB();
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  (void) rhport;
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if ( epnum != 0 && epnum != EP_ISO_NUM )
  {
    // clear stall
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep_addr;

    // reset data toggle to DATA0
    NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | ep_addr;

    // Write any value to SIZE register will allow nRF to ACK/accept data
    // Drop any pending data
    if (dir == TUSB_DIR_OUT) NRF_USBD->SIZE.EPOUT[epnum] = 0;

    __ISB(); __DSB();
  }
}

/*------------------------------------------------------------------*/
/* Interrupt Handler
 *------------------------------------------------------------------*/
void bus_reset(void)
{
  for(int i=0; i<8; i++)
  {
    NRF_USBD->TASKS_STARTEPIN[i] = 0;
    NRF_USBD->TASKS_STARTEPOUT[i] = 0;
  }

  NRF_USBD->TASKS_STARTISOIN  = 0;
  NRF_USBD->TASKS_STARTISOOUT = 0;

  tu_varclr(&_dcd);
  _dcd.xfer[0][TUSB_DIR_IN].mps = MAX_PACKET_SIZE;
  _dcd.xfer[0][TUSB_DIR_OUT].mps = MAX_PACKET_SIZE;
}

void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;

  uint32_t const inten  = NRF_USBD->INTEN;
  uint32_t int_status = 0;

  volatile uint32_t* regevt = &NRF_USBD->EVENTS_USBRESET;

  for(uint8_t i=0; i<USBD_INTEN_EPDATA_Pos+1; i++)
  {
    if ( tu_bit_test(inten, i) && regevt[i]  )
    {
      int_status |= TU_BIT(i);

      // event clear
      regevt[i] = 0;
      __ISB(); __DSB();
    }
  }

  if ( int_status & USBD_INTEN_USBRESET_Msk )
  {
    bus_reset();
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
  }

  // ISOIN: Data was moved to endpoint buffer, client will be notified in SOF
  if ( int_status & USBD_INTEN_ENDISOIN_Msk )
  {
    xfer_td_t* xfer = get_td(EP_ISO_NUM, TUSB_DIR_IN);

    xfer->actual_len = NRF_USBD->ISOIN.AMOUNT;
    // Data transferred from RAM to endpoint output buffer.
    // Next transfer can be scheduled after SOF.
    xfer->iso_in_transfer_ready = true;
  }

  if ( int_status & USBD_INTEN_SOF_Msk )
  {
    // ISOOUT: Transfer data gathered in previous frame from buffer to RAM
    if (NRF_USBD->EPOUTEN & USBD_EPOUTEN_ISOOUT_Msk)
    {
      xact_out_dma(EP_ISO_NUM);
    }
    // ISOIN: Notify client that data was transferred
    xfer_td_t* xfer = get_td(EP_ISO_NUM, TUSB_DIR_IN);
    if ( xfer->iso_in_transfer_ready )
    {
      xfer->iso_in_transfer_ready = false;
      dcd_event_xfer_complete(0, EP_ISO_NUM | TUSB_DIR_IN_MASK, xfer->actual_len, XFER_RESULT_SUCCESS, true);
    }
    dcd_event_bus_signal(0, DCD_EVENT_SOF, true);
  }

  if ( int_status & USBD_INTEN_USBEVENT_Msk )
  {
    uint32_t const evt_cause = NRF_USBD->EVENTCAUSE & (USBD_EVENTCAUSE_SUSPEND_Msk | USBD_EVENTCAUSE_RESUME_Msk);
    NRF_USBD->EVENTCAUSE = evt_cause; // clear interrupt

    if ( evt_cause & USBD_EVENTCAUSE_SUSPEND_Msk )
    {
      dcd_event_bus_signal(0, DCD_EVENT_SUSPEND, true);

      // Put controller into low power mode
      NRF_USBD->LOWPOWER = 1;

      // Leave HFXO disable to application, since it may be used by other
    }

    if ( evt_cause & USBD_EVENTCAUSE_RESUME_Msk  )
    {
      dcd_event_bus_signal(0, DCD_EVENT_RESUME , true);
    }
  }

  // Setup tokens are specific to the Control endpoint.
  if ( int_status & USBD_INTEN_EP0SETUP_Msk )
  {
    uint8_t const setup[8] =
    {
      NRF_USBD->BMREQUESTTYPE , NRF_USBD->BREQUEST, NRF_USBD->WVALUEL , NRF_USBD->WVALUEH,
      NRF_USBD->WINDEXL       , NRF_USBD->WINDEXH , NRF_USBD->WLENGTHL, NRF_USBD->WLENGTHH
    };

    // nrf5x hw auto handle set address, there is no need to inform usb stack
    tusb_control_request_t const * request = (tusb_control_request_t const *) setup;

    if ( !(TUSB_REQ_RCPT_DEVICE   == request->bmRequestType_bit.recipient &&
           TUSB_REQ_TYPE_STANDARD == request->bmRequestType_bit.type &&
           TUSB_REQ_SET_ADDRESS   == request->bRequest) )
    {
      dcd_event_setup_received(0, setup, true);
    }
  }

  if ( int_status & EDPT_END_ALL_MASK )
  {
    // DMA complete move data from SRAM -> Endpoint
    edpt_dma_end();
  }

  //--------------------------------------------------------------------+
  /* Control/Bulk/Interrupt (CBI) Transfer
   *
   * Data flow is:
   *           (bus)              (dma)
   *    Host <-------> Endpoint <-------> RAM
   *
   * For CBI OUT:
   *  - Host -> Endpoint
   *      EPDATA (or EP0DATADONE) interrupted, check EPDATASTATUS.EPOUT[i]
   *      to start DMA. For Bulk/Interrupt, this step can occur automatically (without sw),
   *      which means data may or may not be ready (data_received flag).
   *  - Endpoint -> RAM
   *      ENDEPOUT[i] interrupted, transaction complete, sw prepare next transaction
   *
   * For CBI IN:
   *  - RAM -> Endpoint
   *      ENDEPIN[i] interrupted indicate DMA is complete. HW will start
   *      to move data to host
   *  - Endpoint -> Host
   *      EPDATA (or EP0DATADONE) interrupted, check EPDATASTATUS.EPIN[i].
   *      Transaction is complete, sw prepare next transaction
   *
   * Note: in both Control In and Out of Data stage from Host <-> Endpoint
   * EP0DATADONE will be set as interrupt source
   */
  //--------------------------------------------------------------------+

  /* CBI OUT: Endpoint -> SRAM (aka transaction complete)
   * Note: Since nRF controller auto ACK next packet without SW awareness
   * We must handle this stage before Host -> Endpoint just in case 2 event happens at once
   *
   * ISO OUT: Transaction must fit in single packet, it can be shorter then total
   * len if Host decides to sent fewer bytes, it this case transaction is also
   * complete and next transfer is not initiated here like for CBI.
   */
  for(uint8_t epnum=0; epnum<EP_CBI_COUNT+1; epnum++)
  {
    if ( tu_bit_test(int_status, USBD_INTEN_ENDEPOUT0_Pos+epnum))
    {
      xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);
      uint8_t const xact_len = NRF_USBD->EPOUT[epnum].AMOUNT;

      // Transfer complete if transaction len < Max Packet Size or total len is transferred
      if ( (epnum != EP_ISO_NUM) && (xact_len == xfer->mps) && (xfer->actual_len < xfer->total_len) )
      {
        if ( epnum == 0 )
        {
          // Accept next Control Out packet. TASKS_EP0RCVOUT also require EasyDMA
          if ( _dcd.dma_pending )
          {
            // use usbd task to defer later
            usbd_defer_func( (osal_task_func_t) start_ep0_task, (void*) &NRF_USBD->TASKS_EP0RCVOUT, true );
          }else
          {
            start_ep0_task(&NRF_USBD->TASKS_EP0RCVOUT);
          }
        }else
        {
          // nRF auto accept next Bulk/Interrupt OUT packet
          // nothing to do
        }
      }else
      {
        xfer->total_len = xfer->actual_len;

        // CBI OUT complete
        dcd_event_xfer_complete(0, epnum, xfer->actual_len, XFER_RESULT_SUCCESS, true);
      }
    }

    // Ended event for CBI IN : nothing to do
  }

  // Endpoint <-> Host ( In & OUT )
  if ( int_status & (USBD_INTEN_EPDATA_Msk | USBD_INTEN_EP0DATADONE_Msk) )
  {
    uint32_t data_status = NRF_USBD->EPDATASTATUS;
    NRF_USBD->EPDATASTATUS = data_status;
    __ISB(); __DSB();

    // EP0DATADONE is set with either Control Out on IN Data
    // Since EPDATASTATUS cannot be used to determine whether it is control OUT or IN.
    // We will use BMREQUESTTYPE in setup packet to determine the direction
    bool const is_control_in = (int_status & USBD_INTEN_EP0DATADONE_Msk) && (NRF_USBD->BMREQUESTTYPE & TUSB_DIR_IN_MASK);
    bool const is_control_out = (int_status & USBD_INTEN_EP0DATADONE_Msk) && !(NRF_USBD->BMREQUESTTYPE & TUSB_DIR_IN_MASK);

    // CBI In: Endpoint -> Host (transaction complete)
    for(uint8_t epnum=0; epnum<EP_CBI_COUNT; epnum++)
    {
      if ( tu_bit_test(data_status, epnum) || (epnum == 0 && is_control_in) )
      {
        xfer_td_t* xfer = get_td(epnum, TUSB_DIR_IN);

        xfer->actual_len += NRF_USBD->EPIN[epnum].MAXCNT;

        if ( xfer->actual_len < xfer->total_len )
        {
          // Start DMA to copy next data packet
          xact_in_dma(epnum);
        } else
        {
          // CBI IN complete
          dcd_event_xfer_complete(0, epnum | TUSB_DIR_IN_MASK, xfer->actual_len, XFER_RESULT_SUCCESS, true);
        }
      }
    }

    // CBI OUT: Host -> Endpoint
    for(uint8_t epnum=0; epnum<EP_CBI_COUNT; epnum++)
    {
      if ( tu_bit_test(data_status, 16+epnum) || (epnum == 0 && is_control_out) )
      {
        xfer_td_t* xfer = get_td(epnum, TUSB_DIR_OUT);

        if (xfer->actual_len < xfer->total_len)
        {
          xact_out_dma(epnum);
        }else
        {
          // Data overflow !!! Nah, nRF will auto accept next Bulk/Interrupt OUT packet
          // Mark this endpoint with data received
          xfer->data_received = true;
        }
      }
    }
  }
}

//--------------------------------------------------------------------+
// HFCLK helper
//--------------------------------------------------------------------+
#ifdef SOFTDEVICE_PRESENT

// For enable/disable hfclk with SoftDevice
#include "nrf_mbr.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"

#ifndef SD_MAGIC_NUMBER
  #define SD_MAGIC_NUMBER   0x51B1E5DB
#endif

static inline bool is_sd_existed(void)
{
  return *((uint32_t*)(SOFTDEVICE_INFO_STRUCT_ADDRESS+4)) == SD_MAGIC_NUMBER;
}

// check if SD is existed and enabled
static inline bool is_sd_enabled(void)
{
  if ( !is_sd_existed() ) return false;

  uint8_t sd_en = false;
  (void) sd_softdevice_is_enabled(&sd_en);
  return sd_en;
}
#endif

static bool hfclk_running(void)
{
#ifdef SOFTDEVICE_PRESENT
  if ( is_sd_enabled() )
  {
    uint32_t is_running;
    (void) sd_clock_hfclk_is_running(&is_running);
    return (is_running ? true : false);
  }
#endif

  return nrf_clock_hf_is_running(NRF_CLOCK, NRF_CLOCK_HFCLK_HIGH_ACCURACY);
}

static void hfclk_enable(void)
{
  // already running, nothing to do
  if ( hfclk_running() ) return;

#ifdef SOFTDEVICE_PRESENT
  if ( is_sd_enabled() )
  {
    (void)sd_clock_hfclk_request();
    return;
  }
#endif

  nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED);
  nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
}

static void hfclk_disable(void)
{
#ifdef SOFTDEVICE_PRESENT
  if ( is_sd_enabled() )
  {
    (void)sd_clock_hfclk_release();
    return;
  }
#endif

  nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
}

// Power & Clock Peripheral on nRF5x to manage USB
//
// USB Bus power is managed by Power module, there are 3 VBUS power events:
// Detected, Ready, Removed. Upon these power events, This function will
// enable ( or disable ) usb & hfclk peripheral, set the usb pin pull up
// accordingly to the controller Startup/Standby Sequence in USBD 51.4 specs.
//
// Therefore this function must be called to handle USB power event by
// - nrfx_power_usbevt_init() : if Softdevice is not used or enabled
// - SoftDevice SOC event : if SD is used and enabled
void tusb_hal_nrf_power_event (uint32_t event)
{
  // Value is chosen to be as same as NRFX_POWER_USB_EVT_* in nrfx_power.h
  enum {
    USB_EVT_DETECTED = 0,
    USB_EVT_REMOVED = 1,
    USB_EVT_READY = 2
  };

  switch ( event )
  {
    case USB_EVT_DETECTED:
      TU_LOG2("Power USB Detect\r\n");

      if ( !NRF_USBD->ENABLE )
      {
        /* Prepare for READY event receiving */
        NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
        __ISB(); __DSB(); // for sync

#ifdef NRF52_SERIES
        // ERRATA 171, 187, 166
        if ( nrfx_usbd_errata_187() )
        {
          // CRITICAL_REGION_ENTER();
          if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
          {
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
            *((volatile uint32_t *) (0x4006ED14)) = 0x00000003;
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          }
          else
          {
            *((volatile uint32_t *) (0x4006ED14)) = 0x00000003;
          }
          // CRITICAL_REGION_EXIT();
        }

        if ( nrfx_usbd_errata_171() )
        {
          // CRITICAL_REGION_ENTER();
          if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
          {
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
            *((volatile uint32_t *) (0x4006EC14)) = 0x000000C0;
            *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          }
          else
          {
            *((volatile uint32_t *) (0x4006EC14)) = 0x000000C0;
          }
          // CRITICAL_REGION_EXIT();
        }
#endif

        /* Enable the peripheral */
        NRF_USBD->ENABLE = 1;
        __ISB(); __DSB(); // for sync

        // Enable HFCLK
        hfclk_enable();
      }
    break;

    case USB_EVT_READY:
      TU_LOG2("Power USB Ready\r\n");

      // Skip if pull-up is enabled and HCLK is already running.
      // Application probably call this more than necessary.
      if ( NRF_USBD->USBPULLUP && hfclk_running() ) break;

      /* Waiting for USBD peripheral enabled */
      while ( !(USBD_EVENTCAUSE_READY_Msk & NRF_USBD->EVENTCAUSE) ) { }

      NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
      __ISB(); __DSB(); // for sync

#ifdef NRF52_SERIES
      if ( nrfx_usbd_errata_171() )
      {
        // CRITICAL_REGION_ENTER();
        if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
        {
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          *((volatile uint32_t *) (0x4006EC14)) = 0x00000000;
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
        }
        else
        {
          *((volatile uint32_t *) (0x4006EC14)) = 0x00000000;
        }

        // CRITICAL_REGION_EXIT();
      }

      if ( nrfx_usbd_errata_187() )
      {
        // CRITICAL_REGION_ENTER();
        if ( *((volatile uint32_t *) (0x4006EC00)) == 0x00000000 )
        {
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
          *((volatile uint32_t *) (0x4006ED14)) = 0x00000000;
          *((volatile uint32_t *) (0x4006EC00)) = 0x00009375;
        }
        else
        {
          *((volatile uint32_t *) (0x4006ED14)) = 0x00000000;
        }
        // CRITICAL_REGION_EXIT();
      }

      if ( nrfx_usbd_errata_166() )
      {
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x800)) = 0x7E3;
        *((volatile uint32_t *) (NRF_USBD_BASE + 0x804)) = 0x40;

        __ISB(); __DSB();
      }
#endif

      // ISO buffer Lower half for IN, upper half for OUT
      NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

      // Enable interrupt
      NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_EPDATA_Msk |
          USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk | USBD_INTEN_ENDEPOUT0_Msk;

      // Enable interrupt, priorities should be set by application
      NVIC_ClearPendingIRQ(USBD_IRQn);
      NVIC_EnableIRQ(USBD_IRQn);

      // Wait for HFCLK
      while ( !hfclk_running() ) { }

      // Enable pull up
      NRF_USBD->USBPULLUP = 1;
      __ISB(); __DSB(); // for sync
    break;

    case USB_EVT_REMOVED:
      TU_LOG2("Power USB Removed\r\n");
      if ( NRF_USBD->ENABLE )
      {
        // Abort all transfers

        // Disable pull up
        NRF_USBD->USBPULLUP = 0;
        __ISB(); __DSB(); // for sync

        // Disable Interrupt
        NVIC_DisableIRQ(USBD_IRQn);

        // disable all interrupt
        NRF_USBD->INTENCLR = NRF_USBD->INTEN;

        NRF_USBD->ENABLE = 0;
        __ISB(); __DSB(); // for sync

        hfclk_disable();

        dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, is_in_isr());
      }
    break;

    default: break;
  }
}

#endif
