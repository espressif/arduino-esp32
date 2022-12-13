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

#ifndef _TUSB_EHCI_H_
#define _TUSB_EHCI_H_


/* Abbreviation
 * HC: Host Controller
 * HCD: Host Controller Driver
 * QHD: Queue Head for non-ISO transfer
 * QTD: Queue Transfer Descriptor for non-ISO transfer
 * ITD: Iso Transfer Descriptor for highspeed
 * SITD: Split ISO Transfer Descriptor for full-speed
 * SMASK: Start Split mask for Slipt Transaction
 * CMASK: Complete Split mask for Slipt Transaction
*/

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// EHCI CONFIGURATION & CONSTANTS
//--------------------------------------------------------------------+

// TODO merge OHCI with EHCI
enum {
  EHCI_MAX_ITD  = 4,
  EHCI_MAX_SITD = 16
};

//--------------------------------------------------------------------+
// EHCI Data Structure
//--------------------------------------------------------------------+
enum
{
  EHCI_QTYPE_ITD = 0 ,
  EHCI_QTYPE_QHD     ,
  EHCI_QTYPE_SITD    ,
  EHCI_QTYPE_FSTN
};

/// EHCI PID
enum
{
  EHCI_PID_OUT = 0 ,
  EHCI_PID_IN      ,
  EHCI_PID_SETUP
};

/// Link pointer
typedef union {
	uint32_t address;
	struct  {
		uint32_t terminate : 1;
		uint32_t type      : 2;
	};
}ehci_link_t;

/// Queue Element Transfer Descriptor
/// Qtd is used to declare overlay in ehci_qhd_t -> cannot be declared with TU_ATTR_ALIGNED(32)
typedef struct
{
	// Word 0: Next QTD Pointer
	ehci_link_t next;

	// Word 1: Alternate Next QTD Pointer (not used)
	union{
	  ehci_link_t alternate;
	  struct {
	    uint32_t                : 5;
	    uint32_t used           : 1;
	    uint32_t                : 10;
	    uint32_t expected_bytes : 16;
	  };
	};

	// Word 2: qTQ Token
	volatile uint32_t ping_err             : 1  ; ///< For Highspeed: 0 Out, 1 Ping. Full/Slow used as error indicator
	volatile uint32_t non_hs_split_state   : 1  ; ///< Used by HC to track the state of split transaction
	volatile uint32_t non_hs_missed_uframe : 1  ; ///< HC misses a complete split transaction
	volatile uint32_t xact_err             : 1  ; ///< Error (Timeout, CRC, Bad PID ... )
	volatile uint32_t babble_err           : 1  ; ///< Babble detected, also set Halted bit to 1
	volatile uint32_t buffer_err           : 1  ; ///< Data overrun/underrun error
	volatile uint32_t halted               : 1  ; ///< Serious error or STALL received
	volatile uint32_t active               : 1  ; ///< Start transfer, clear by HC when complete

	uint32_t pid                           : 2  ; ///< 0: OUT, 1: IN, 2 Setup
	volatile uint32_t err_count            : 2  ; ///< Error Counter of consecutive errors
	volatile uint32_t current_page         : 3  ; ///< Index into the qTD buffer pointer list
	uint32_t int_on_complete               : 1  ; ///< Interrupt on complete
	volatile uint32_t total_bytes          : 15 ; ///< Transfer bytes, decreased during transaction
	volatile uint32_t data_toggle          : 1  ; ///< Data Toggle bit


	/// Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the start of a 4K page
	uint32_t buffer[5];
} ehci_qtd_t;

TU_VERIFY_STATIC( sizeof(ehci_qtd_t) == 32, "size is not correct" );

/// Queue Head
typedef struct TU_ATTR_ALIGNED(32)
{
  // Word 0: Next QHD
	ehci_link_t next;

	// Word 1: Endpoint Characteristics
	uint32_t dev_addr              : 7  ; ///< device address
	uint32_t fl_inactive_next_xact : 1  ; ///< Only valid for Periodic with Full/Slow speed
	uint32_t ep_number             : 4  ; ///< EP number
	uint32_t ep_speed              : 2  ; ///< 0: Full, 1: Low, 2: High
	uint32_t data_toggle_control   : 1  ; ///< 0: use DT in qHD, 1: use DT in qTD
	uint32_t head_list_flag        : 1  ; ///< Head of the queue
	uint32_t max_packet_size       : 11 ; ///< Max packet size
	uint32_t fl_ctrl_ep_flag       : 1  ; ///< 1 if is Full/Low speed control endpoint
	uint32_t nak_reload            : 4  ; ///< Used by HC

	// Word 2: Endpoint Capabilities
	uint32_t int_smask             : 8  ; ///< Interrupt Schedule Mask
	uint32_t fl_int_cmask          : 8  ; ///< Split Completion Mask for Full/Slow speed
	uint32_t fl_hub_addr           : 7  ; ///< Hub Address for Full/Slow speed
	uint32_t fl_hub_port           : 7  ; ///< Hub Port for Full/Slow speed
	uint32_t mult                  : 2  ; ///< Transaction per micro frame

	// Word 3: Current qTD Pointer
	volatile uint32_t qtd_addr;

	// Word 4-11: Transfer Overlay
	volatile ehci_qtd_t qtd_overlay;

	//--------------------------------------------------------------------+
  /// Due to the fact QHD is 32 bytes aligned but occupies only 48 bytes
	/// thus there are 16 bytes padding free that we can make use of.
  //--------------------------------------------------------------------+
	uint8_t used;
	uint8_t removing; // removed from asyn list, waiting for async advance
	uint8_t pid;
	uint8_t interval_ms; // polling interval in frames (or millisecond)

	uint16_t total_xferred_bytes; // number of bytes xferred until a qtd with ioc bit set
	uint8_t reserved2[2];

	ehci_qtd_t * volatile p_qtd_list_head;	// head of the scheduled TD list
	ehci_qtd_t * volatile p_qtd_list_tail;	// tail of the scheduled TD list
} ehci_qhd_t;

TU_VERIFY_STATIC( sizeof(ehci_qhd_t) == 64, "size is not correct" );

/// Highspeed Isochronous Transfer Descriptor (section 3.3)
typedef struct TU_ATTR_ALIGNED(32) {
	// Word 0: Next Link Pointer
	ehci_link_t next;

	// Word 1-8: iTD Transaction Status and Control List
	struct  {
	  // iTD Control
		volatile uint32_t offset          : 12 ; ///< This field is a value that is an offset, expressed in bytes, from the beginning of a buffer.
		volatile uint32_t page_select     : 3  ; ///< These bits are set by software to indicate which of the buffer page pointers the offset field in this slot should be concatenated to produce the starting memory address for this transaction. The valid range of values for this field is 0 to 6
             uint32_t int_on_complete : 1  ; ///< If this bit is set to a one, it specifies that when this transaction completes, the Host Controller should issue an interrupt at the next interrupt threshold
		volatile uint32_t length          : 12 ; ///< For an OUT, this field is the number of data bytes the host controller will send during the transaction. The host controller is not required to update this field to reflect the actual number of bytes transferred during the transfer
																						 ///< For an IN, the initial value of the field is the number of bytes the host expects the endpoint to deliver. During the status update, the host controller writes back the number of bytes successfully received. The value in this register is the actual byte count
		// iTD Status
		volatile uint32_t error           : 1  ; ///< Set to a one by the Host Controller during status update in the case where the host did not receive a valid response from the device (Timeout, CRC, Bad PID, etc.). This bit may only be set for isochronous IN transactions.
		volatile uint32_t babble_err      : 1  ; ///< Set to a 1 by the Host Controller during status update when a babble is detected during the transaction
		volatile uint32_t buffer_err      : 1  ; ///< Set to a 1 by the Host Controller during status update to indicate that the Host Controller is unable to keep up with the reception of incoming data (overrun) or is unable to supply data fast enough during transmission (underrun).
		volatile uint32_t active          : 1  ; ///< Set to 1 by software to enable the execution of an isochronous transaction by the Host Controller
	} xact[8];

	// Word 9-15  Buffer Page Pointer List (Plus)
	uint32_t BufferPointer[7];

//	// FIXME: Store meta data into buffer pointer reserved for saving memory
//	/*---------- HCD Area ----------*/
//	uint32_t used;
//	uint32_t IhdIdx;
//	uint32_t reserved[6];
} ehci_itd_t;

TU_VERIFY_STATIC( sizeof(ehci_itd_t) == 64, "size is not correct" );

/// Split (Full-Speed) Isochronous Transfer Descriptor
typedef struct TU_ATTR_ALIGNED(32)
{
  // Word 0: Next Link Pointer
	ehci_link_t next;

	// Word 1: siTD Endpoint Characteristics
	uint32_t dev_addr    : 7; ///< This field selects the specific device serving as the data source or sink.
	uint32_t             : 1; ///< reserved
	uint32_t ep_number   : 4; ///< This 4-bit field selects the particular endpoint number on the device serving as the data source or sink.
	uint32_t             : 4; ///< This field is reserved and should be set to zero.
	uint32_t hub_addr    : 7; ///< This field holds the device address of the transaction translators’ hub.
	uint32_t             : 1; ///< reserved
	uint32_t port_number : 7; ///< This field is the port number of the recipient transaction translator.
	uint32_t direction   : 1; ///<  0 = OUT; 1 = IN. This field encodes whether the full-speed transaction should be an IN or OUT.

	// Word 2: Micro-frame Schedule Control
	uint8_t int_smask   ; ///< This field (along with the Activeand SplitX-statefields in the Statusbyte) are used to determine during which micro-frames the host controller should execute complete-split transactions
	uint8_t fl_int_cmask; ///< This field (along with the Activeand SplitX-statefields in the Statusbyte) are used to determine during which micro-frames the host controller should execute start-split transactions.
	uint16_t reserved ; ///< reserved

	// Word 3: siTD Transfer Status and Control
	// Status [7:0] TODO identical to qTD Token'status --> refactor later
	volatile uint32_t                 : 1  ; // reserved
	volatile uint32_t split_state     : 1  ;
	volatile uint32_t missed_uframe   : 1  ;
	volatile uint32_t xact_err        : 1  ;
	volatile uint32_t babble_err      : 1  ;
	volatile uint32_t buffer_err      : 1  ;
	volatile uint32_t error           : 1  ;
	volatile uint32_t active          : 1  ;
	// Micro-frame Schedule Control
	volatile uint32_t cmask_progress  : 8  ; ///< This field is used by the host controller to record which split-completes have been executed. See Section 4.12.3.3.2 for behavioral requirements.
	volatile uint32_t total_bytes     : 10 ; ///< This field is initialized by software to the total number of bytes expected in this transfer. Maximum value is 1023
	volatile uint32_t                 : 4  ; ///< reserved
	volatile uint32_t page_select     : 1  ; ///< Used to indicate which data page pointer should be concatenated with the CurrentOffsetfield to construct a data buffer pointer
					 uint32_t int_on_complete : 1  ; ///< Do not interrupt when transaction is complete. 1 = Do interrupt when transaction is complete
					 uint32_t                 : 0  ; // padding to the end of current storage unit

	/// Word 4-5: Buffer Pointer List
	uint32_t buffer[2];		// buffer[1] TP: Transaction Position - T-Count: Transaction Count

// 	union{
// 		uint32_t BufferPointer1;
// 		struct  {
// 			volatile uint32_t TCount : 3;
// 			volatile uint32_t TPosition : 2;
// 		};
// 	};

	/*---------- Word 6 ----------*/
	ehci_link_t back;

	/// SITD is 32-byte aligned but occupies only 28 --> 4 bytes for storing extra data
	uint8_t used;
	uint8_t ihd_idx;
	uint8_t reserved2[2];
} ehci_sitd_t;

TU_VERIFY_STATIC( sizeof(ehci_sitd_t) == 32, "size is not correct" );

//--------------------------------------------------------------------+
// EHCI Operational Register
//--------------------------------------------------------------------+
enum ehci_interrupt_mask_{
  EHCI_INT_MASK_USB                   = TU_BIT(0),
  EHCI_INT_MASK_ERROR                 = TU_BIT(1),
  EHCI_INT_MASK_PORT_CHANGE           = TU_BIT(2),

  EHCI_INT_MASK_FRAMELIST_ROLLOVER    = TU_BIT(3),
  EHCI_INT_MASK_PCI_HOST_SYSTEM_ERROR = TU_BIT(4),
  EHCI_INT_MASK_ASYNC_ADVANCE         = TU_BIT(5),
  EHCI_INT_MASK_NXP_SOF               = TU_BIT(7),

  EHCI_INT_MASK_NXP_ASYNC             = TU_BIT(18),
  EHCI_INT_MASK_NXP_PERIODIC          = TU_BIT(19),

  EHCI_INT_MASK_ALL                   =
      EHCI_INT_MASK_USB | EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE |
      EHCI_INT_MASK_FRAMELIST_ROLLOVER | EHCI_INT_MASK_PCI_HOST_SYSTEM_ERROR |
      EHCI_INT_MASK_ASYNC_ADVANCE | EHCI_INT_MASK_NXP_SOF |
      EHCI_INT_MASK_NXP_ASYNC | EHCI_INT_MASK_NXP_PERIODIC
};

enum ehci_usbcmd_pos_ {
  EHCI_USBCMD_POS_RUN_STOP               = 0,
  EHCI_USBCMD_POS_FRAMELIST_SIZE         = 2,
  EHCI_USBCMD_POS_PERIOD_ENABLE          = 4,
  EHCI_USBCMD_POS_ASYNC_ENABLE           = 5,
  EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB = 15,
  EHCI_USBCMD_POS_INTERRUPT_THRESHOLD    = 16
};

enum ehci_portsc_change_mask_{
  EHCI_PORTSC_MASK_CURRENT_CONNECT_STATUS = TU_BIT(0),
  EHCI_PORTSC_MASK_CONNECT_STATUS_CHANGE  = TU_BIT(1),
  EHCI_PORTSC_MASK_PORT_EANBLED           = TU_BIT(2),
  EHCI_PORTSC_MASK_PORT_ENABLE_CHAGNE     = TU_BIT(3),
  EHCI_PORTSC_MASK_OVER_CURRENT_CHANGE    = TU_BIT(5),
  EHCI_PORTSC_MASK_PORT_RESET             = TU_BIT(8),

  EHCI_PORTSC_MASK_ALL =
      EHCI_PORTSC_MASK_CONNECT_STATUS_CHANGE |
      EHCI_PORTSC_MASK_PORT_ENABLE_CHAGNE |
      EHCI_PORTSC_MASK_OVER_CURRENT_CHANGE
};

typedef volatile struct
{
  union {
    uint32_t command;

    struct {
      uint32_t run_stop               : 1 ; ///< 1=Run. 0=Stop
      uint32_t reset                  : 1 ; ///< SW write 1 to reset HC, clear by HC when complete
      uint32_t framelist_size         : 2 ; ///< Frame List size 0: 1024, 1: 512, 2: 256
      uint32_t periodic_enable        : 1 ; ///< This bit controls whether the host controller skips processing the Periodic Schedule. Values mean: 0b Do not process the Periodic Schedule 1b Use the PERIODICLISTBASE register to access the Periodic Schedule.
      uint32_t async_enable           : 1 ; ///< This bit controls whether the host controller skips processing the Asynchronous Schedule. Values mean: 0b Do not process the Asynchronous Schedule 1b Use the ASYNCLISTADDR register to access the Asynchronous Schedule.
      uint32_t async_adv_doorbell     : 1 ; ///< Tell HC to interrupt next time it advances async list. Clear by HC
      uint32_t light_reset            : 1 ; ///< Reset HC without affecting ports state
      uint32_t async_park_count       : 2 ; ///< not used by tinyusb
      uint32_t                        : 1 ;
      uint32_t async_park_enable      : 1 ; ///< Enable park mode, not used by tinyusb
      uint32_t                        : 3 ;
      uint32_t nxp_framelist_size_msb : 1 ; ///< NXP customized : Bit 2 of the Frame List Size bits \n 011b: 128 elements \n 100b: 64 elements \n 101b: 32 elements \n 110b: 16 elements \n 111b: 8 elements
      uint32_t int_threshold          : 8 ; ///< Default 08h. Interrupt rate in unit of micro frame
    }command_bm;
  };

  union {
    uint32_t status;

    struct {
      uint32_t usb                   : 1  ; ///< qTD with IOC is retired
      uint32_t usb_error             : 1  ; ///< qTD retired due to error
      uint32_t port_change_detect    : 1  ; ///< Set when PortOwner or ForcePortResume change from 0 -> 1
      uint32_t framelist_rollover    : 1  ; ///< R/WC The Host Controller sets this bit to a one when the Frame List Index(see Section 2.3.4) rolls over from its maximum value to zero. The exact value at which the rollover occurs depends on the frame list size. For example, if the frame list size (as programmed in the Frame List Sizefield of the USBCMD register) is 1024, the Frame Index Registerrolls over every time FRINDEX[13] toggles. Similarly, if the size is 512, the Host Controller sets this bit to a one every time FRINDEX[12] toggles.
      uint32_t pci_host_system_error : 1  ; ///< R/WC (not used by NXP) The Host Controller sets this bit to 1 when a serious error occurs during a host system access involving the Host Controller module. In a PCI system, conditions that set this bit to 1 include PCI Parity error, PCI Master Abort, and PCI Target Abort. When this error occurs, the Host Controller clears the Run/Stop bit in the Command register to prevent further execution of the scheduled TDs.
      uint32_t async_adv             : 1  ; ///< Async Advance interrupt
      uint32_t                       : 1  ;
      uint32_t nxp_int_sof           : 1  ; ///< NXP customized:  this bit will be set every 125us and can be used by host controller driver as a time base.
      uint32_t                       : 4  ;
      uint32_t hc_halted             : 1  ; ///< Opposite value to run_stop bit.
      uint32_t reclamation           : 1  ; ///< Used to detect empty async shecudle
      uint32_t periodic_status       : 1  ; ///< Periodic schedule status
      uint32_t async_status          : 1  ; ///< Async schedule status
      uint32_t                       : 2  ;
      uint32_t nxp_int_async         : 1  ; ///< NXP customized: This bit is set by the Host Controller when the cause of an interrupt is a completion of a USB transaction where the Transfer Descriptor (TD) has an interrupt on complete (IOC) bit set and the TD was from the asynchronous schedule. This bit is also set by the Host when a short packet is detected and the packet is on the asynchronous schedule.
      uint32_t nxp_int_period        : 1  ; ///< NXP customized: This bit is set by the Host Controller when the cause of an interrupt is a completion of a USB transaction where the Transfer Descriptor (TD) has an interrupt on complete (IOC) bit set and the TD was from the periodic schedule.
      uint32_t                       : 12 ;
    }status_bm;
  };

  union{
    uint32_t inten;

    struct {
      uint32_t usb                   : 1  ;
      uint32_t usb_error             : 1  ;
      uint32_t port_change_detect    : 1  ;
      uint32_t framelist_rollover    : 1  ;
      uint32_t pci_host_system_error : 1  ;
      uint32_t async_adv             : 1  ;
      uint32_t                       : 1  ;
      uint32_t nxp_int_sof           : 1  ;
      uint32_t                       : 10 ;
      uint32_t nxp_int_async         : 1  ;
      uint32_t nxp_int_period        : 1  ;
      uint32_t                       : 12 ;
    }inten_bm;
  };

  uint32_t frame_index        ; ///< Micro frame counter
  uint32_t ctrl_ds_seg        ; ///< Control Data Structure Segment
  uint32_t periodic_list_base ; ///< Beginning address of perodic frame list
  uint32_t async_list_addr    ; ///< Address of next async QHD to be executed
  uint32_t nxp_tt_control     ; ///< nxp embedded transaction translator (reserved by EHCI specs)
  uint32_t reserved[8]        ;
  uint32_t config_flag        ; ///< not used by NXP

  union {
    uint32_t portsc             ; ///< port status and control
    struct {
      uint32_t current_connect_status      : 1; ///< 0: No device, 1: Device is present on port
      uint32_t connect_status_change       : 1; ///< Change in Current Connect Status
      uint32_t port_enabled                : 1; ///< Ports can only be enabled by HC as a part of the reset and enable. SW can write 0 to disable
      uint32_t port_enable_change          : 1; ///< Port Enabled has changed
      uint32_t over_current_active         : 1; ///< Port has an over-current condition
      uint32_t over_current_change         : 1; ///< Change to Over-current Active
      uint32_t force_port_resume           : 1; ///< Resume detected/driven on port. This functionality defined for manipulating this bit depends on the value of the Suspend bit.
      uint32_t suspend                     : 1; ///< Port in suspend state
      uint32_t port_reset                  : 1; ///< 1=Port is in Reset. 0=Port is not in Reset
      uint32_t nxp_highspeed_status        : 1; ///< NXP customized: 0=connected to the port is not in High-speed mode, 1=connected to the port is in High-speed mode
      uint32_t line_status                 : 2; ///< D+/D- state: 00: SE0, 10: J-state, 01: K-state
      uint32_t port_power                  : 1; ///< 0= power off, 1= power on
      uint32_t port_owner                  : 1; ///< not used by NXP
      uint32_t port_indicator_control      : 2; ///< 00b: off, 01b: Amber, 10b: green, 11b: undefined
      uint32_t port_test_control           : 4; ///< Port test mode, not used by tinyusb
      uint32_t wake_on_connect_enable      : 1; ///< Enables device connects as wake-up events
      uint32_t wake_on_disconnect_enable   : 1; ///< Enables device disconnects as wake-up events
      uint32_t wake_on_over_current_enable : 1; ///< Enables over-current conditions as wake-up events
      uint32_t nxp_phy_clock_disable       : 1; ///< NXP customized: the PHY can be put into Low Power Suspend – Clock Disable when the downstream device has been put into suspend mode or when no downstream device is connected. Low power suspend is completely under the control of software. 0: enable PHY clock, 1: disable PHY clock
      uint32_t nxp_port_force_fullspeed    : 1; ///< NXP customized: Writing this bit to a 1 will force the port to only connect at Full Speed. It disables the chirp sequence that allowsthe port to identify itself as High Speed. This is useful for testing FS configurations with a HS host, hub or device.
      uint32_t TU_RESERVED                 : 1;
      uint32_t nxp_port_speed              : 2; ///< NXP customized: This register field indicates the speed atwhich the port is operating. For HS mode operation in the host controllerand HS/FS operation in the device controller the port routing steers data to the Protocol engine. For FS and LS mode operation in the host controller, the port routing steers data to the Protocol Engine w/ Embedded Transaction Translator. 0x0: Fullspeed, 0x1: Lowspeed, 0x2: Highspeed
      uint32_t TU_RESERVED                 : 4;
    }portsc_bm;
  };
}ehci_registers_t;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_EHCI_H_ */
