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

#ifndef _TUSB_OHCI_H_
#define _TUSB_OHCI_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// OHCI CONFIGURATION & CONSTANTS
//--------------------------------------------------------------------+
#define HOST_HCD_XFER_INTERRUPT // TODO interrupt is used widely, should always be enabled
#define OHCI_PERIODIC_LIST (defined HOST_HCD_XFER_INTERRUPT || defined HOST_HCD_XFER_ISOCHRONOUS)

// TODO merge OHCI with EHCI
enum {
  OHCI_MAX_ITD = 4
};

#define ED_MAX       (CFG_TUH_DEVICE_MAX*CFG_TUH_ENDPOINT_MAX)
#define GTD_MAX      ED_MAX

//--------------------------------------------------------------------+
// OHCI Data Structure
//--------------------------------------------------------------------+
typedef struct {
  uint32_t interrupt_table[32];
  volatile uint16_t frame_number;
  volatile uint16_t frame_pad;
  volatile uint32_t done_head;
  uint8_t reserved[116+4];  // TODO try to make use of this area if possible, extra 4 byte to make the whole struct size = 256
}ohci_hcca_t; // TU_ATTR_ALIGNED(256)

TU_VERIFY_STATIC( sizeof(ohci_hcca_t) == 256, "size is not correct" );

// common link item for gtd and itd for list travel
// use as pointer only
typedef struct TU_ATTR_ALIGNED(16) {
  uint32_t reserved[2];
  volatile uint32_t next;
  uint32_t reserved2;
}ohci_td_item_t;

typedef struct TU_ATTR_ALIGNED(16)
{
	// Word 0
	uint32_t used                    : 1;
	uint32_t index                   : 4;  // endpoint index the td belongs to, or device address in case of control xfer
  uint32_t expected_bytes          : 13; // TODO available for hcd

  uint32_t buffer_rounding         : 1;
  uint32_t pid                     : 2;
  uint32_t delay_interrupt         : 3;
  volatile uint32_t data_toggle    : 2;
  volatile uint32_t error_count    : 2;
  volatile uint32_t condition_code : 4;

	// Word 1
	volatile uint8_t* current_buffer_pointer;

	// Word 2 : next TD
	volatile uint32_t next;

	// Word 3
	uint8_t* buffer_end;
} ohci_gtd_t;

TU_VERIFY_STATIC( sizeof(ohci_gtd_t) == 16, "size is not correct" );

typedef struct TU_ATTR_ALIGNED(16)
{
  // Word 0
	uint32_t dev_addr          : 7;
	uint32_t ep_number         : 4;
	uint32_t pid               : 2;
	uint32_t speed             : 1;
	uint32_t skip              : 1;
	uint32_t is_iso            : 1;
	uint32_t max_packet_size   : 11;
	      // HCD: make use of 5 reserved bits
	uint32_t used              : 1;
	uint32_t is_interrupt_xfer : 1;
	uint32_t is_stalled        : 1;
	uint32_t                   : 2;

	// Word 1
	uint32_t td_tail;

	// Word 2
	volatile union {
		uint32_t address;
		struct {
			uint32_t halted : 1;
			uint32_t toggle : 1;
			uint32_t : 30;
		};
	}td_head;

	// Word 3: next ED
	uint32_t next;
} ohci_ed_t;

TU_VERIFY_STATIC( sizeof(ohci_ed_t) == 16, "size is not correct" );

typedef struct TU_ATTR_ALIGNED(32)
{
	/*---------- Word 1 ----------*/
  uint32_t starting_frame          : 16;
  uint32_t                         : 5; // can be used
  uint32_t delay_interrupt         : 3;
  uint32_t frame_count             : 3;
  uint32_t                         : 1; // can be used
  volatile uint32_t condition_code : 4;

	/*---------- Word 2 ----------*/
	uint32_t buffer_page0;	// 12 lsb bits can be used

	/*---------- Word 3 ----------*/
	volatile uint32_t next;

	/*---------- Word 4 ----------*/
	uint32_t buffer_end;

	/*---------- Word 5-8 ----------*/
	volatile uint16_t offset_packetstatus[8];
} ochi_itd_t;

TU_VERIFY_STATIC( sizeof(ochi_itd_t) == 32, "size is not correct" );

// structure with member alignment required from large to small
typedef struct TU_ATTR_ALIGNED(256)
{
  ohci_hcca_t hcca;

  ohci_ed_t bulk_head_ed; // static bulk head (dummy)
  ohci_ed_t period_head_ed; // static periodic list head (dummy)

  // control endpoints has reserved resources
  struct {
    ohci_ed_t ed;
    ohci_gtd_t gtd;
  }control[CFG_TUH_DEVICE_MAX+CFG_TUH_HUB+1];

  //  ochi_itd_t itd[OHCI_MAX_ITD]; // itd requires alignment of 32
  ohci_ed_t ed_pool[ED_MAX];
  ohci_gtd_t gtd_pool[GTD_MAX];

  volatile uint16_t frame_number_hi;

} ohci_data_t;

//--------------------------------------------------------------------+
// OHCI Operational Register
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// OHCI Data Organization
//--------------------------------------------------------------------+
typedef volatile struct
{
  uint32_t revision;

  union {
    uint32_t control;
    struct {
      uint32_t control_bulk_service_ratio : 2;
      uint32_t periodic_list_enable       : 1;
      uint32_t isochronous_enable         : 1;
      uint32_t control_list_enable        : 1;
      uint32_t bulk_list_enable           : 1;
      uint32_t hc_functional_state        : 2;
      uint32_t interrupt_routing          : 1;
      uint32_t remote_wakeup_connected    : 1;
      uint32_t remote_wakeup_enale        : 1;
      uint32_t TU_RESERVED                : 21;
    }control_bit;
  };

  union {
    uint32_t command_status;
    struct {
      uint32_t controller_reset         : 1;
      uint32_t control_list_filled      : 1;
      uint32_t bulk_list_filled         : 1;
      uint32_t ownership_change_request : 1;
      uint32_t                          : 12;
      uint32_t scheduling_overrun_count : 2;
    }command_status_bit;
  };

  uint32_t interrupt_status;
  uint32_t interrupt_enable;
  uint32_t interrupt_disable;

  uint32_t hcca;
  uint32_t period_current_ed;
  uint32_t control_head_ed;
  uint32_t control_current_ed;
  uint32_t bulk_head_ed;
  uint32_t bulk_current_ed;
  uint32_t done_head;

  uint32_t frame_interval;
  uint32_t frame_remaining;
  uint32_t frame_number;
  uint32_t periodic_start;
  uint32_t lowspeed_threshold;

  union {
    uint32_t rh_descriptorA;
    struct {
      uint32_t number_downstream_ports     : 8;
      uint32_t power_switching_mode        : 1;
      uint32_t no_power_switching          : 1;
      uint32_t device_type                 : 1;
      uint32_t overcurrent_protection_mode : 1;
      uint32_t no_over_current_protection  : 1;
      uint32_t reserved                    : 11;
      uint32_t power_on_to_good_time       : 8;
    } rh_descriptorA_bit;
  };

  union {
    uint32_t rh_descriptorB;
    struct {
      uint32_t device_removable        : 16;
      uint32_t port_power_control_mask : 16;
    } rh_descriptorB_bit;
  };

  union {
    uint32_t rh_status;
    struct {
      uint32_t local_power_status            : 1; // read Local Power Status; write: Clear Global Power
      uint32_t over_current_indicator        : 1;
      uint32_t                               : 13;
      uint32_t device_remote_wakeup_enable   : 1;
      uint32_t local_power_status_change     : 1;
      uint32_t over_current_indicator_change : 1;
      uint32_t                               : 13;
      uint32_t clear_remote_wakeup_enable    : 1;
    }rh_status_bit;
  };

  union {
    uint32_t rhport_status[TUP_OHCI_RHPORTS];
    struct {
      uint32_t current_connect_status             : 1;
      uint32_t port_enable_status                 : 1;
      uint32_t port_suspend_status                : 1;
      uint32_t port_over_current_indicator        : 1;
      uint32_t port_reset_status                  : 1;
      uint32_t                                    : 3;
      uint32_t port_power_status                  : 1;
      uint32_t low_speed_device_attached          : 1;
      uint32_t                                    : 6;
      uint32_t connect_status_change              : 1;
      uint32_t port_enable_status_change          : 1;
      uint32_t port_suspend_status_change         : 1;
      uint32_t port_over_current_indicator_change : 1;
      uint32_t port_reset_status_change           : 1;
      uint32_t TU_RESERVED                        : 11;
    }rhport_status_bit[TUP_OHCI_RHPORTS];
  };
}ohci_registers_t;

TU_VERIFY_STATIC( sizeof(ohci_registers_t) == (0x54 + (4 * TUP_OHCI_RHPORTS)), "size is not correct");

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OHCI_H_ */
