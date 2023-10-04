/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Rafael Silva (@perigoso)
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

#ifndef _TUSB_RUSB2_TYPE_H_
#define _TUSB_RUSB2_TYPE_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// CCRX specific attribute to generate a Code that Accesses Variables in the Declared Size
#ifdef __CCRX__
  #define _ccrx_evenaccess __evenaccess
#else
  #define _ccrx_evenaccess
#endif

/*--------------------------------------------------------------------*/
/* Register Definitions                                           */
/*--------------------------------------------------------------------*/

/* Start of definition of packed structs (used by the CCRX toolchain) */
TU_ATTR_PACKED_BEGIN
TU_ATTR_BIT_FIELD_ORDER_BEGIN

// TODO same as RUSB2_PIPE_TR_t
typedef struct TU_ATTR_PACKED _ccrx_evenaccess {
  union {
    struct {
      uint16_t      : 8;
      uint16_t TRCLR: 1;
      uint16_t TRENB: 1;
      uint16_t      : 0;
    };
    uint16_t TRE;
  };
  uint16_t TRN;
} reg_pipetre_t;

typedef struct {
  union {
    volatile uint16_t E; /* (@ 0x00000000) Pipe Transaction Counter Enable Register */

    struct TU_ATTR_PACKED {
      uint16_t                : 8;
      volatile uint16_t TRCLR : 1; /* [8..8] Transaction Counter Clear */
      volatile uint16_t TRENB : 1; /* [9..9] Transaction Counter Enable */
      uint16_t                : 6;
    } E_b;
  };

  union {
    volatile uint16_t N; /* (@ 0x00000002) Pipe Transaction Counter Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t TRNCNT : 16; /* [15..0] Transaction Counter */
    } N_b;
  };
} RUSB2_PIPE_TR_t; /* Size = 4 (0x4) */


/* RUSB2 Registers Structure */
typedef struct _ccrx_evenaccess {
  union {
    volatile uint16_t SYSCFG; /* (@ 0x00000000) System Configuration Control Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t USBE  : 1; /* [0..0] USB Operation Enable */
      uint16_t                : 2;
      volatile uint16_t DMRPU : 1; /* [3..3] D- Line Resistor Control */
      volatile uint16_t DPRPU : 1; /* [4..4] D+ Line Resistor Control */
      volatile uint16_t DRPD  : 1; /* [5..5] D+/D- Line Resistor Control */
      volatile uint16_t DCFM  : 1; /* [6..6] Controller Function Select */
      volatile uint16_t HSE   : 1; // [7..7] High-Speed Operation Enable
      volatile uint16_t CNEN  : 1; /* [8..8] CNEN Single End Receiver Enable */
      uint16_t                : 1;
      volatile uint16_t SCKE  : 1; /* [10..10] USB Clock Enable */
      uint16_t                : 5;
    } SYSCFG_b;
  };

  union {
    volatile uint16_t BUSWAIT; /* (@ 0x00000002) CPU Bus Wait Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t BWAIT : 4; /* [3..0] CPU Bus Access Wait Specification BWAIT waits (BWAIT+2 access cycles) */
      uint16_t                : 12;
    } BUSWAIT_b;
  };

  union {
    volatile const uint16_t SYSSTS0; /* (@ 0x00000004) System Configuration Status Register 0 */

    struct TU_ATTR_PACKED {
      volatile const uint16_t LNST   : 2; /* [1..0] USB Data Line Status Monitor */
      volatile const uint16_t IDMON  : 1; /* [2..2] External ID0 Input Pin Monitor */
      uint16_t                       : 2;
      volatile const uint16_t SOFEA  : 1; /* [5..5] SOF Active Monitor While Host Controller Function is Selected. */
      volatile const uint16_t HTACT  : 1; /* [6..6] USB Host Sequencer Status Monitor */
      uint16_t                       : 7;
      volatile const uint16_t OVCMON : 2; /* [15..14] External USB0_OVRCURA/ USB0_OVRCURB Input Pin Monitor */
    } SYSSTS0_b;
  };

  union {
    volatile const uint16_t PLLSTA; /* (@ 0x00000006) PLL Status Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t PLLLOCK : 1; /* [0..0] PLL Lock Flag */
      uint16_t                        : 15;
    } PLLSTA_b;
  };

  union {
    volatile uint16_t DVSTCTR0; /* (@ 0x00000008) Device State Control Register 0 */

    struct TU_ATTR_PACKED {
      volatile const uint16_t RHST : 3; /* [2..0] USB Bus Reset Status */
      uint16_t                     : 1;
      volatile uint16_t UACT       : 1; /* [4..4] USB Bus Enable */
      volatile uint16_t RESUME     : 1; /* [5..5] Resume Output */
      volatile uint16_t USBRST     : 1; /* [6..6] USB Bus Reset Output */
      volatile uint16_t RWUPE      : 1; /* [7..7] Wakeup Detection Enable */
      volatile uint16_t WKUP       : 1; /* [8..8] Wakeup Output */
      volatile uint16_t VBUSEN     : 1; /* [9..9] USB_VBUSEN Output Pin Control */
      volatile uint16_t EXICEN     : 1; /* [10..10] USB_EXICEN Output Pin Control */
      volatile uint16_t HNPBTOA    : 1; /* [11..11] Host Negotiation Protocol (HNP) */
      uint16_t                     : 4;
    } DVSTCTR0_b;
  };
  volatile const uint16_t RESERVED;

  union {
    volatile uint16_t TESTMODE; /* (@ 0x0000000C) USB Test Mode Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t UTST : 4; /* [3..0] Test Mode */
      uint16_t               : 12;
    } TESTMODE_b;
  };
  volatile const uint16_t RESERVED1;
  volatile const uint32_t RESERVED2;

  union {
    volatile uint32_t CFIFO; /* (@ 0x00000014) CFIFO Port Register */

    struct TU_ATTR_PACKED {
      union {
        volatile uint16_t CFIFOL; /* (@ 0x00000014) CFIFO Port Register L */
        volatile uint8_t CFIFOLL; /* (@ 0x00000014) CFIFO Port Register LL */
      };

      union {
        volatile uint16_t CFIFOH; /* (@ 0x00000016) CFIFO Port Register H */

        struct TU_ATTR_PACKED {
          volatile const uint8_t RESERVED3;
          volatile uint8_t CFIFOHH; /* (@ 0x00000017) CFIFO Port Register HH */
        };
      };
    };
  };

  union {
    volatile uint32_t D0FIFO; /* (@ 0x00000018) D0FIFO Port Register */

    struct TU_ATTR_PACKED {
      union {
        volatile uint16_t D0FIFOL; /* (@ 0x00000018) D0FIFO Port Register L */
        volatile uint8_t D0FIFOLL; /* (@ 0x00000018) D0FIFO Port Register LL */
      };

      union {
        volatile uint16_t D0FIFOH; /* (@ 0x0000001A) D0FIFO Port Register H */

        struct TU_ATTR_PACKED {
          volatile const uint8_t RESERVED4;
          volatile uint8_t D0FIFOHH; /* (@ 0x0000001B) D0FIFO Port Register HH */
        };
      };
    };
  };

  union {
    volatile uint32_t D1FIFO; /* (@ 0x0000001C) D1FIFO Port Register */

    struct TU_ATTR_PACKED {
      union {
        volatile uint16_t D1FIFOL; /* (@ 0x0000001C) D1FIFO Port Register L */
        volatile uint8_t D1FIFOLL; /* (@ 0x0000001C) D1FIFO Port Register LL */
      };

      union {
        volatile uint16_t D1FIFOH; /* (@ 0x0000001E) D1FIFO Port Register H */

        struct TU_ATTR_PACKED {
          volatile const uint8_t RESERVED5;
          volatile uint8_t D1FIFOHH; /* (@ 0x0000001F) D1FIFO Port Register HH */
        };
      };
    };
  };

  union {
    volatile uint16_t CFIFOSEL; /* (@ 0x00000020) CFIFO Port Select Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t CURPIPE : 4; /* [3..0] CFIFO Port Access Pipe Specification */
      uint16_t                  : 1;
      volatile uint16_t ISEL    : 1; /* [5..5] CFIFO Port Access Direction When DCP is Selected */
      uint16_t                  : 2;
      volatile uint16_t BIGEND  : 1; /* [8..8] CFIFO Port Endian Control */
      uint16_t                  : 1;
      volatile uint16_t MBW     : 2; /* [11..10] CFIFO Port Access Bit Width */
      uint16_t                  : 2;
      volatile uint16_t REW     : 1; /* [14..14] Buffer Pointer Rewind */
      volatile uint16_t RCNT    : 1; /* [15..15] Read Count Mode */
    } CFIFOSEL_b;
  };

  union {
    volatile uint16_t CFIFOCTR; /* (@ 0x00000022) CFIFO Port Control Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t DTLN : 12; /* [11..0] Receive Data LengthIndicates the length of the receive data. */
      uint16_t                     : 1;
      volatile const uint16_t FRDY : 1;  /* [13..13] FIFO Port Ready */
      volatile uint16_t BCLR       : 1;  /* [14..14] CPU Buffer ClearNote: Only 0 can be read. */
      volatile uint16_t BVAL       : 1;  /* [15..15] Buffer Memory Valid Flag */
    } CFIFOCTR_b;
  };
  volatile const uint32_t RESERVED6;

  union {
    volatile uint16_t D0FIFOSEL; /* (@ 0x00000028) D0FIFO Port Select Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t CURPIPE : 4; /* [3..0] FIFO Port Access Pipe Specification */
      uint16_t                  : 4;
      volatile uint16_t BIGEND  : 1; /* [8..8] FIFO Port Endian Control */
      uint16_t                  : 1;
      volatile uint16_t MBW     : 2; /* [11..10] FIFO Port Access Bit Width */
      volatile uint16_t DREQE   : 1; /* [12..12] DMA/DTC Transfer Request Enable */
      volatile uint16_t DCLRM   : 1; /* [13..13] Auto Buffer Memory Clear Mode Accessed after Specified Pipe Data is Read */
      volatile uint16_t REW     : 1; /* [14..14] Buffer Pointer RewindNote: Only 0 can be read. */
      volatile uint16_t RCNT    : 1; /* [15..15] Read Count Mode */
    } D0FIFOSEL_b;
  };

  union {
    volatile uint16_t D0FIFOCTR; /* (@ 0x0000002A) D0FIFO Port Control Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t DTLN : 12; /* [11..0] Receive Data LengthIndicates the length of the receive data. */
      uint16_t                     : 1;
      volatile const uint16_t FRDY : 1;  /* [13..13] FIFO Port Ready */
      volatile uint16_t BCLR       : 1;  /* [14..14] CPU Buffer ClearNote: Only 0 can be read. */
      volatile uint16_t BVAL       : 1;  /* [15..15] Buffer Memory Valid Flag */
    } D0FIFOCTR_b;
  };

  union {
    volatile uint16_t D1FIFOSEL; /* (@ 0x0000002C) D1FIFO Port Select Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t CURPIPE : 4; /* [3..0] FIFO Port Access Pipe Specification */
      uint16_t                  : 4;
      volatile uint16_t BIGEND  : 1; /* [8..8] FIFO Port Endian Control */
      uint16_t                  : 1;
      volatile uint16_t MBW     : 2; /* [11..10] FIFO Port Access Bit Width */
      volatile uint16_t DREQE   : 1; /* [12..12] DMA/DTC Transfer Request Enable */
      volatile uint16_t DCLRM   : 1; /* [13..13] Auto Buffer Memory Clear Mode Accessed after Specified Pipe Data is Read */
      volatile uint16_t REW     : 1; /* [14..14] Buffer Pointer Rewind */
      volatile uint16_t RCNT    : 1; /* [15..15] Read Count Mode */
    } D1FIFOSEL_b;
  };

  union {
    volatile uint16_t D1FIFOCTR; /* (@ 0x0000002E) D1FIFO Port Control Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t DTLN : 12; /* [11..0] Receive Data LengthIndicates the length of the receive data. */
      uint16_t                     : 1;
      volatile const uint16_t FRDY : 1;  /* [13..13] FIFO Port Ready */
      volatile uint16_t BCLR       : 1;  /* [14..14] CPU Buffer ClearNote: Only 0 can be read. */
      volatile uint16_t BVAL       : 1;  /* [15..15] Buffer Memory Valid Flag */
    } D1FIFOCTR_b;
  };

  union {
    volatile uint16_t INTENB0; /* (@ 0x00000030) Interrupt Enable Register 0 */

    struct TU_ATTR_PACKED {
      uint16_t                : 8;
      volatile uint16_t BRDYE : 1; /* [8..8] Buffer Ready Interrupt Enable */
      volatile uint16_t NRDYE : 1; /* [9..9] Buffer Not Ready Response Interrupt Enable */
      volatile uint16_t BEMPE : 1; /* [10..10] Buffer Empty Interrupt Enable */
      volatile uint16_t CTRE  : 1; /* [11..11] Control Transfer Stage Transition Interrupt Enable */
      volatile uint16_t DVSE  : 1; /* [12..12] Device State Transition Interrupt Enable */
      volatile uint16_t SOFE  : 1; /* [13..13] Frame Number Update Interrupt Enable */
      volatile uint16_t RSME  : 1; /* [14..14] Resume Interrupt Enable */
      volatile uint16_t VBSE  : 1; /* [15..15] VBUS Interrupt Enable */
    } INTENB0_b;
  };

  union {
    volatile uint16_t INTENB1; /* (@ 0x00000032) Interrupt Enable Register 1 */

    struct TU_ATTR_PACKED {
      volatile uint16_t PDDETINTE0 : 1; /* [0..0] PDDETINT0 Detection Interrupt Enable */
      uint16_t                     : 3;
      volatile uint16_t SACKE      : 1; /* [4..4] Setup Transaction Normal Response Interrupt Enable */
      volatile uint16_t SIGNE      : 1; /* [5..5] Setup Transaction Error Interrupt Enable */
      volatile uint16_t EOFERRE    : 1; /* [6..6] EOF Error Detection Interrupt Enable */
               uint16_t            : 1;
      volatile uint16_t LPMENDE    : 1; /*!< [8..8] LPM Transaction End Interrupt Enable                               */
      volatile uint16_t L1RSMENDE  : 1; /*!< [9..9] L1 Resume End Interrupt Enable                                     */
               uint16_t            : 1;
      volatile uint16_t ATTCHE     : 1; /* [11..11] Connection Detection Interrupt Enable */
      volatile uint16_t DTCHE      : 1; /* [12..12] Disconnection Detection Interrupt Enable */
      uint16_t                     : 1;
      volatile uint16_t BCHGE      : 1; /* [14..14] USB Bus Change Interrupt Enable */
      volatile uint16_t OVRCRE     : 1; /* [15..15] Overcurrent Input Change Interrupt Enable */
    } INTENB1_b;
  };
  volatile const uint16_t RESERVED7;

  union {
    volatile uint16_t BRDYENB; /* (@ 0x00000036) BRDY Interrupt Enable Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PIPE0BRDYE : 1; /* [0..0] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE1BRDYE : 1; /* [1..1] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE2BRDYE : 1; /* [2..2] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE3BRDYE : 1; /* [3..3] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE4BRDYE : 1; /* [4..4] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE5BRDYE : 1; /* [5..5] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE6BRDYE : 1; /* [6..6] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE7BRDYE : 1; /* [7..7] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE8BRDYE : 1; /* [8..8] BRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE9BRDYE : 1; /* [9..9] BRDY Interrupt Enable for PIPE */
      uint16_t                     : 6;
    } BRDYENB_b;
  };

  union {
    volatile uint16_t NRDYENB; /* (@ 0x00000038) NRDY Interrupt Enable Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PIPE0NRDYE : 1; /* [0..0] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE1NRDYE : 1; /* [1..1] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE2NRDYE : 1; /* [2..2] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE3NRDYE : 1; /* [3..3] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE4NRDYE : 1; /* [4..4] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE5NRDYE : 1; /* [5..5] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE6NRDYE : 1; /* [6..6] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE7NRDYE : 1; /* [7..7] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE8NRDYE : 1; /* [8..8] NRDY Interrupt Enable for PIPE */
      volatile uint16_t PIPE9NRDYE : 1; /* [9..9] NRDY Interrupt Enable for PIPE */
      uint16_t                     : 6;
    } NRDYENB_b;
  };

  union {
    volatile uint16_t BEMPENB; /* (@ 0x0000003A) BEMP Interrupt Enable Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PIPE0BEMPE : 1; /* [0..0] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE1BEMPE : 1; /* [1..1] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE2BEMPE : 1; /* [2..2] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE3BEMPE : 1; /* [3..3] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE4BEMPE : 1; /* [4..4] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE5BEMPE : 1; /* [5..5] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE6BEMPE : 1; /* [6..6] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE7BEMPE : 1; /* [7..7] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE8BEMPE : 1; /* [8..8] BEMP Interrupt Enable for PIPE */
      volatile uint16_t PIPE9BEMPE : 1; /* [9..9] BEMP Interrupt Enable for PIPE */
      uint16_t                     : 6;
    } BEMPENB_b;
  };

  union {
    volatile uint16_t SOFCFG; /* (@ 0x0000003C) SOF Output Configuration Register */

    struct TU_ATTR_PACKED {
      uint16_t                        : 4;
      volatile const uint16_t EDGESTS : 1; /* [4..4] Edge Interrupt Output Status Monitor */
      volatile uint16_t INTL          : 1; /* [5..5] Interrupt Output Sense Select */
      volatile uint16_t BRDYM         : 1; /* [6..6] BRDY Interrupt Status Clear Timing */
      uint16_t                        : 1;
      volatile uint16_t TRNENSEL      : 1; /* [8..8] Transaction-Enabled Time Select */
      uint16_t                        : 7;
    } SOFCFG_b;
  };

  union {
    volatile uint16_t PHYSET; /* (@ 0x0000003E) PHY Setting Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t DIRPD    : 1; /* [0..0] Power-Down Control */
      volatile uint16_t PLLRESET : 1; /* [1..1] PLL Reset Control */
      uint16_t                   : 1;
      volatile uint16_t CDPEN    : 1; /* [3..3] Charging Downstream Port Enable */
      volatile uint16_t CLKSEL   : 2; /* [5..4] Input System Clock Frequency */
      uint16_t                   : 2;
      volatile uint16_t REPSEL   : 2; /* [9..8] Terminating Resistance Adjustment Cycle */
      uint16_t                   : 1;
      volatile uint16_t REPSTART : 1; /* [11..11] Forcibly Start Terminating Resistance Adjustment */
      uint16_t                   : 3;
      volatile uint16_t HSEB     : 1; /* [15..15] CL-Only Mode */
    } PHYSET_b;
  };

  union {
    volatile uint16_t INTSTS0; /* (@ 0x00000040) Interrupt Status Register 0 */

    struct TU_ATTR_PACKED {
      volatile const uint16_t CTSQ  : 3; /* [2..0] Control Transfer Stage */
      volatile uint16_t VALID       : 1; /* [3..3] USB Request Reception */
      volatile const uint16_t DVSQ  : 3; /* [6..4] Device State */
      volatile const uint16_t VBSTS : 1; /* [7..7] VBUS Input Status */
      volatile const uint16_t BRDY  : 1; /* [8..8] Buffer Ready Interrupt Status */
      volatile const uint16_t NRDY  : 1; /* [9..9] Buffer Not Ready Interrupt Status */
      volatile const uint16_t BEMP  : 1; /* [10..10] Buffer Empty Interrupt Status */
      volatile uint16_t CTRT        : 1; /* [11..11] Control Transfer Stage Transition Interrupt Status */
      volatile uint16_t DVST        : 1; /* [12..12] Device State Transition Interrupt Status */
      volatile uint16_t SOFR        : 1; /* [13..13] Frame Number Refresh Interrupt Status */
      volatile uint16_t RESM        : 1; /* [14..14] Resume Interrupt Status */
      volatile uint16_t VBINT       : 1; /* [15..15] VBUS Interrupt Status */
    } INTSTS0_b;
  };

  union {
    volatile uint16_t INTSTS1; /* (@ 0x00000042) Interrupt Status Register 1 */

    struct TU_ATTR_PACKED {
      volatile uint16_t PDDETINT0 : 1; /* [0..0] PDDET0 Detection Interrupt Status */
      uint16_t                    : 3;
      volatile uint16_t SACK      : 1; /* [4..4] Setup Transaction Normal Response Interrupt Status */
      volatile uint16_t SIGN      : 1; /* [5..5] Setup Transaction Error Interrupt Status */
      volatile uint16_t EOFERR    : 1; /* [6..6] EOF Error Detection Interrupt Status */
      uint16_t                    : 1;
      volatile uint16_t LPMEND    : 1; /* [8..8] LPM Transaction End Interrupt Status */
      volatile uint16_t L1RSMEND  : 1; /* [9..9] L1 Resume End Interrupt Status */
      uint16_t                    : 1;
      volatile uint16_t ATTCH     : 1; /* [11..11] ATTCH Interrupt Status */
      volatile uint16_t DTCH      : 1; /* [12..12] USB Disconnection Detection Interrupt Status */
      uint16_t                    : 1;
      volatile uint16_t BCHG      : 1; /* [14..14] USB Bus Change Interrupt Status */
      volatile uint16_t OVRCR     : 1; /* [15..15] Overcurrent Input Change Interrupt Status */
    } INTSTS1_b;
  };
  volatile const uint16_t RESERVED8;

  union {
    volatile uint16_t BRDYSTS; /* (@ 0x00000046) BRDY Interrupt Status Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PIPE0BRDY : 1; /* [0..0] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE1BRDY : 1; /* [1..1] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE2BRDY : 1; /* [2..2] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE3BRDY : 1; /* [3..3] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE4BRDY : 1; /* [4..4] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE5BRDY : 1; /* [5..5] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE6BRDY : 1; /* [6..6] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE7BRDY : 1; /* [7..7] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE8BRDY : 1; /* [8..8] BRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE9BRDY : 1; /* [9..9] BRDY Interrupt Status for PIPE */
      uint16_t                    : 6;
    } BRDYSTS_b;
  };

  union {
    volatile uint16_t NRDYSTS; /* (@ 0x00000048) NRDY Interrupt Status Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PIPE0NRDY : 1; /* [0..0] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE1NRDY : 1; /* [1..1] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE2NRDY : 1; /* [2..2] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE3NRDY : 1; /* [3..3] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE4NRDY : 1; /* [4..4] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE5NRDY : 1; /* [5..5] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE6NRDY : 1; /* [6..6] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE7NRDY : 1; /* [7..7] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE8NRDY : 1; /* [8..8] NRDY Interrupt Status for PIPE */
      volatile uint16_t PIPE9NRDY : 1; /* [9..9] NRDY Interrupt Status for PIPE */
      uint16_t                    : 6;
    } NRDYSTS_b;
  };

  union {
    volatile uint16_t BEMPSTS; /* (@ 0x0000004A) BEMP Interrupt Status Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PIPE0BEMP : 1; /* [0..0] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE1BEMP : 1; /* [1..1] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE2BEMP : 1; /* [2..2] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE3BEMP : 1; /* [3..3] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE4BEMP : 1; /* [4..4] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE5BEMP : 1; /* [5..5] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE6BEMP : 1; /* [6..6] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE7BEMP : 1; /* [7..7] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE8BEMP : 1; /* [8..8] BEMP Interrupt Status for PIPE */
      volatile uint16_t PIPE9BEMP : 1; /* [9..9] BEMP Interrupt Status for PIPE */
      uint16_t                    : 6;
    } BEMPSTS_b;
  };

  union {
    volatile uint16_t FRMNUM; /* (@ 0x0000004C) Frame Number Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t FRNM : 11; /* [10..0] Frame NumberLatest frame number */
      uint16_t                     : 3;
      volatile uint16_t CRCE       : 1; /* [14..14] Receive Data Error */
      volatile uint16_t OVRN       : 1; /* [15..15] Overrun/Underrun Detection Status */
    } FRMNUM_b;
  };

  union {
    volatile uint16_t UFRMNUM; /* (@ 0x0000004E) uFrame Number Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t UFRNM : 3; /* [2..0] MicroframeIndicate the microframe number. */
      uint16_t                      : 12;
      volatile uint16_t DVCHG       : 1; /* [15..15] Device State Change */
    } UFRMNUM_b;
  };

  union {
    volatile uint16_t USBADDR; /* (@ 0x00000050) USB Address Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t USBADDR : 7; /* [6..0] USB Address In device controller mode */
      uint16_t                        : 1;
      volatile uint16_t STSRECOV0     : 3; /* [10..8] Status Recovery */
      uint16_t                        : 5;
    } USBADDR_b;
  };
  volatile const uint16_t RESERVED9;

  union {
    volatile uint16_t USBREQ; /* (@ 0x00000054) USB Request Type Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t BMREQUESTTYPE : 8; /* [7..0] Request TypeThese bits store the USB request bmRequestType value. */
      volatile uint16_t BREQUEST      : 8; /* [15..8] RequestThese bits store the USB request bRequest value. */
    } USBREQ_b;
  };

  union {
    volatile uint16_t USBVAL; /* (@ 0x00000056) USB Request Value Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t WVALUE : 16; /* [15..0] ValueThese bits store the USB request Value value. */
    } USBVAL_b;
  };

  union {
    volatile uint16_t USBINDX; /* (@ 0x00000058) USB Request Index Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t WINDEX : 16; /* [15..0] IndexThese bits store the USB request wIndex value. */
    } USBINDX_b;
  };

  union {
    volatile uint16_t USBLENG; /* (@ 0x0000005A) USB Request Length Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t WLENGTH : 16; /* [15..0] LengthThese bits store the USB request wLength value. */
    } USBLENG_b;
  };

  union {
    volatile uint16_t DCPCFG; /* (@ 0x0000005C) DCP Configuration Register */

    struct TU_ATTR_PACKED {
      uint16_t                 : 4;
      volatile uint16_t DIR    : 1; /* [4..4] Transfer Direction */
      uint16_t                 : 2;
      volatile uint16_t SHTNAK : 1; /* [7..7] Pipe Disabled at End of Transfer */
      volatile uint16_t CNTMD  : 1; /* [8..8] Continuous Transfer Mode */
      uint16_t                 : 7;
    } DCPCFG_b;
  };

  union {
    volatile uint16_t DCPMAXP; /* (@ 0x0000005E) DCP Maximum Packet Size Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t MXPS   : 7; /* [6..0] Maximum Packet Size */
      uint16_t                 : 5;
      volatile uint16_t DEVSEL : 4; /* [15..12] Device Select */
    } DCPMAXP_b;
  };

  union {
    volatile uint16_t DCPCTR; /* (@ 0x00000060) DCP Control Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PID         : 2; /* [1..0] Response PID */
      volatile uint16_t CCPL        : 1; /* [2..2] Control Transfer End Enable */
      uint16_t                      : 2;
      volatile const uint16_t PBUSY : 1; /* [5..5] Pipe Busy */
      volatile const uint16_t SQMON : 1; /* [6..6] Sequence Toggle Bit Monitor */
      volatile uint16_t SQSET       : 1; /* [7..7] Sequence Toggle Bit Set */
      volatile uint16_t SQCLR       : 1; /* [8..8] Sequence Toggle Bit Clear */
      uint16_t                      : 2;
      volatile uint16_t SUREQCLR    : 1; /* [11..11] SUREQ Bit Clear */
      volatile uint16_t CSSTS       : 1; /* [12..12] Split Transaction COMPLETE SPLIT(CSPLIT) Status                  */
      volatile uint16_t CSCLR       : 1; /* [13..13] Split Transaction CSPLIT Status Clear                            */
      volatile uint16_t SUREQ       : 1; /* [14..14] Setup Token Transmission */
      volatile const uint16_t BSTS  : 1; /* [15..15] Buffer Status */
    } DCPCTR_b;
  };
  volatile const uint16_t RESERVED10;

  union {
    volatile uint16_t PIPESEL; /* (@ 0x00000064) Pipe Window Select Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PIPESEL : 4; /* [3..0] Pipe Window Select */
      uint16_t                  : 12;
    } PIPESEL_b;
  };
  volatile const uint16_t RESERVED11;

  union {
    volatile uint16_t PIPECFG;      /* (@ 0x00000068) Pipe Configuration Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t EPNUM  : 4; /* [3..0] Endpoint Number */
      volatile uint16_t DIR    : 1; /* [4..4] Transfer Direction */
      uint16_t                 : 2;
      volatile uint16_t SHTNAK : 1; /* [7..7] Pipe Disabled at End of Transfer */
      volatile uint16_t CNTMD  : 1; /* [8..8] Continuous Transfer Mode                                           */
      volatile uint16_t DBLB   : 1; /* [9..9] Double Buffer Mode */
      volatile uint16_t BFRE   : 1; /* [10..10] BRDY Interrupt Operation Specification */
      uint16_t                 : 3;
      volatile uint16_t TYPE   : 2; /* [15..14] Transfer Type */
    } PIPECFG_b;
  };

  union {
    volatile uint16_t PIPEBUF;         /*!< (@ 0x0000006A) Pipe Buffer Register                                       */

    struct {
      volatile uint16_t BUFNMB  : 8; // [7..0] Buffer NumberThese bits specify the FIFO buffer number of the selected pipe (04h to 87h)
      uint16_t                  : 2;
      volatile uint16_t BUFSIZE : 5; /*!< [14..10] Buffer Size 00h: 64 bytes 01h: 128 bytes : 1Fh: 2 Kbytes         */
      uint16_t                  : 1;
    } PIPEBUF_b;
  };

  union {
    volatile uint16_t PIPEMAXP; /* (@ 0x0000006C) Pipe Maximum Packet Size Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t MXPS   : 11; /* [10..0] Maximum Packet Size */
      uint16_t                 : 1;
      volatile uint16_t DEVSEL : 4; /* [15..12] Device Select */
    } PIPEMAXP_b;
  };

  union {
    volatile uint16_t PIPEPERI; /* (@ 0x0000006E) Pipe Cycle Control Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t IITV : 3; /* [2..0] Interval Error Detection Interval */
      uint16_t               : 9;
      volatile uint16_t IFIS : 1; /* [12..12] Isochronous IN Buffer Flush */
      uint16_t               : 3;
    } PIPEPERI_b;
  };

  union {
    volatile uint16_t PIPE_CTR[9];        /* (@ 0x00000070) Pipe [0..8] Control Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t PID          : 2; /* [1..0] Response PID */
      uint16_t                       : 3;
      volatile const uint16_t PBUSY  : 1; /* [5..5] Pipe Busy */
      volatile const uint16_t SQMON  : 1; /* [6..6] Sequence Toggle Bit Confirmation */
      volatile uint16_t SQSET        : 1; /* [7..7] Sequence Toggle Bit Set */
      volatile uint16_t SQCLR        : 1; /* [8..8] Sequence Toggle Bit Clear */
      volatile uint16_t ACLRM        : 1; /* [9..9] Auto Buffer Clear Mode */
      volatile uint16_t ATREPM       : 1; /* [10..10] Auto Response Mode */
      uint16_t                       : 1;
      volatile const uint16_t CSSTS  : 1; /* [12..12] CSSTS Status */
      volatile uint16_t CSCLR        : 1; /* [13..13] CSPLIT Status Clear */
      volatile const uint16_t INBUFM : 1; /* [14..14] Transmit Buffer Monitor */
      volatile const uint16_t BSTS   : 1; /* [15..15] Buffer Status */
    } PIPE_CTR_b[9];
  };
  volatile const uint16_t RESERVED13;
  volatile const uint32_t RESERVED14[3];
  volatile RUSB2_PIPE_TR_t PIPE_TR[5]; /* (@ 0x00000090) Pipe Transaction Counter Registers */
  volatile const uint32_t RESERVED15[3];

  union {
    volatile uint16_t USBBCCTRL0;             /* (@ 0x000000B0) BC Control Register 0 */

    struct TU_ATTR_PACKED {
      volatile uint16_t RPDME0           : 1; /* [0..0] D- Pin Pull-Down Control */
      volatile uint16_t IDPSRCE0         : 1; /* [1..1] D+ Pin IDPSRC Output Control */
      volatile uint16_t IDMSINKE0        : 1; /* [2..2] D- Pin 0.6 V Input Detection (Comparator and Sink) Control */
      volatile uint16_t VDPSRCE0         : 1; /* [3..3] D+ Pin VDPSRC (0.6 V) Output Control */
      volatile uint16_t IDPSINKE0        : 1; /* [4..4] D+ Pin 0.6 V Input Detection (Comparator and Sink) Control */
      volatile uint16_t VDMSRCE0         : 1; /* [5..5] D- Pin VDMSRC (0.6 V) Output Control */
      uint16_t                           : 1;
      volatile uint16_t BATCHGE0         : 1; /* [7..7] BC (Battery Charger) Function Ch0 General Enable Control */
      volatile const uint16_t CHGDETSTS0 : 1; /* [8..8] D- Pin 0.6 V Input Detection Status */
      volatile const uint16_t PDDETSTS0  : 1; /* [9..9] D+ Pin 0.6 V Input Detection Status */
      uint16_t                           : 6;
    } USBBCCTRL0_b;
  };
  volatile const uint16_t RESERVED16;
  volatile const uint32_t RESERVED17[4];

  union {
    volatile uint16_t UCKSEL; /* (@ 0x000000C4) USB Clock Selection Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t UCKSELC : 1; /* [0..0] USB Clock Selection */
      uint16_t                  : 15;
    } UCKSEL_b;
  };
  volatile const uint16_t RESERVED18;
  volatile const uint32_t RESERVED19;

  union {
    volatile uint16_t USBMC; /* (@ 0x000000CC) USB Module Control Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t VDDUSBE : 1; /* [0..0] USB Reference Power Supply Circuit On/Off Control */
      uint16_t                  : 6;
      volatile uint16_t VDCEN   : 1; /* [7..7] USB Regulator On/Off Control */
      uint16_t                  : 8;
    } USBMC_b;
  };
  volatile const uint16_t RESERVED20;

  union {
    volatile uint16_t DEVADD[10]; /* (@ 0x000000D0) Device Address Configuration Register */

    struct TU_ATTR_PACKED {
      uint16_t                  : 6;
      volatile uint16_t USBSPD  : 2; /* [7..6] Transfer Speed of Communication Target Device */
      volatile uint16_t HUBPORT : 3; /* [10..8] Communication Target Connecting Hub Port */
      volatile uint16_t UPPHUB  : 4; /* [14..11] Communication Target Connecting Hub Register */
      uint16_t                  : 1;
    } DEVADD_b[10];
  };
  volatile const uint32_t RESERVED21[3];

  union {
    volatile uint32_t PHYSLEW; /* (@ 0x000000F0) PHY Cross Point Adjustment Register */

    struct TU_ATTR_PACKED {
      volatile uint32_t SLEWR00 : 1; /* [0..0] Receiver Cross Point Adjustment 00 */
      volatile uint32_t SLEWR01 : 1; /* [1..1] Receiver Cross Point Adjustment 01 */
      volatile uint32_t SLEWF00 : 1; /* [2..2] Receiver Cross Point Adjustment 00 */
      volatile uint32_t SLEWF01 : 1; /* [3..3] Receiver Cross Point Adjustment 01 */
      uint32_t                  : 28;
    } PHYSLEW_b;
  };
  volatile const uint32_t RESERVED22[3];

  union {
    volatile uint16_t LPCTRL; /* (@ 0x00000100) Low Power Control Register */

    struct TU_ATTR_PACKED {
      uint16_t                : 7;
      volatile uint16_t HWUPM : 1; /* [7..7] Resume Return Mode Setting */
      uint16_t                : 8;
    } LPCTRL_b;
  };

  union {
    volatile uint16_t LPSTS; /* (@ 0x00000102) Low Power Status Register */

    struct TU_ATTR_PACKED {
      uint16_t                   : 14;
      volatile uint16_t SUSPENDM : 1; /* [14..14] UTMI SuspendM Control */
      uint16_t                   : 1;
    } LPSTS_b;
  };
  volatile const uint32_t RESERVED23[15];

  union {
    volatile uint16_t BCCTRL; /* (@ 0x00000140) Battery Charging Control Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t IDPSRCE         : 1; /* [0..0] IDPSRC Control */
      volatile uint16_t IDMSINKE        : 1; /* [1..1] IDMSINK Control */
      volatile uint16_t VDPSRCE         : 1; /* [2..2] VDPSRC Control */
      volatile uint16_t IDPSINKE        : 1; /* [3..3] IDPSINK Control */
      volatile uint16_t VDMSRCE         : 1; /* [4..4] VDMSRC Control */
      volatile uint16_t DCPMODE         : 1; /* [5..5] DCP Mode Control */
      uint16_t                          : 2;
      volatile const uint16_t CHGDETSTS : 1; /* [8..8] CHGDET Status */
      volatile const uint16_t PDDETSTS  : 1; /* [9..9] PDDET Status */
      uint16_t                          : 6;
    } BCCTRL_b;
  };
  volatile const uint16_t RESERVED24;

  union {
    volatile uint16_t PL1CTRL1; /* (@ 0x00000144) Function L1 Control Register 1 */

    struct TU_ATTR_PACKED {
      volatile uint16_t L1RESPEN   : 1; /* [0..0] L1 Response Enable */
      volatile uint16_t L1RESPMD   : 2; /* [2..1] L1 Response Mode */
      volatile uint16_t L1NEGOMD   : 1; /* [3..3] L1 Response Negotiation Control. */
      volatile const uint16_t DVSQ : 4; /* [7..4] DVSQ Extension.DVSQ[3] is Mirror of DVSQ[2:0] in INTSTS0. */
      volatile uint16_t HIRDTHR    : 4; /* [11..8] L1 Response Negotiation Threshold Value */
      uint16_t                     : 2;
      volatile uint16_t L1EXTMD    : 1; /* [14..14] PHY Control Mode at L1 Return */
      uint16_t                     : 1;
    } PL1CTRL1_b;
  };

  union {
    volatile uint16_t PL1CTRL2; /* (@ 0x00000146) Function L1 Control Register 2 */

    struct TU_ATTR_PACKED {
      uint16_t                  : 8;
      volatile uint16_t HIRDMON : 4; /* [11..8] HIRD Value Monitor */
      volatile uint16_t RWEMON  : 1;  /* [12..12] RWE Value Monitor */
      uint16_t                  : 3;
    } PL1CTRL2_b;
  };

  union {
    volatile uint16_t HL1CTRL1; /* (@ 0x00000148) Host L1 Control Register 1 */

    struct TU_ATTR_PACKED {
      volatile uint16_t L1REQ          : 1;       /* [0..0] L1 Transition Request */
      volatile const uint16_t L1STATUS : 2; /* [2..1] L1 Request Completion Status */
      uint16_t                         : 13;
    } HL1CTRL1_b;
  };

  union {
    volatile uint16_t HL1CTRL2; /* (@ 0x0000014A) Host L1 Control Register 2 */

    struct TU_ATTR_PACKED {
      volatile uint16_t L1ADDR : 4; /* [3..0] LPM Token DeviceAddress */
      uint16_t                 : 4;
      volatile uint16_t HIRD   : 4; /* [11..8] LPM Token HIRD */
      volatile uint16_t L1RWE  : 1; /* [12..12] LPM Token L1 Remote Wake Enable */
      uint16_t                 : 2;
      volatile uint16_t BESL   : 1; /* [15..15] BESL & Alternate HIRD */
    } HL1CTRL2_b;
  };

  volatile uint32_t RESERVED25_1;

  union {
    volatile uint16_t PHYTRIM1;          /*!< (@ 0x00000150) PHY Timing Register 1                                      */

    struct {
      volatile uint16_t DRISE      : 2; /*!< [1..0] FS/LS Rising-Edge Output Waveform Adjustment Function              */
      volatile uint16_t DFALL      : 2; /*!< [3..2] FS/LS Falling-Edge Output Waveform Adjustment Function             */
               uint16_t            : 3;
      volatile uint16_t PCOMPENB   : 1; /*!< [7..7] PVDD Start-up Detection                                            */
      volatile uint16_t HSIUP      : 4; /*!< [11..8] HS Output Level Setting                                           */
      volatile uint16_t IMPOFFSET  : 3; /*!< [14..12] terminating resistance offset value setting.Offset value for adjusting the terminating resistance.                           */
               uint16_t            : 1;
    } PHYTRIM1_b;
  };

  union {
    volatile uint16_t PHYTRIM2;         /*!< (@ 0x00000152) PHY Timing Register 2                                      */

    struct {
      volatile  uint16_t SQU      : 4; /*!< [3..0] Squelch Detection Level                                            */
      uint16_t                    : 3;
      volatile  uint16_t HSRXENMO : 1; /*!< [7..7] HS Receive Enable Control Mode                                     */
      volatile  uint16_t PDR      : 2; /*!< [9..8] HS Output Adjustment Function                                      */
      uint16_t                    : 2;
      volatile  uint16_t DIS      : 3; /*!< [14..12] Disconnect Detection Level                                       */
      uint16_t                    : 1;
    } PHYTRIM2_b;
  };
  volatile uint32_t RESERVED25_2[3];

  union {
    volatile const uint32_t DPUSR0R; /* (@ 0x00000160) Deep Standby USB Transceiver Control/Pin Monitor Register */

    struct TU_ATTR_PACKED {
      uint32_t                         : 20;
      volatile const uint32_t DOVCAHM  : 1; /* [20..20] OVRCURA InputIndicates OVRCURA input signal on the HS side of USB port. */
      volatile const uint32_t DOVCBHM  : 1; /* [21..21] OVRCURB InputIndicates OVRCURB input signal on the HS side of USB port. */
      uint32_t                         : 1;
      volatile const uint32_t DVBSTSHM : 1; /* [23..23] VBUS InputIndicates VBUS input signal on the HS side of USB port. */
      uint32_t                         : 8;
    } DPUSR0R_b;
  };

  union {
    volatile uint32_t DPUSR1R; /* (@ 0x00000164) Deep Standby USB Suspend/Resume Interrupt Register */

    struct TU_ATTR_PACKED {
      uint32_t                        : 4;
      volatile uint32_t DOVCAHE       : 1; /* [4..4] OVRCURA Interrupt Enable Clear */
      volatile uint32_t DOVCBHE       : 1; /* [5..5] OVRCURB Interrupt Enable Clear */
      uint32_t                        : 1;
      volatile uint32_t DVBSTSHE      : 1; /* [7..7] VBUS Interrupt Enable/Clear */
      uint32_t                        : 12;
      volatile const uint32_t DOVCAH  : 1; /* [20..20] Indication of Return from OVRCURA Interrupt Source */
      volatile const uint32_t DOVCBH  : 1; /* [21..21] Indication of Return from OVRCURB Interrupt Source */
      uint32_t                        : 1;
      volatile const uint32_t DVBSTSH : 1; /* [23..23] Indication of Return from VBUS Interrupt Source */
      uint32_t                        : 8;
    } DPUSR1R_b;
  };

  union {
    volatile uint16_t DPUSR2R; /* (@ 0x00000168) Deep Standby USB Suspend/Resume Interrupt Register */

    struct TU_ATTR_PACKED {
      volatile const uint16_t DPINT : 1; /* [0..0] Indication of Return from DP Interrupt Source */
      volatile const uint16_t DMINT : 1; /* [1..1] Indication of Return from DM Interrupt Source */
      uint16_t                      : 2;
      volatile const uint16_t DPVAL : 1; /* [4..4] DP InputIndicates DP input signal on the HS side of USB port. */
      volatile const uint16_t DMVAL : 1; /* [5..5] DM InputIndicates DM input signal on the HS side of USB port. */
      uint16_t                      : 2;
      volatile uint16_t DPINTE      : 1; /* [8..8] DP Interrupt Enable Clear */
      volatile uint16_t DMINTE      : 1; /* [9..9] DM Interrupt Enable Clear */
      uint16_t                      : 6;
    } DPUSR2R_b;
  };

  union {
    volatile uint16_t DPUSRCR; /* (@ 0x0000016A) Deep Standby USB Suspend/Resume Command Register */

    struct TU_ATTR_PACKED {
      volatile uint16_t FIXPHY   : 1; /* [0..0] USB Transceiver Control Fix */
      volatile uint16_t FIXPHYPD : 1; /* [1..1] USB Transceiver Control Fix for PLL */
      uint16_t                   : 14;
    } DPUSRCR_b;
  };
  volatile const uint32_t RESERVED26[165];

  union {
    volatile uint32_t
      DPUSR0R_FS; /* (@ 0x00000400) Deep Software Standby USB Transceiver Control/Pin Monitor Register */

    struct TU_ATTR_PACKED {
      volatile uint32_t SRPC0         : 1; /* [0..0] USB Single End Receiver Control */
      volatile uint32_t RPUE0         : 1; /* [1..1] DP Pull-Up Resistor Control */
      uint32_t                        : 1;
      volatile uint32_t DRPD0         : 1; /* [3..3] D+/D- Pull-Down Resistor Control */
      volatile uint32_t FIXPHY0       : 1; /* [4..4] USB Transceiver Output Fix */
      uint32_t                        : 11;
      volatile const uint32_t DP0     : 1; /* [16..16] USB0 D+ InputIndicates the D+ input signal of the USB. */
      volatile const uint32_t DM0     : 1; /* [17..17] USB D-InputIndicates the D- input signal of the USB. */
      uint32_t                        : 2;
      volatile const uint32_t DOVCA0  : 1; /* [20..20] USB OVRCURA InputIndicates the OVRCURA input signal of the USB. */
      volatile const uint32_t DOVCB0  : 1; /* [21..21] USB OVRCURB InputIndicates the OVRCURB input signal of the USB. */
      uint32_t                        : 1;
      volatile const uint32_t DVBSTS0 : 1; /* [23..23] USB VBUS InputIndicates the VBUS input signal of the USB. */
      uint32_t                        : 8;
    } DPUSR0R_FS_b;
  };

  union {
    volatile uint32_t DPUSR1R_FS; /* (@ 0x00000404) Deep Software Standby USB Suspend/Resume Interrupt Register */

    struct TU_ATTR_PACKED {
      volatile uint32_t DPINTE0        : 1; /* [0..0] USB DP Interrupt Enable/Clear */
      volatile uint32_t DMINTE0        : 1; /* [1..1] USB DM Interrupt Enable/Clear */
      uint32_t                         : 2;
      volatile uint32_t DOVRCRAE0      : 1; /* [4..4] USB OVRCURA Interrupt Enable/Clear */
      volatile uint32_t DOVRCRBE0      : 1; /* [5..5] USB OVRCURB Interrupt Enable/Clear */
      uint32_t                         : 1;
      volatile uint32_t DVBSE0         : 1; /* [7..7] USB VBUS Interrupt Enable/Clear */
      uint32_t                         : 8;
      volatile const uint32_t DPINT0   : 1; /* [16..16] USB DP Interrupt Source Recovery */
      volatile const uint32_t DMINT0   : 1; /* [17..17] USB DM Interrupt Source Recovery */
      uint32_t                         : 2;
      volatile const uint32_t DOVRCRA0 : 1; /* [20..20] USB OVRCURA Interrupt Source Recovery */
      volatile const uint32_t DOVRCRB0 : 1; /* [21..21] USB OVRCURB Interrupt Source Recovery */
      uint32_t                         : 1;
      volatile const uint32_t DVBINT0  : 1; /* [23..23] USB VBUS Interrupt Source Recovery */
      uint32_t                         : 8;
    } DPUSR1R_FS_b;
  };
} rusb2_reg_t; /* Size = 1032 (0x408) */

TU_ATTR_PACKED_END /* End of definition of packed structs (used by the CCRX toolchain) */
TU_ATTR_BIT_FIELD_ORDER_END

/*--------------------------------------------------------------------*/
/* Register Bit Definitions                                           */
/*--------------------------------------------------------------------*/

// PIPE_TR
// E
#define RUSB2_PIPE_TR_E_TRENB_Pos       (9UL)        /* TRENB (Bit 9) */
#define RUSB2_PIPE_TR_E_TRENB_Msk       (0x200UL)    /* TRENB (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_TR_E_TRCLR_Pos       (8UL)        /* TRCLR (Bit 8) */
#define RUSB2_PIPE_TR_E_TRCLR_Msk       (0x100UL)    /* TRCLR (Bitfield-Mask: 0x01) */

// N
#define RUSB2_PIPE_TR_N_TRNCNT_Pos      (0UL)        /* TRNCNT (Bit 0) */
#define RUSB2_PIPE_TR_N_TRNCNT_Msk      (0xffffUL)   /* TRNCNT (Bitfield-Mask: 0xffff) */

// Core Registers

// SYSCFG
#define RUSB2_SYSCFG_SCKE_Pos           (10UL)       /* SCKE (Bit 10) */
#define RUSB2_SYSCFG_SCKE_Msk           (0x400UL)    /* SCKE (Bitfield-Mask: 0x01) */
#define RUSB2_SYSCFG_CNEN_Pos           (8UL)        /* CNEN (Bit 8) */
#define RUSB2_SYSCFG_CNEN_Msk           (0x100UL)    /* CNEN (Bitfield-Mask: 0x01) */
#define RUSB2_SYSCFG_HSE_Pos            (7UL)        /*!< HSE (Bit 7)                                           */
#define RUSB2_SYSCFG_HSE_Msk            (0x80UL)     /*!< HSE (Bitfield-Mask: 0x01)                             */
#define RUSB2_SYSCFG_DCFM_Pos           (6UL)        /* DCFM (Bit 6) */
#define RUSB2_SYSCFG_DCFM_Msk           (0x40UL)     /* DCFM (Bitfield-Mask: 0x01) */
#define RUSB2_SYSCFG_DRPD_Pos           (5UL)        /* DRPD (Bit 5) */
#define RUSB2_SYSCFG_DRPD_Msk           (0x20UL)     /* DRPD (Bitfield-Mask: 0x01) */
#define RUSB2_SYSCFG_DPRPU_Pos          (4UL)        /* DPRPU (Bit 4) */
#define RUSB2_SYSCFG_DPRPU_Msk          (0x10UL)     /* DPRPU (Bitfield-Mask: 0x01) */
#define RUSB2_SYSCFG_DMRPU_Pos          (3UL)        /* DMRPU (Bit 3) */
#define RUSB2_SYSCFG_DMRPU_Msk          (0x8UL)      /* DMRPU (Bitfield-Mask: 0x01) */
#define RUSB2_SYSCFG_USBE_Pos           (0UL)        /* USBE (Bit 0) */
#define RUSB2_SYSCFG_USBE_Msk           (0x1UL)      /* USBE (Bitfield-Mask: 0x01) */

// BUSWAIT
#define RUSB2_BUSWAIT_BWAIT_Pos         (0UL)        /* BWAIT (Bit 0) */
#define RUSB2_BUSWAIT_BWAIT_Msk         (0xfUL)      /* BWAIT (Bitfield-Mask: 0x0f) */

// SYSSTS0
#define RUSB2_SYSSTS0_OVCMON_Pos        (14UL)       /* OVCMON (Bit 14) */
#define RUSB2_SYSSTS0_OVCMON_Msk        (0xc000UL)   /* OVCMON (Bitfield-Mask: 0x03) */
#define RUSB2_SYSSTS0_HTACT_Pos         (6UL)        /* HTACT (Bit 6) */
#define RUSB2_SYSSTS0_HTACT_Msk         (0x40UL)     /* HTACT (Bitfield-Mask: 0x01) */
#define RUSB2_SYSSTS0_SOFEA_Pos         (5UL)        /* SOFEA (Bit 5) */
#define RUSB2_SYSSTS0_SOFEA_Msk         (0x20UL)     /* SOFEA (Bitfield-Mask: 0x01) */
#define RUSB2_SYSSTS0_IDMON_Pos         (2UL)        /* IDMON (Bit 2) */
#define RUSB2_SYSSTS0_IDMON_Msk         (0x4UL)      /* IDMON (Bitfield-Mask: 0x01) */
#define RUSB2_SYSSTS0_LNST_Pos          (0UL)        /* LNST (Bit 0) */
#define RUSB2_SYSSTS0_LNST_Msk          (0x3UL)      /* LNST (Bitfield-Mask: 0x03) */

// PLLSTA
#define RUSB2_PLLSTA_PLLLOCK_Pos        (0UL)        /* PLLLOCK (Bit 0) */
#define RUSB2_PLLSTA_PLLLOCK_Msk        (0x1UL)      /* PLLLOCK (Bitfield-Mask: 0x01) */

// DVSTCTR0
#define RUSB2_DVSTCTR0_HNPBTOA_Pos      (11UL)       /* HNPBTOA (Bit 11) */
#define RUSB2_DVSTCTR0_HNPBTOA_Msk      (0x800UL)    /* HNPBTOA (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_EXICEN_Pos       (10UL)       /* EXICEN (Bit 10) */
#define RUSB2_DVSTCTR0_EXICEN_Msk       (0x400UL)    /* EXICEN (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_VBUSEN_Pos       (9UL)        /* VBUSEN (Bit 9) */
#define RUSB2_DVSTCTR0_VBUSEN_Msk       (0x200UL)    /* VBUSEN (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_WKUP_Pos         (8UL)        /* WKUP (Bit 8) */
#define RUSB2_DVSTCTR0_WKUP_Msk         (0x100UL)    /* WKUP (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_RWUPE_Pos        (7UL)        /* RWUPE (Bit 7) */
#define RUSB2_DVSTCTR0_RWUPE_Msk        (0x80UL)     /* RWUPE (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_USBRST_Pos       (6UL)        /* USBRST (Bit 6) */
#define RUSB2_DVSTCTR0_USBRST_Msk       (0x40UL)     /* USBRST (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_RESUME_Pos       (5UL)        /* RESUME (Bit 5) */
#define RUSB2_DVSTCTR0_RESUME_Msk       (0x20UL)     /* RESUME (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_UACT_Pos         (4UL)        /* UACT (Bit 4) */
#define RUSB2_DVSTCTR0_UACT_Msk         (0x10UL)     /* UACT (Bitfield-Mask: 0x01) */
#define RUSB2_DVSTCTR0_RHST_Pos         (0UL)        /* RHST (Bit 0) */
#define RUSB2_DVSTCTR0_RHST_Msk         (0x7UL)      /* RHST (Bitfield-Mask: 0x07) */

// TESTMODE
#define RUSB2_TESTMODE_UTST_Pos         (0UL)        /* UTST (Bit 0) */
#define RUSB2_TESTMODE_UTST_Msk         (0xfUL)      /* UTST (Bitfield-Mask: 0x0f) */

// CFIFOSEL
#define RUSB2_CFIFOSEL_RCNT_Pos         (15UL)       /* RCNT (Bit 15) */
#define RUSB2_CFIFOSEL_RCNT_Msk         (0x8000UL)   /* RCNT (Bitfield-Mask: 0x01) */
#define RUSB2_CFIFOSEL_REW_Pos          (14UL)       /* REW (Bit 14) */
#define RUSB2_CFIFOSEL_REW_Msk          (0x4000UL)   /* REW (Bitfield-Mask: 0x01) */
#define RUSB2_CFIFOSEL_MBW_Pos          (10UL)       /* MBW (Bit 10) */
#define RUSB2_CFIFOSEL_MBW_Msk          (0xc00UL)    /* MBW (Bitfield-Mask: 0x03) */
#define RUSB2_CFIFOSEL_BIGEND_Pos       (8UL)        /* BIGEND (Bit 8) */
#define RUSB2_CFIFOSEL_BIGEND_Msk       (0x100UL)    /* BIGEND (Bitfield-Mask: 0x01) */
#define RUSB2_CFIFOSEL_ISEL_Pos         (5UL)        /* ISEL (Bit 5) */
#define RUSB2_CFIFOSEL_ISEL_Msk         (0x20UL)     /* ISEL (Bitfield-Mask: 0x01) */
#define RUSB2_CFIFOSEL_CURPIPE_Pos      (0UL)        /* CURPIPE (Bit 0) */
#define RUSB2_CFIFOSEL_CURPIPE_Msk      (0xfUL)      /* CURPIPE (Bitfield-Mask: 0x0f) */

// CFIFOCTR
#define RUSB2_CFIFOCTR_BVAL_Pos         (15UL)       /* BVAL (Bit 15) */
#define RUSB2_CFIFOCTR_BVAL_Msk         (0x8000UL)   /* BVAL (Bitfield-Mask: 0x01) */
#define RUSB2_CFIFOCTR_BCLR_Pos         (14UL)       /* BCLR (Bit 14) */
#define RUSB2_CFIFOCTR_BCLR_Msk         (0x4000UL)   /* BCLR (Bitfield-Mask: 0x01) */
#define RUSB2_CFIFOCTR_FRDY_Pos         (13UL)       /* FRDY (Bit 13) */
#define RUSB2_CFIFOCTR_FRDY_Msk         (0x2000UL)   /* FRDY (Bitfield-Mask: 0x01) */
#define RUSB2_CFIFOCTR_DTLN_Pos         (0UL)        /* DTLN (Bit 0) */
#define RUSB2_CFIFOCTR_DTLN_Msk         (0xfffUL)    /* DTLN (Bitfield-Mask: 0xfff) */

// D0FIFOSEL
#define RUSB2_D0FIFOSEL_RCNT_Pos        (15UL)       /* RCNT (Bit 15) */
#define RUSB2_D0FIFOSEL_RCNT_Msk        (0x8000UL)   /* RCNT (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOSEL_REW_Pos         (14UL)       /* REW (Bit 14) */
#define RUSB2_D0FIFOSEL_REW_Msk         (0x4000UL)   /* REW (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOSEL_DCLRM_Pos       (13UL)       /* DCLRM (Bit 13) */
#define RUSB2_D0FIFOSEL_DCLRM_Msk       (0x2000UL)   /* DCLRM (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOSEL_DREQE_Pos       (12UL)       /* DREQE (Bit 12) */
#define RUSB2_D0FIFOSEL_DREQE_Msk       (0x1000UL)   /* DREQE (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOSEL_MBW_Pos         (10UL)       /* MBW (Bit 10) */
#define RUSB2_D0FIFOSEL_MBW_Msk         (0xc00UL)    /* MBW (Bitfield-Mask: 0x03) */
#define RUSB2_D0FIFOSEL_BIGEND_Pos      (8UL)        /* BIGEND (Bit 8) */
#define RUSB2_D0FIFOSEL_BIGEND_Msk      (0x100UL)    /* BIGEND (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOSEL_CURPIPE_Pos     (0UL)        /* CURPIPE (Bit 0) */
#define RUSB2_D0FIFOSEL_CURPIPE_Msk     (0xfUL)      /* CURPIPE (Bitfield-Mask: 0x0f) */

// D0FIFOCTR
#define RUSB2_D0FIFOCTR_BVAL_Pos        (15UL)       /* BVAL (Bit 15) */
#define RUSB2_D0FIFOCTR_BVAL_Msk        (0x8000UL)   /* BVAL (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOCTR_BCLR_Pos        (14UL)       /* BCLR (Bit 14) */
#define RUSB2_D0FIFOCTR_BCLR_Msk        (0x4000UL)   /* BCLR (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOCTR_FRDY_Pos        (13UL)       /* FRDY (Bit 13) */
#define RUSB2_D0FIFOCTR_FRDY_Msk        (0x2000UL)   /* FRDY (Bitfield-Mask: 0x01) */
#define RUSB2_D0FIFOCTR_DTLN_Pos        (0UL)        /* DTLN (Bit 0) */
#define RUSB2_D0FIFOCTR_DTLN_Msk        (0xfffUL)    /* DTLN (Bitfield-Mask: 0xfff) */

// D1FIFOSEL
#define RUSB2_D1FIFOSEL_RCNT_Pos        (15UL)       /* RCNT (Bit 15) */
#define RUSB2_D1FIFOSEL_RCNT_Msk        (0x8000UL)   /* RCNT (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOSEL_REW_Pos         (14UL)       /* REW (Bit 14) */
#define RUSB2_D1FIFOSEL_REW_Msk         (0x4000UL)   /* REW (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOSEL_DCLRM_Pos       (13UL)       /* DCLRM (Bit 13) */
#define RUSB2_D1FIFOSEL_DCLRM_Msk       (0x2000UL)   /* DCLRM (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOSEL_DREQE_Pos       (12UL)       /* DREQE (Bit 12) */
#define RUSB2_D1FIFOSEL_DREQE_Msk       (0x1000UL)   /* DREQE (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOSEL_MBW_Pos         (10UL)       /* MBW (Bit 10) */
#define RUSB2_D1FIFOSEL_MBW_Msk         (0xc00UL)    /* MBW (Bitfield-Mask: 0x03) */
#define RUSB2_D1FIFOSEL_BIGEND_Pos      (8UL)        /* BIGEND (Bit 8) */
#define RUSB2_D1FIFOSEL_BIGEND_Msk      (0x100UL)    /* BIGEND (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOSEL_CURPIPE_Pos     (0UL)        /* CURPIPE (Bit 0) */
#define RUSB2_D1FIFOSEL_CURPIPE_Msk     (0xfUL)      /* CURPIPE (Bitfield-Mask: 0x0f) */

// D1FIFOCTR
#define RUSB2_D1FIFOCTR_BVAL_Pos        (15UL)       /* BVAL (Bit 15) */
#define RUSB2_D1FIFOCTR_BVAL_Msk        (0x8000UL)   /* BVAL (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOCTR_BCLR_Pos        (14UL)       /* BCLR (Bit 14) */
#define RUSB2_D1FIFOCTR_BCLR_Msk        (0x4000UL)   /* BCLR (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOCTR_FRDY_Pos        (13UL)       /* FRDY (Bit 13) */
#define RUSB2_D1FIFOCTR_FRDY_Msk        (0x2000UL)   /* FRDY (Bitfield-Mask: 0x01) */
#define RUSB2_D1FIFOCTR_DTLN_Pos        (0UL)        /* DTLN (Bit 0) */
#define RUSB2_D1FIFOCTR_DTLN_Msk        (0xfffUL)    /* DTLN (Bitfield-Mask: 0xfff) */

// INTENB0
#define RUSB2_INTENB0_VBSE_Pos          (15UL)       /* VBSE (Bit 15) */
#define RUSB2_INTENB0_VBSE_Msk          (0x8000UL)   /* VBSE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB0_RSME_Pos          (14UL)       /* RSME (Bit 14) */
#define RUSB2_INTENB0_RSME_Msk          (0x4000UL)   /* RSME (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB0_SOFE_Pos          (13UL)       /* SOFE (Bit 13) */
#define RUSB2_INTENB0_SOFE_Msk          (0x2000UL)   /* SOFE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB0_DVSE_Pos          (12UL)       /* DVSE (Bit 12) */
#define RUSB2_INTENB0_DVSE_Msk          (0x1000UL)   /* DVSE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB0_CTRE_Pos          (11UL)       /* CTRE (Bit 11) */
#define RUSB2_INTENB0_CTRE_Msk          (0x800UL)    /* CTRE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB0_BEMPE_Pos         (10UL)       /* BEMPE (Bit 10) */
#define RUSB2_INTENB0_BEMPE_Msk         (0x400UL)    /* BEMPE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB0_NRDYE_Pos         (9UL)        /* NRDYE (Bit 9) */
#define RUSB2_INTENB0_NRDYE_Msk         (0x200UL)    /* NRDYE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB0_BRDYE_Pos         (8UL)        /* BRDYE (Bit 8) */
#define RUSB2_INTENB0_BRDYE_Msk         (0x100UL)    /* BRDYE (Bitfield-Mask: 0x01) */

// INTENB1
#define RUSB2_INTENB1_OVRCRE_Pos        (15UL)       /* OVRCRE (Bit 15) */
#define RUSB2_INTENB1_OVRCRE_Msk        (0x8000UL)   /* OVRCRE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB1_BCHGE_Pos         (14UL)       /* BCHGE (Bit 14) */
#define RUSB2_INTENB1_BCHGE_Msk         (0x4000UL)   /* BCHGE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB1_DTCHE_Pos         (12UL)       /* DTCHE (Bit 12) */
#define RUSB2_INTENB1_DTCHE_Msk         (0x1000UL)   /* DTCHE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB1_ATTCHE_Pos        (11UL)       /* ATTCHE (Bit 11) */
#define RUSB2_INTENB1_ATTCHE_Msk        (0x800UL)    /* ATTCHE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB1_L1RSMENDE_Pos     (9UL)        /*!< L1RSMENDE (Bit 9)                                     */
#define RUSB2_INTENB1_L1RSMENDE_Msk     (0x200UL)    /*!< L1RSMENDE (Bitfield-Mask: 0x01)                       */
#define RUSB2_INTENB1_LPMENDE_Pos       (8UL)        /*!< LPMENDE (Bit 8)                                       */
#define RUSB2_INTENB1_LPMENDE_Msk       (0x100UL)    /*!< LPMENDE (Bitfield-Mask: 0x01)                         */
#define RUSB2_INTENB1_EOFERRE_Pos       (6UL)        /* EOFERRE (Bit 6) */
#define RUSB2_INTENB1_EOFERRE_Msk       (0x40UL)     /* EOFERRE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB1_SIGNE_Pos         (5UL)        /* SIGNE (Bit 5) */
#define RUSB2_INTENB1_SIGNE_Msk         (0x20UL)     /* SIGNE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB1_SACKE_Pos         (4UL)        /* SACKE (Bit 4) */
#define RUSB2_INTENB1_SACKE_Msk         (0x10UL)     /* SACKE (Bitfield-Mask: 0x01) */
#define RUSB2_INTENB1_PDDETINTE0_Pos    (0UL)        /* PDDETINTE0 (Bit 0) */
#define RUSB2_INTENB1_PDDETINTE0_Msk    (0x1UL)      /* PDDETINTE0 (Bitfield-Mask: 0x01) */

// BRDYENB
#define RUSB2_BRDYENB_PIPEBRDYE_Pos     (0UL)        /* PIPEBRDYE (Bit 0) */
#define RUSB2_BRDYENB_PIPEBRDYE_Msk     (0x1UL)      /* PIPEBRDYE (Bitfield-Mask: 0x01) */

// NRDYENB
#define RUSB2_NRDYENB_PIPENRDYE_Pos     (0UL)        /* PIPENRDYE (Bit 0) */
#define RUSB2_NRDYENB_PIPENRDYE_Msk     (0x1UL)      /* PIPENRDYE (Bitfield-Mask: 0x01) */

// BEMPENB
#define RUSB2_BEMPENB_PIPEBEMPE_Pos     (0UL)        /* PIPEBEMPE (Bit 0) */
#define RUSB2_BEMPENB_PIPEBEMPE_Msk     (0x1UL)      /* PIPEBEMPE (Bitfield-Mask: 0x01) */

// SOFCFG
#define RUSB2_SOFCFG_TRNENSEL_Pos       (8UL)        /* TRNENSEL (Bit 8) */
#define RUSB2_SOFCFG_TRNENSEL_Msk       (0x100UL)    /* TRNENSEL (Bitfield-Mask: 0x01) */
#define RUSB2_SOFCFG_BRDYM_Pos          (6UL)        /* BRDYM (Bit 6) */
#define RUSB2_SOFCFG_BRDYM_Msk          (0x40UL)     /* BRDYM (Bitfield-Mask: 0x01) */
#define RUSB2_SOFCFG_INTL_Pos           (5UL)        /* INTL (Bit 5) */
#define RUSB2_SOFCFG_INTL_Msk           (0x20UL)     /* INTL (Bitfield-Mask: 0x01) */
#define RUSB2_SOFCFG_EDGESTS_Pos        (4UL)        /* EDGESTS (Bit 4) */
#define RUSB2_SOFCFG_EDGESTS_Msk        (0x10UL)     /* EDGESTS (Bitfield-Mask: 0x01) */

// PHYSET
#define RUSB2_PHYSET_HSEB_Pos           (15UL)       /* HSEB (Bit 15) */
#define RUSB2_PHYSET_HSEB_Msk           (0x8000UL)   /* HSEB (Bitfield-Mask: 0x01) */
#define RUSB2_PHYSET_REPSTART_Pos       (11UL)       /* REPSTART (Bit 11) */
#define RUSB2_PHYSET_REPSTART_Msk       (0x800UL)    /* REPSTART (Bitfield-Mask: 0x01) */
#define RUSB2_PHYSET_REPSEL_Pos         (8UL)        /* REPSEL (Bit 8) */
#define RUSB2_PHYSET_REPSEL_Msk         (0x300UL)    /* REPSEL (Bitfield-Mask: 0x03) */
#define RUSB2_PHYSET_CLKSEL_Pos         (4UL)        /* CLKSEL (Bit 4) */
#define RUSB2_PHYSET_CLKSEL_Msk         (0x30UL)     /* CLKSEL (Bitfield-Mask: 0x03) */
#define RUSB2_PHYSET_CDPEN_Pos          (3UL)        /* CDPEN (Bit 3) */
#define RUSB2_PHYSET_CDPEN_Msk          (0x8UL)      /* CDPEN (Bitfield-Mask: 0x01) */
#define RUSB2_PHYSET_PLLRESET_Pos       (1UL)        /* PLLRESET (Bit 1) */
#define RUSB2_PHYSET_PLLRESET_Msk       (0x2UL)      /* PLLRESET (Bitfield-Mask: 0x01) */
#define RUSB2_PHYSET_DIRPD_Pos          (0UL)        /* DIRPD (Bit 0) */
#define RUSB2_PHYSET_DIRPD_Msk          (0x1UL)      /* DIRPD (Bitfield-Mask: 0x01) */

// INTSTS0
#define RUSB2_INTSTS0_VBINT_Pos         (15UL)       /* VBINT (Bit 15) */
#define RUSB2_INTSTS0_VBINT_Msk         (0x8000UL)   /* VBINT (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_RESM_Pos          (14UL)       /* RESM (Bit 14) */
#define RUSB2_INTSTS0_RESM_Msk          (0x4000UL)   /* RESM (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_SOFR_Pos          (13UL)       /* SOFR (Bit 13) */
#define RUSB2_INTSTS0_SOFR_Msk          (0x2000UL)   /* SOFR (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_DVST_Pos          (12UL)       /* DVST (Bit 12) */
#define RUSB2_INTSTS0_DVST_Msk          (0x1000UL)   /* DVST (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_CTRT_Pos          (11UL)       /* CTRT (Bit 11) */
#define RUSB2_INTSTS0_CTRT_Msk          (0x800UL)    /* CTRT (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_BEMP_Pos          (10UL)       /* BEMP (Bit 10) */
#define RUSB2_INTSTS0_BEMP_Msk          (0x400UL)    /* BEMP (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_NRDY_Pos          (9UL)        /* NRDY (Bit 9) */
#define RUSB2_INTSTS0_NRDY_Msk          (0x200UL)    /* NRDY (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_BRDY_Pos          (8UL)        /* BRDY (Bit 8) */
#define RUSB2_INTSTS0_BRDY_Msk          (0x100UL)    /* BRDY (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_VBSTS_Pos         (7UL)        /* VBSTS (Bit 7) */
#define RUSB2_INTSTS0_VBSTS_Msk         (0x80UL)     /* VBSTS (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_DVSQ_Pos          (4UL)        /* DVSQ (Bit 4) */
#define RUSB2_INTSTS0_DVSQ_Msk          (0x70UL)     /* DVSQ (Bitfield-Mask: 0x07) */
#define RUSB2_INTSTS0_VALID_Pos         (3UL)        /* VALID (Bit 3) */
#define RUSB2_INTSTS0_VALID_Msk         (0x8UL)      /* VALID (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS0_CTSQ_Pos          (0UL)        /* CTSQ (Bit 0) */
#define RUSB2_INTSTS0_CTSQ_Msk          (0x7UL)      /* CTSQ (Bitfield-Mask: 0x07) */

// INTSTS1
#define RUSB2_INTSTS1_OVRCR_Pos         (15UL)       /* OVRCR (Bit 15) */
#define RUSB2_INTSTS1_OVRCR_Msk         (0x8000UL)   /* OVRCR (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_BCHG_Pos          (14UL)       /* BCHG (Bit 14) */
#define RUSB2_INTSTS1_BCHG_Msk          (0x4000UL)   /* BCHG (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_DTCH_Pos          (12UL)       /* DTCH (Bit 12) */
#define RUSB2_INTSTS1_DTCH_Msk          (0x1000UL)   /* DTCH (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_ATTCH_Pos         (11UL)       /* ATTCH (Bit 11) */
#define RUSB2_INTSTS1_ATTCH_Msk         (0x800UL)    /* ATTCH (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_L1RSMEND_Pos      (9UL)        /* L1RSMEND (Bit 9) */
#define RUSB2_INTSTS1_L1RSMEND_Msk      (0x200UL)    /* L1RSMEND (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_LPMEND_Pos        (8UL)        /* LPMEND (Bit 8) */
#define RUSB2_INTSTS1_LPMEND_Msk        (0x100UL)    /* LPMEND (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_EOFERR_Pos        (6UL)        /* EOFERR (Bit 6) */
#define RUSB2_INTSTS1_EOFERR_Msk        (0x40UL)     /* EOFERR (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_SIGN_Pos          (5UL)        /* SIGN (Bit 5) */
#define RUSB2_INTSTS1_SIGN_Msk          (0x20UL)     /* SIGN (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_SACK_Pos          (4UL)        /* SACK (Bit 4) */
#define RUSB2_INTSTS1_SACK_Msk          (0x10UL)     /* SACK (Bitfield-Mask: 0x01) */
#define RUSB2_INTSTS1_PDDETINT0_Pos     (0UL)        /* PDDETINT0 (Bit 0) */
#define RUSB2_INTSTS1_PDDETINT0_Msk     (0x1UL)      /* PDDETINT0 (Bitfield-Mask: 0x01) */

// BRDYSTS
#define RUSB2_BRDYSTS_PIPEBRDY_Pos      (0UL)        /* PIPEBRDY (Bit 0) */
#define RUSB2_BRDYSTS_PIPEBRDY_Msk      (0x1UL)      /* PIPEBRDY (Bitfield-Mask: 0x01) */

// NRDYSTS
#define RUSB2_NRDYSTS_PIPENRDY_Pos      (0UL)        /* PIPENRDY (Bit 0) */
#define RUSB2_NRDYSTS_PIPENRDY_Msk      (0x1UL)      /* PIPENRDY (Bitfield-Mask: 0x01) */

// BEMPSTS
#define RUSB2_BEMPSTS_PIPEBEMP_Pos      (0UL)        /* PIPEBEMP (Bit 0) */
#define RUSB2_BEMPSTS_PIPEBEMP_Msk      (0x1UL)      /* PIPEBEMP (Bitfield-Mask: 0x01) */

// FRMNUM
#define RUSB2_FRMNUM_OVRN_Pos           (15UL)       /* OVRN (Bit 15) */
#define RUSB2_FRMNUM_OVRN_Msk           (0x8000UL)   /* OVRN (Bitfield-Mask: 0x01) */
#define RUSB2_FRMNUM_CRCE_Pos           (14UL)       /* CRCE (Bit 14) */
#define RUSB2_FRMNUM_CRCE_Msk           (0x4000UL)   /* CRCE (Bitfield-Mask: 0x01) */
#define RUSB2_FRMNUM_FRNM_Pos           (0UL)        /* FRNM (Bit 0) */
#define RUSB2_FRMNUM_FRNM_Msk           (0x7ffUL)    /* FRNM (Bitfield-Mask: 0x7ff) */

// UFRMNUM
#define RUSB2_UFRMNUM_DVCHG_Pos         (15UL)       /* DVCHG (Bit 15) */
#define RUSB2_UFRMNUM_DVCHG_Msk         (0x8000UL)   /* DVCHG (Bitfield-Mask: 0x01) */
#define RUSB2_UFRMNUM_UFRNM_Pos         (0UL)        /* UFRNM (Bit 0) */
#define RUSB2_UFRMNUM_UFRNM_Msk         (0x7UL)      /* UFRNM (Bitfield-Mask: 0x07) */

// USBADDR
#define RUSB2_USBADDR_STSRECOV0_Pos     (8UL)        /* STSRECOV0 (Bit 8) */
#define RUSB2_USBADDR_STSRECOV0_Msk     (0x700UL)    /* STSRECOV0 (Bitfield-Mask: 0x07) */
#define RUSB2_USBADDR_USBADDR_Pos       (0UL)        /* USBADDR (Bit 0) */
#define RUSB2_USBADDR_USBADDR_Msk       (0x7fUL)     /* USBADDR (Bitfield-Mask: 0x7f) */

// USBREQ
#define RUSB2_USBREQ_BREQUEST_Pos       (8UL)        /* BREQUEST (Bit 8) */
#define RUSB2_USBREQ_BREQUEST_Msk       (0xff00UL)   /* BREQUEST (Bitfield-Mask: 0xff) */
#define RUSB2_USBREQ_BMREQUESTTYPE_Pos  (0UL)        /* BMREQUESTTYPE (Bit 0) */
#define RUSB2_USBREQ_BMREQUESTTYPE_Msk  (0xffUL)     /* BMREQUESTTYPE (Bitfield-Mask: 0xff) */

// USBVAL
#define RUSB2_USBVAL_WVALUE_Pos         (0UL)        /* WVALUE (Bit 0) */
#define RUSB2_USBVAL_WVALUE_Msk         (0xffffUL)   /* WVALUE (Bitfield-Mask: 0xffff) */

// USBINDX
#define RUSB2_USBINDX_WINDEX_Pos        (0UL)        /* WINDEX (Bit 0) */
#define RUSB2_USBINDX_WINDEX_Msk        (0xffffUL)   /* WINDEX (Bitfield-Mask: 0xffff) */

// USBLENG
#define RUSB2_USBLENG_WLENGTH_Pos       (0UL)        /* WLENGTH (Bit 0) */
#define RUSB2_USBLENG_WLENGTH_Msk       (0xffffUL)   /* WLENGTH (Bitfield-Mask: 0xffff) */

// DCPCFG
#define RUSB2_DCPCFG_CNTMD_Pos          (8UL)        /* CNTMD (Bit 8) */
#define RUSB2_DCPCFG_CNTMD_Msk          (0x100UL)    /* CNTMD (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCFG_SHTNAK_Pos         (7UL)        /* SHTNAK (Bit 7) */
#define RUSB2_DCPCFG_SHTNAK_Msk         (0x80UL)     /* SHTNAK (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCFG_DIR_Pos            (4UL)        /* DIR (Bit 4) */
#define RUSB2_DCPCFG_DIR_Msk            (0x10UL)     /* DIR (Bitfield-Mask: 0x01) */

// DCPMAXP
#define RUSB2_DCPMAXP_DEVSEL_Pos        (12UL)       /* DEVSEL (Bit 12) */
#define RUSB2_DCPMAXP_DEVSEL_Msk        (0xf000UL)   /* DEVSEL (Bitfield-Mask: 0x0f) */
#define RUSB2_DCPMAXP_MXPS_Pos          (0UL)        /* MXPS (Bit 0) */
#define RUSB2_DCPMAXP_MXPS_Msk          (0x7fUL)     /* MXPS (Bitfield-Mask: 0x7f) */

// DCPCTR
#define RUSB2_DCPCTR_BSTS_Pos           (15UL)       /* BSTS (Bit 15) */
#define RUSB2_DCPCTR_BSTS_Msk           (0x8000UL)   /* BSTS (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCTR_SUREQ_Pos          (14UL)       /* SUREQ (Bit 14) */
#define RUSB2_DCPCTR_SUREQ_Msk          (0x4000UL)   /* SUREQ (Bitfield-Mask: 0x01) */
#define R_USB_HS0_DCPCTR_CSCLR_Pos      (13UL)       /*!< CSCLR (Bit 13)                                        */
#define RUSB2_DCPCTR_CSCLR_Msk          (0x2000UL)   /*!< CSCLR (Bitfield-Mask: 0x01)                           */
#define RUSB2_DCPCTR_CSSTS_Pos          (12UL)       /*!< CSSTS (Bit 12)                                        */
#define RUSB2_DCPCTR_CSSTS_Msk          (0x1000UL)   /*!< CSSTS (Bitfield-Mask: 0x01)                           */
#define RUSB2_DCPCTR_SUREQCLR_Pos       (11UL)       /* SUREQCLR (Bit 11) */
#define RUSB2_DCPCTR_SUREQCLR_Msk       (0x800UL)    /* SUREQCLR (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCTR_SQCLR_Pos          (8UL)        /* SQCLR (Bit 8) */
#define RUSB2_DCPCTR_SQCLR_Msk          (0x100UL)    /* SQCLR (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCTR_SQSET_Pos          (7UL)        /* SQSET (Bit 7) */
#define RUSB2_DCPCTR_SQSET_Msk          (0x80UL)     /* SQSET (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCTR_SQMON_Pos          (6UL)        /* SQMON (Bit 6) */
#define RUSB2_DCPCTR_SQMON_Msk          (0x40UL)     /* SQMON (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCTR_PBUSY_Pos          (5UL)        /* PBUSY (Bit 5) */
#define RUSB2_DCPCTR_PBUSY_Msk          (0x20UL)     /* PBUSY (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCTR_CCPL_Pos           (2UL)        /* CCPL (Bit 2) */
#define RUSB2_DCPCTR_CCPL_Msk           (0x4UL)      /* CCPL (Bitfield-Mask: 0x01) */
#define RUSB2_DCPCTR_PID_Pos            (0UL)        /* PID (Bit 0) */
#define RUSB2_DCPCTR_PID_Msk            (0x3UL)      /* PID (Bitfield-Mask: 0x03) */

// PIPESEL
#define RUSB2_PIPESEL_PIPESEL_Pos       (0UL)        /* PIPESEL (Bit 0) */
#define RUSB2_PIPESEL_PIPESEL_Msk       (0xfUL)      /* PIPESEL (Bitfield-Mask: 0x0f) */

// PIPECFG
#define RUSB2_PIPECFG_TYPE_Pos          (14UL)       /* TYPE (Bit 14) */
#define RUSB2_PIPECFG_TYPE_Msk          (0xc000UL)   /* TYPE (Bitfield-Mask: 0x03) */
#define RUSB2_PIPECFG_BFRE_Pos          (10UL)       /* BFRE (Bit 10) */
#define RUSB2_PIPECFG_BFRE_Msk          (0x400UL)    /* BFRE (Bitfield-Mask: 0x01) */
#define RUSB2_PIPECFG_DBLB_Pos          (9UL)        /* DBLB (Bit 9) */
#define RUSB2_PIPECFG_DBLB_Msk          (0x200UL)    /* DBLB (Bitfield-Mask: 0x01) */
#define RUSB2_PIPECFG_CNTMD_Pos         (8UL)        /*!< CNTMD (Bit 8)                                         */
#define RUSB2_PIPECFG_CNTMD_Msk         (0x100UL)    /*!< CNTMD (Bitfield-Mask: 0x01)                           */
#define RUSB2_PIPECFG_SHTNAK_Pos        (7UL)        /* SHTNAK (Bit 7) */
#define RUSB2_PIPECFG_SHTNAK_Msk        (0x80UL)     /* SHTNAK (Bitfield-Mask: 0x01) */
#define RUSB2_PIPECFG_DIR_Pos           (4UL)        /* DIR (Bit 4) */
#define RUSB2_PIPECFG_DIR_Msk           (0x10UL)     /* DIR (Bitfield-Mask: 0x01) */
#define RUSB2_PIPECFG_EPNUM_Pos         (0UL)        /* EPNUM (Bit 0) */
#define RUSB2_PIPECFG_EPNUM_Msk         (0xfUL)      /* EPNUM (Bitfield-Mask: 0x0f) */

// PIPEBUF
#define RUSB2_PIPEBUF_BUFSIZE_Pos       (10UL)       /*!< BUFSIZE (Bit 10)                                      */
#define RUSB2_PIPEBUF_BUFSIZE_Msk       (0x7c00UL)   /*!< BUFSIZE (Bitfield-Mask: 0x1f)                         */
#define RUSB2_PIPEBUF_BUFNMB_Pos        (0UL)        /*!< BUFNMB (Bit 0)                                        */
#define RUSB2_PIPEBUF_BUFNMB_Msk        (0xffUL)     /*!< BUFNMB (Bitfield-Mask: 0xff)                          */

// PIPEMAXP
#define RUSB2_PIPEMAXP_DEVSEL_Pos       (12UL)       /* DEVSEL (Bit 12) */
#define RUSB2_PIPEMAXP_DEVSEL_Msk       (0xf000UL)   /* DEVSEL (Bitfield-Mask: 0x0f) */
#define RUSB2_PIPEMAXP_MXPS_Pos         (0UL)        /* MXPS (Bit 0) */
#define RUSB2_PIPEMAXP_MXPS_Msk         (0x1ffUL)    /* MXPS (Bitfield-Mask: 0x1ff) */

// PIPEPERI
#define RUSB2_PIPEPERI_IFIS_Pos         (12UL)       /* IFIS (Bit 12) */
#define RUSB2_PIPEPERI_IFIS_Msk         (0x1000UL)   /* IFIS (Bitfield-Mask: 0x01) */
#define RUSB2_PIPEPERI_IITV_Pos         (0UL)        /* IITV (Bit 0) */
#define RUSB2_PIPEPERI_IITV_Msk         (0x7UL)      /* IITV (Bitfield-Mask: 0x07) */

// PIPE_CTR
#define RUSB2_PIPE_CTR_BSTS_Pos         (15UL)       /* BSTS (Bit 15) */
#define RUSB2_PIPE_CTR_BSTS_Msk         (0x8000UL)   /* BSTS (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_INBUFM_Pos       (14UL)       /* INBUFM (Bit 14) */
#define RUSB2_PIPE_CTR_INBUFM_Msk       (0x4000UL)   /* INBUFM (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_CSCLR_Pos        (13UL)       /* CSCLR (Bit 13) */
#define RUSB2_PIPE_CTR_CSCLR_Msk        (0x2000UL)   /* CSCLR (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_CSSTS_Pos        (12UL)       /* CSSTS (Bit 12) */
#define RUSB2_PIPE_CTR_CSSTS_Msk        (0x1000UL)   /* CSSTS (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_ATREPM_Pos       (10UL)       /* ATREPM (Bit 10) */
#define RUSB2_PIPE_CTR_ATREPM_Msk       (0x400UL)    /* ATREPM (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_ACLRM_Pos        (9UL)        /* ACLRM (Bit 9) */
#define RUSB2_PIPE_CTR_ACLRM_Msk        (0x200UL)    /* ACLRM (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_SQCLR_Pos        (8UL)        /* SQCLR (Bit 8) */
#define RUSB2_PIPE_CTR_SQCLR_Msk        (0x100UL)    /* SQCLR (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_SQSET_Pos        (7UL)        /* SQSET (Bit 7) */
#define RUSB2_PIPE_CTR_SQSET_Msk        (0x80UL)     /* SQSET (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_SQMON_Pos        (6UL)        /* SQMON (Bit 6) */
#define RUSB2_PIPE_CTR_SQMON_Msk        (0x40UL)     /* SQMON (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_PBUSY_Pos        (5UL)        /* PBUSY (Bit 5) */
#define RUSB2_PIPE_CTR_PBUSY_Msk        (0x20UL)     /* PBUSY (Bitfield-Mask: 0x01) */
#define RUSB2_PIPE_CTR_PID_Pos          (0UL)        /* PID (Bit 0) */
#define RUSB2_PIPE_CTR_PID_Msk          (0x3UL)      /* PID (Bitfield-Mask: 0x03) */

// DEVADD
#define RUSB2_DEVADD_UPPHUB_Pos         (11UL)       /* UPPHUB (Bit 11) */
#define RUSB2_DEVADD_UPPHUB_Msk         (0x7800UL)   /* UPPHUB (Bitfield-Mask: 0x0f) */
#define RUSB2_DEVADD_HUBPORT_Pos        (8UL)        /* HUBPORT (Bit 8) */
#define RUSB2_DEVADD_HUBPORT_Msk        (0x700UL)    /* HUBPORT (Bitfield-Mask: 0x07) */
#define RUSB2_DEVADD_USBSPD_Pos         (6UL)        /* USBSPD (Bit 6) */
#define RUSB2_DEVADD_USBSPD_Msk         (0xc0UL)     /* USBSPD (Bitfield-Mask: 0x03) */

// USBBCCTRL0
#define RUSB2_USBBCCTRL0_PDDETSTS0_Pos  (9UL)        /* PDDETSTS0 (Bit 9) */
#define RUSB2_USBBCCTRL0_PDDETSTS0_Msk  (0x200UL)    /* PDDETSTS0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_CHGDETSTS0_Pos (8UL)        /* CHGDETSTS0 (Bit 8) */
#define RUSB2_USBBCCTRL0_CHGDETSTS0_Msk (0x100UL)    /* CHGDETSTS0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_BATCHGE0_Pos   (7UL)        /* BATCHGE0 (Bit 7) */
#define RUSB2_USBBCCTRL0_BATCHGE0_Msk   (0x80UL)     /* BATCHGE0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_VDMSRCE0_Pos   (5UL)        /* VDMSRCE0 (Bit 5) */
#define RUSB2_USBBCCTRL0_VDMSRCE0_Msk   (0x20UL)     /* VDMSRCE0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_IDPSINKE0_Pos  (4UL)        /* IDPSINKE0 (Bit 4) */
#define RUSB2_USBBCCTRL0_IDPSINKE0_Msk  (0x10UL)     /* IDPSINKE0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_VDPSRCE0_Pos   (3UL)        /* VDPSRCE0 (Bit 3) */
#define RUSB2_USBBCCTRL0_VDPSRCE0_Msk   (0x8UL)      /* VDPSRCE0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_IDMSINKE0_Pos  (2UL)        /* IDMSINKE0 (Bit 2) */
#define RUSB2_USBBCCTRL0_IDMSINKE0_Msk  (0x4UL)      /* IDMSINKE0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_IDPSRCE0_Pos   (1UL)        /* IDPSRCE0 (Bit 1) */
#define RUSB2_USBBCCTRL0_IDPSRCE0_Msk   (0x2UL)      /* IDPSRCE0 (Bitfield-Mask: 0x01) */
#define RUSB2_USBBCCTRL0_RPDME0_Pos     (0UL)        /* RPDME0 (Bit 0) */
#define RUSB2_USBBCCTRL0_RPDME0_Msk     (0x1UL)      /* RPDME0 (Bitfield-Mask: 0x01) */

// UCKSEL
#define RUSB2_UCKSEL_UCKSELC_Pos        (0UL)        /* UCKSELC (Bit 0) */
#define RUSB2_UCKSEL_UCKSELC_Msk        (0x1UL)      /* UCKSELC (Bitfield-Mask: 0x01) */

// USBMC
#define RUSB2_USBMC_VDCEN_Pos           (7UL)        /* VDCEN (Bit 7) */
#define RUSB2_USBMC_VDCEN_Msk           (0x80UL)     /* VDCEN (Bitfield-Mask: 0x01) */
#define RUSB2_USBMC_VDDUSBE_Pos         (0UL)        /* VDDUSBE (Bit 0) */
#define RUSB2_USBMC_VDDUSBE_Msk         (0x1UL)      /* VDDUSBE (Bitfield-Mask: 0x01) */

// PHYSLEW
#define RUSB2_PHYSLEW_SLEWF01_Pos       (3UL)        /* SLEWF01 (Bit 3) */
#define RUSB2_PHYSLEW_SLEWF01_Msk       (0x8UL)      /* SLEWF01 (Bitfield-Mask: 0x01) */
#define RUSB2_PHYSLEW_SLEWF00_Pos       (2UL)        /* SLEWF00 (Bit 2) */
#define RUSB2_PHYSLEW_SLEWF00_Msk       (0x4UL)      /* SLEWF00 (Bitfield-Mask: 0x01) */
#define RUSB2_PHYSLEW_SLEWR01_Pos       (1UL)        /* SLEWR01 (Bit 1) */
#define RUSB2_PHYSLEW_SLEWR01_Msk       (0x2UL)      /* SLEWR01 (Bitfield-Mask: 0x01) */
#define RUSB2_PHYSLEW_SLEWR00_Pos       (0UL)        /* SLEWR00 (Bit 0) */
#define RUSB2_PHYSLEW_SLEWR00_Msk       (0x1UL)      /* SLEWR00 (Bitfield-Mask: 0x01) */

// LPCTRL
#define RUSB2_LPCTRL_HWUPM_Pos          (7UL)        /* HWUPM (Bit 7) */
#define RUSB2_LPCTRL_HWUPM_Msk          (0x80UL)     /* HWUPM (Bitfield-Mask: 0x01) */

// LPSTS
#define RUSB2_LPSTS_SUSPENDM_Pos        (14UL)       /* SUSPENDM (Bit 14) */
#define RUSB2_LPSTS_SUSPENDM_Msk        (0x4000UL)   /* SUSPENDM (Bitfield-Mask: 0x01) */

// BCCTRL
#define RUSB2_BCCTRL_PDDETSTS_Pos       (9UL)        /* PDDETSTS (Bit 9) */
#define RUSB2_BCCTRL_PDDETSTS_Msk       (0x200UL)    /* PDDETSTS (Bitfield-Mask: 0x01) */
#define RUSB2_BCCTRL_CHGDETSTS_Pos      (8UL)        /* CHGDETSTS (Bit 8) */
#define RUSB2_BCCTRL_CHGDETSTS_Msk      (0x100UL)    /* CHGDETSTS (Bitfield-Mask: 0x01) */
#define RUSB2_BCCTRL_DCPMODE_Pos        (5UL)        /* DCPMODE (Bit 5) */
#define RUSB2_BCCTRL_DCPMODE_Msk        (0x20UL)     /* DCPMODE (Bitfield-Mask: 0x01) */
#define RUSB2_BCCTRL_VDMSRCE_Pos        (4UL)        /* VDMSRCE (Bit 4) */
#define RUSB2_BCCTRL_VDMSRCE_Msk        (0x10UL)     /* VDMSRCE (Bitfield-Mask: 0x01) */
#define RUSB2_BCCTRL_IDPSINKE_Pos       (3UL)        /* IDPSINKE (Bit 3) */
#define RUSB2_BCCTRL_IDPSINKE_Msk       (0x8UL)      /* IDPSINKE (Bitfield-Mask: 0x01) */
#define RUSB2_BCCTRL_VDPSRCE_Pos        (2UL)        /* VDPSRCE (Bit 2) */
#define RUSB2_BCCTRL_VDPSRCE_Msk        (0x4UL)      /* VDPSRCE (Bitfield-Mask: 0x01) */
#define RUSB2_BCCTRL_IDMSINKE_Pos       (1UL)        /* IDMSINKE (Bit 1) */
#define RUSB2_BCCTRL_IDMSINKE_Msk       (0x2UL)      /* IDMSINKE (Bitfield-Mask: 0x01) */
#define RUSB2_BCCTRL_IDPSRCE_Pos        (0UL)        /* IDPSRCE (Bit 0) */
#define RUSB2_BCCTRL_IDPSRCE_Msk        (0x1UL)      /* IDPSRCE (Bitfield-Mask: 0x01) */

// PL1CTRL1
#define RUSB2_PL1CTRL1_L1EXTMD_Pos      (14UL)       /* L1EXTMD (Bit 14) */
#define RUSB2_PL1CTRL1_L1EXTMD_Msk      (0x4000UL)   /* L1EXTMD (Bitfield-Mask: 0x01) */
#define RUSB2_PL1CTRL1_HIRDTHR_Pos      (8UL)        /* HIRDTHR (Bit 8) */
#define RUSB2_PL1CTRL1_HIRDTHR_Msk      (0xf00UL)    /* HIRDTHR (Bitfield-Mask: 0x0f) */
#define RUSB2_PL1CTRL1_DVSQ_Pos         (4UL)        /* DVSQ (Bit 4) */
#define RUSB2_PL1CTRL1_DVSQ_Msk         (0xf0UL)     /* DVSQ (Bitfield-Mask: 0x0f) */
#define RUSB2_PL1CTRL1_L1NEGOMD_Pos     (3UL)        /* L1NEGOMD (Bit 3) */
#define RUSB2_PL1CTRL1_L1NEGOMD_Msk     (0x8UL)      /* L1NEGOMD (Bitfield-Mask: 0x01) */
#define RUSB2_PL1CTRL1_L1RESPMD_Pos     (1UL)        /* L1RESPMD (Bit 1) */
#define RUSB2_PL1CTRL1_L1RESPMD_Msk     (0x6UL)      /* L1RESPMD (Bitfield-Mask: 0x03) */
#define RUSB2_PL1CTRL1_L1RESPEN_Pos     (0UL)        /* L1RESPEN (Bit 0) */
#define RUSB2_PL1CTRL1_L1RESPEN_Msk     (0x1UL)      /* L1RESPEN (Bitfield-Mask: 0x01) */

// PL1CTRL2
#define RUSB2_PL1CTRL2_RWEMON_Pos       (12UL)       /* RWEMON (Bit 12) */
#define RUSB2_PL1CTRL2_RWEMON_Msk       (0x1000UL)   /* RWEMON (Bitfield-Mask: 0x01) */
#define RUSB2_PL1CTRL2_HIRDMON_Pos      (8UL)        /* HIRDMON (Bit 8) */
#define RUSB2_PL1CTRL2_HIRDMON_Msk      (0xf00UL)    /* HIRDMON (Bitfield-Mask: 0x0f) */

// HL1CTRL1
#define RUSB2_HL1CTRL1_L1STATUS_Pos     (1UL)        /* L1STATUS (Bit 1) */
#define RUSB2_HL1CTRL1_L1STATUS_Msk     (0x6UL)      /* L1STATUS (Bitfield-Mask: 0x03) */
#define RUSB2_HL1CTRL1_L1REQ_Pos        (0UL)        /* L1REQ (Bit 0) */
#define RUSB2_HL1CTRL1_L1REQ_Msk        (0x1UL)      /* L1REQ (Bitfield-Mask: 0x01) */

// HL1CTRL2
#define RUSB2_HL1CTRL2_BESL_Pos         (15UL)       /* BESL (Bit 15) */
#define RUSB2_HL1CTRL2_BESL_Msk         (0x8000UL)   /* BESL (Bitfield-Mask: 0x01) */
#define RUSB2_HL1CTRL2_L1RWE_Pos        (12UL)       /* L1RWE (Bit 12) */
#define RUSB2_HL1CTRL2_L1RWE_Msk        (0x1000UL)   /* L1RWE (Bitfield-Mask: 0x01) */
#define RUSB2_HL1CTRL2_HIRD_Pos         (8UL)        /* HIRD (Bit 8) */
#define RUSB2_HL1CTRL2_HIRD_Msk         (0xf00UL)    /* HIRD (Bitfield-Mask: 0x0f) */
#define RUSB2_HL1CTRL2_L1ADDR_Pos       (0UL)        /* L1ADDR (Bit 0) */
#define RUSB2_HL1CTRL2_L1ADDR_Msk       (0xfUL)      /* L1ADDR (Bitfield-Mask: 0x0f) */

// PHYTRIM1
#define RUSB2_PHYTRIM1_IMPOFFSET_Pos    (12UL)       /*!< IMPOFFSET (Bit 12)                                    */
#define RUSB2_PHYTRIM1_IMPOFFSET_Msk    (0x7000UL)   /*!< IMPOFFSET (Bitfield-Mask: 0x07)                       */
#define RUSB2_PHYTRIM1_HSIUP_Pos        (8UL)        /*!< HSIUP (Bit 8)                                         */
#define RUSB2_PHYTRIM1_HSIUP_Msk        (0xf00UL)    /*!< HSIUP (Bitfield-Mask: 0x0f)                           */
#define RUSB2_PHYTRIM1_PCOMPENB_Pos     (7UL)        /*!< PCOMPENB (Bit 7)                                      */
#define RUSB2_PHYTRIM1_PCOMPENB_Msk     (0x80UL)     /*!< PCOMPENB (Bitfield-Mask: 0x01)                        */
#define RUSB2_PHYTRIM1_DFALL_Pos        (2UL)        /*!< DFALL (Bit 2)                                         */
#define RUSB2_PHYTRIM1_DFALL_Msk        (0xcUL)      /*!< DFALL (Bitfield-Mask: 0x03)                           */
#define RUSB2_PHYTRIM1_DRISE_Pos        (0UL)        /*!< DRISE (Bit 0)                                         */
#define RUSB2_PHYTRIM1_DRISE_Msk        (0x3UL)      /*!< DRISE (Bitfield-Mask: 0x03)                           */

// PHYTRIM2
#define RUSB2_PHYTRIM2_DIS_Pos          (12UL)       /*!< DIS (Bit 12)                                          */
#define RUSB2_PHYTRIM2_DIS_Msk          (0x7000UL)   /*!< DIS (Bitfield-Mask: 0x07)                             */
#define RUSB2_PHYTRIM2_PDR_Pos          (8UL)        /*!< PDR (Bit 8)                                           */
#define RUSB2_PHYTRIM2_PDR_Msk          (0x300UL)    /*!< PDR (Bitfield-Mask: 0x03)                             */
#define RUSB2_PHYTRIM2_HSRXENMO_Pos     (7UL)        /*!< HSRXENMO (Bit 7)                                      */
#define RUSB2_PHYTRIM2_HSRXENMO_Msk     (0x80UL)     /*!< HSRXENMO (Bitfield-Mask: 0x01)                        */
#define RUSB2_PHYTRIM2_SQU_Pos          (0UL)        /*!< SQU (Bit 0)                                           */
#define RUSB2_PHYTRIM2_SQU_Msk          (0xfUL)      /*!< SQU (Bitfield-Mask: 0x0f)                             */

// DPUSR0R
#define RUSB2_DPUSR0R_DVBSTSHM_Pos      (23UL)       /* DVBSTSHM (Bit 23) */
#define RUSB2_DPUSR0R_DVBSTSHM_Msk      (0x800000UL) /* DVBSTSHM (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_DOVCBHM_Pos       (21UL)       /* DOVCBHM (Bit 21) */
#define RUSB2_DPUSR0R_DOVCBHM_Msk       (0x200000UL) /* DOVCBHM (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_DOVCAHM_Pos       (20UL)       /* DOVCAHM (Bit 20) */
#define RUSB2_DPUSR0R_DOVCAHM_Msk       (0x100000UL) /* DOVCAHM (Bitfield-Mask: 0x01) */

// DPUSR1R
#define RUSB2_DPUSR1R_DVBSTSH_Pos       (23UL)       /* DVBSTSH (Bit 23) */
#define RUSB2_DPUSR1R_DVBSTSH_Msk       (0x800000UL) /* DVBSTSH (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_DOVCBH_Pos        (21UL)       /* DOVCBH (Bit 21) */
#define RUSB2_DPUSR1R_DOVCBH_Msk        (0x200000UL) /* DOVCBH (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_DOVCAH_Pos        (20UL)       /* DOVCAH (Bit 20) */
#define RUSB2_DPUSR1R_DOVCAH_Msk        (0x100000UL) /* DOVCAH (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_DVBSTSHE_Pos      (7UL)        /* DVBSTSHE (Bit 7) */
#define RUSB2_DPUSR1R_DVBSTSHE_Msk      (0x80UL)     /* DVBSTSHE (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_DOVCBHE_Pos       (5UL)        /* DOVCBHE (Bit 5) */
#define RUSB2_DPUSR1R_DOVCBHE_Msk       (0x20UL)     /* DOVCBHE (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_DOVCAHE_Pos       (4UL)        /* DOVCAHE (Bit 4) */
#define RUSB2_DPUSR1R_DOVCAHE_Msk       (0x10UL)     /* DOVCAHE (Bitfield-Mask: 0x01) */

// DPUSR2R
#define RUSB2_DPUSR2R_DMINTE_Pos        (9UL)        /* DMINTE (Bit 9) */
#define RUSB2_DPUSR2R_DMINTE_Msk        (0x200UL)    /* DMINTE (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR2R_DPINTE_Pos        (8UL)        /* DPINTE (Bit 8) */
#define RUSB2_DPUSR2R_DPINTE_Msk        (0x100UL)    /* DPINTE (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR2R_DMVAL_Pos         (5UL)        /* DMVAL (Bit 5) */
#define RUSB2_DPUSR2R_DMVAL_Msk         (0x20UL)     /* DMVAL (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR2R_DPVAL_Pos         (4UL)        /* DPVAL (Bit 4) */
#define RUSB2_DPUSR2R_DPVAL_Msk         (0x10UL)     /* DPVAL (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR2R_DMINT_Pos         (1UL)        /* DMINT (Bit 1) */
#define RUSB2_DPUSR2R_DMINT_Msk         (0x2UL)      /* DMINT (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR2R_DPINT_Pos         (0UL)        /* DPINT (Bit 0) */
#define RUSB2_DPUSR2R_DPINT_Msk         (0x1UL)      /* DPINT (Bitfield-Mask: 0x01) */

// DPUSRCR
#define RUSB2_DPUSRCR_FIXPHYPD_Pos      (1UL)        /* FIXPHYPD (Bit 1) */
#define RUSB2_DPUSRCR_FIXPHYPD_Msk      (0x2UL)      /* FIXPHYPD (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSRCR_FIXPHY_Pos        (0UL)        /* FIXPHY (Bit 0) */
#define RUSB2_DPUSRCR_FIXPHY_Msk        (0x1UL)      /* FIXPHY (Bitfield-Mask: 0x01) */

// DPUSR0R_FS
#define RUSB2_DPUSR0R_FS_DVBSTS0_Pos    (23UL)       /* DVBSTS0 (Bit 23) */
#define RUSB2_DPUSR0R_FS_DVBSTS0_Msk    (0x800000UL) /* DVBSTS0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_DOVCB0_Pos     (21UL)       /* DOVCB0 (Bit 21) */
#define RUSB2_DPUSR0R_FS_DOVCB0_Msk     (0x200000UL) /* DOVCB0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_DOVCA0_Pos     (20UL)       /* DOVCA0 (Bit 20) */
#define RUSB2_DPUSR0R_FS_DOVCA0_Msk     (0x100000UL) /* DOVCA0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_DM0_Pos        (17UL)       /* DM0 (Bit 17) */
#define RUSB2_DPUSR0R_FS_DM0_Msk        (0x20000UL)  /* DM0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_DP0_Pos        (16UL)       /* DP0 (Bit 16) */
#define RUSB2_DPUSR0R_FS_DP0_Msk        (0x10000UL)  /* DP0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_FIXPHY0_Pos    (4UL)        /* FIXPHY0 (Bit 4) */
#define RUSB2_DPUSR0R_FS_FIXPHY0_Msk    (0x10UL)     /* FIXPHY0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_DRPD0_Pos      (3UL)        /* DRPD0 (Bit 3) */
#define RUSB2_DPUSR0R_FS_DRPD0_Msk      (0x8UL)      /* DRPD0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_RPUE0_Pos      (1UL)        /* RPUE0 (Bit 1) */
#define RUSB2_DPUSR0R_FS_RPUE0_Msk      (0x2UL)      /* RPUE0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR0R_FS_SRPC0_Pos      (0UL)        /* SRPC0 (Bit 0) */
#define RUSB2_DPUSR0R_FS_SRPC0_Msk      (0x1UL)      /* SRPC0 (Bitfield-Mask: 0x01) */

// DPUSR1R_FS
#define RUSB2_DPUSR1R_FS_DVBINT0_Pos    (23UL)       /* DVBINT0 (Bit 23) */
#define RUSB2_DPUSR1R_FS_DVBINT0_Msk    (0x800000UL) /* DVBINT0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DOVRCRB0_Pos   (21UL)       /* DOVRCRB0 (Bit 21) */
#define RUSB2_DPUSR1R_FS_DOVRCRB0_Msk   (0x200000UL) /* DOVRCRB0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DOVRCRA0_Pos   (20UL)       /* DOVRCRA0 (Bit 20) */
#define RUSB2_DPUSR1R_FS_DOVRCRA0_Msk   (0x100000UL) /* DOVRCRA0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DMINT0_Pos     (17UL)       /* DMINT0 (Bit 17) */
#define RUSB2_DPUSR1R_FS_DMINT0_Msk     (0x20000UL)  /* DMINT0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DPINT0_Pos     (16UL)       /* DPINT0 (Bit 16) */
#define RUSB2_DPUSR1R_FS_DPINT0_Msk     (0x10000UL)  /* DPINT0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DVBSE0_Pos     (7UL)        /* DVBSE0 (Bit 7) */
#define RUSB2_DPUSR1R_FS_DVBSE0_Msk     (0x80UL)     /* DVBSE0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DOVRCRBE0_Pos  (5UL)        /* DOVRCRBE0 (Bit 5) */
#define RUSB2_DPUSR1R_FS_DOVRCRBE0_Msk  (0x20UL)     /* DOVRCRBE0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DOVRCRAE0_Pos  (4UL)        /* DOVRCRAE0 (Bit 4) */
#define RUSB2_DPUSR1R_FS_DOVRCRAE0_Msk  (0x10UL)     /* DOVRCRAE0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DMINTE0_Pos    (1UL)        /* DMINTE0 (Bit 1) */
#define RUSB2_DPUSR1R_FS_DMINTE0_Msk    (0x2UL)      /* DMINTE0 (Bitfield-Mask: 0x01) */
#define RUSB2_DPUSR1R_FS_DPINTE0_Pos    (0UL)        /* DPINTE0 (Bit 0) */
#define RUSB2_DPUSR1R_FS_DPINTE0_Msk    (0x1UL)      /* DPINTE0 (Bitfield-Mask: 0x01) */

/*--------------------------------------------------------------------*/
/* Register Bit Utils                                           */
/*--------------------------------------------------------------------*/
#define RUSB2_PIPE_CTR_PID_NAK          (0U << RUSB2_PIPE_CTR_PID_Pos)    /* NAK response */
#define RUSB2_PIPE_CTR_PID_BUF          (1U << RUSB2_PIPE_CTR_PID_Pos)    /* BUF response (depends buffer state) */
#define RUSB2_PIPE_CTR_PID_STALL        (2U << RUSB2_PIPE_CTR_PID_Pos)    /* STALL response */
#define RUSB2_PIPE_CTR_PID_STALL2       (3U << RUSB2_PIPE_CTR_PID_Pos)    /* Also STALL response */

#define RUSB2_DVSTCTR0_RHST_LS          (1U << RUSB2_DVSTCTR0_RHST_Pos)   /*  Low-speed connection */
#define RUSB2_DVSTCTR0_RHST_FS          (2U << RUSB2_DVSTCTR0_RHST_Pos)   /*  Full-speed connection */
#define RUSB2_DVSTCTR0_RHST_HS          (3U << RUSB2_DVSTCTR0_RHST_Pos)   /*  Full-speed connection */

#define RUSB2_DEVADD_USBSPD_LS          (1U << RUSB2_DEVADD_USBSPD_Pos)   /* Target Device Low-speed */
#define RUSB2_DEVADD_USBSPD_FS          (2U << RUSB2_DEVADD_USBSPD_Pos)   /* Target Device Full-speed */

#define RUSB2_CFIFOSEL_ISEL_WRITE       (1U << RUSB2_CFIFOSEL_ISEL_Pos)   /* FIFO write AKA TX*/

#define RUSB2_FIFOSEL_BIGEND            (1U << RUSB2_CFIFOSEL_BIGEND_Pos) /* FIFO Big Endian */
#define RUSB2_FIFOSEL_MBW_8BIT          (0U << RUSB2_CFIFOSEL_MBW_Pos)    /* 8-bit width */
#define RUSB2_FIFOSEL_MBW_16BIT         (1U << RUSB2_CFIFOSEL_MBW_Pos)    /* 16-bit width */
#define RUSB2_FIFOSEL_MBW_32BIT         (2U << RUSB2_CFIFOSEL_MBW_Pos)    /* 32-bit width */

#define RUSB2_INTSTS0_CTSQ_CTRL_RDATA   (1U << RUSB2_INTSTS0_CTSQ_Pos)

#define RUSB2_INTSTS0_DVSQ_STATE_DEF    (1U << RUSB2_INTSTS0_DVSQ_Pos)    /* Default state */
#define RUSB2_INTSTS0_DVSQ_STATE_ADDR   (2U << RUSB2_INTSTS0_DVSQ_Pos)    /* Address state */
#define RUSB2_INTSTS0_DVSQ_STATE_SUSP0  (4U << RUSB2_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define RUSB2_INTSTS0_DVSQ_STATE_SUSP1  (5U << RUSB2_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define RUSB2_INTSTS0_DVSQ_STATE_SUSP2  (6U << RUSB2_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define RUSB2_INTSTS0_DVSQ_STATE_SUSP3  (7U << RUSB2_INTSTS0_DVSQ_Pos)    /* Suspend state */

#define RUSB2_PIPECFG_TYPE_BULK         (1U << RUSB2_PIPECFG_TYPE_Pos)
#define RUSB2_PIPECFG_TYPE_INT          (2U << RUSB2_PIPECFG_TYPE_Pos)
#define RUSB2_PIPECFG_TYPE_ISO          (3U << RUSB2_PIPECFG_TYPE_Pos)

//--------------------------------------------------------------------+
// Static Assert
//--------------------------------------------------------------------+

TU_VERIFY_STATIC(sizeof(RUSB2_PIPE_TR_t) == 4, "incorrect size");
TU_VERIFY_STATIC(sizeof(rusb2_reg_t) == 1032, "incorrect size");

TU_VERIFY_STATIC(offsetof(rusb2_reg_t, SYSCFG     ) == 0x0000, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, BUSWAIT    ) == 0x0002, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, SYSSTS0    ) == 0x0004, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PLLSTA     ) == 0x0006, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DVSTCTR0   ) == 0x0008, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, TESTMODE   ) == 0x000C, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, CFIFO      ) == 0x0014, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, D0FIFO     ) == 0x0018, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, D1FIFO     ) == 0x001C, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, CFIFOSEL   ) == 0x0020, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, CFIFOCTR   ) == 0x0022, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, D0FIFOSEL  ) == 0x0028, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, D0FIFOCTR  ) == 0x002A, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, D1FIFOSEL  ) == 0x002C, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, D1FIFOCTR  ) == 0x002E, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, INTENB0    ) == 0x0030, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, INTENB1    ) == 0x0032, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, BRDYENB    ) == 0x0036, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, NRDYENB    ) == 0x0038, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, BEMPENB    ) == 0x003A, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, SOFCFG     ) == 0x003C, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PHYSET     ) == 0x003E, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, INTSTS0    ) == 0x0040, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, INTSTS1    ) == 0x0042, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, BRDYSTS    ) == 0x0046, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, NRDYSTS    ) == 0x0048, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, BEMPSTS    ) == 0x004A, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, FRMNUM     ) == 0x004C, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, UFRMNUM    ) == 0x004E, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, USBADDR    ) == 0x0050, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, USBREQ     ) == 0x0054, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, USBVAL     ) == 0x0056, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, USBINDX    ) == 0x0058, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, USBLENG    ) == 0x005A, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DCPCFG     ) == 0x005C, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DCPMAXP    ) == 0x005E, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DCPCTR     ) == 0x0060, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PIPESEL    ) == 0x0064, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PIPECFG    ) == 0x0068, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PIPEBUF    ) == 0x006A, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PIPEMAXP   ) == 0x006C, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PIPEPERI   ) == 0x006E, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PIPE_CTR   ) == 0x0070, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PIPE_TR    ) == 0x0090, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, USBBCCTRL0 ) == 0x00B0, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, UCKSEL     ) == 0x00C4, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, USBMC      ) == 0x00CC, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DEVADD     ) == 0x00D0, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PHYSLEW    ) == 0x00F0, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, LPCTRL     ) == 0x0100, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, LPSTS      ) == 0x0102, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, BCCTRL     ) == 0x0140, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PL1CTRL1   ) == 0x0144, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PL1CTRL2   ) == 0x0146, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, HL1CTRL1   ) == 0x0148, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, HL1CTRL2   ) == 0x014A, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PHYTRIM1   ) == 0x0150, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, PHYTRIM2   ) == 0x0152, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DPUSR0R    ) == 0x0160, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DPUSR1R    ) == 0x0164, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DPUSR2R    ) == 0x0168, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DPUSRCR    ) == 0x016A, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DPUSR0R_FS ) == 0x0400, "incorrect offset");
TU_VERIFY_STATIC(offsetof(rusb2_reg_t, DPUSR1R_FS ) == 0x0404, "incorrect offset");

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_RUSB2_TYPE_H_ */
