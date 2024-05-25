/*
 * The MIT License (MIT)
 *
 * Copyright(c) N Conrad
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_FSDEV_STM32_H
#define TUSB_FSDEV_STM32_H

#if CFG_TUSB_MCU == OPT_MCU_STM32F0
  #include "stm32f0xx.h"
  #define FSDEV_PMA_SIZE (1024u)
  // F0x2 models are crystal-less
  // All have internal D+ pull-up
  // 070RB:    2 x 16 bits/word memory     LPM Support, BCD Support
  // PMA dedicated to USB (no sharing with CAN)

#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  #include "stm32f1xx.h"
  #define FSDEV_PMA_SIZE (512u)
  // NO internal Pull-ups
  //         *B, and *C:    2 x 16 bits/word

  // F1 names this differently from the rest
  #define USB_CNTR_LPMODE   USB_CNTR_LP_MODE

#elif defined(STM32F302xB) || defined(STM32F302xC) || \
      defined(STM32F303xB) || defined(STM32F303xC) || \
      defined(STM32F373xC)
  #include "stm32f3xx.h"
  #define FSDEV_PMA_SIZE (512u)
  // NO internal Pull-ups
  //         *B, and *C:    1 x 16 bits/word
  // PMA dedicated to USB (no sharing with CAN)

#elif defined(STM32F302x6) || defined(STM32F302x8) || \
      defined(STM32F302xD) || defined(STM32F302xE) || \
      defined(STM32F303xD) || defined(STM32F303xE)
  #include "stm32f3xx.h"
  #define FSDEV_PMA_SIZE (1024u)
  // NO internal Pull-ups
  // *6, *8, *D, and *E:    2 x 16 bits/word     LPM Support
  // When CAN clock is enabled, USB can use first 768 bytes ONLY.

#elif CFG_TUSB_MCU == OPT_MCU_STM32L0
  #include "stm32l0xx.h"
  #define FSDEV_PMA_SIZE (1024u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32L1
  #include "stm32l1xx.h"
  #define FSDEV_PMA_SIZE (512u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32G4
  #include "stm32g4xx.h"
  #define FSDEV_PMA_SIZE (1024u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32G0
  #include "stm32g0xx.h"
  #define FSDEV_BUS_32BIT
  #define FSDEV_PMA_SIZE (2048u)
  #undef USB_PMAADDR
  #define USB_PMAADDR USB_DRD_PMAADDR
  #define USB_TypeDef USB_DRD_TypeDef
  #define EP0R CHEP0R
  #define USB_EP_CTR_RX USB_EP_VTRX
  #define USB_EP_CTR_TX USB_EP_VTTX
  #define USB_EP_T_FIELD USB_CHEP_UTYPE
  #define USB_EPREG_MASK USB_CHEP_REG_MASK
  #define USB_EPTX_DTOGMASK USB_CHEP_TX_DTOGMASK
  #define USB_EPRX_DTOGMASK USB_CHEP_RX_DTOGMASK
  #define USB_EPTX_DTOG1 USB_CHEP_TX_DTOG1
  #define USB_EPTX_DTOG2 USB_CHEP_TX_DTOG2
  #define USB_EPRX_DTOG1 USB_CHEP_RX_DTOG1
  #define USB_EPRX_DTOG2 USB_CHEP_RX_DTOG2
  #define USB_EPRX_STAT USB_CH_RX_VALID
  #define USB_EPKIND_MASK USB_EP_KIND_MASK
  #define USB USB_DRD_FS
  #define USB_CNTR_FRES USB_CNTR_USBRST
  #define USB_CNTR_RESUME USB_CNTR_L2RES
  #define USB_ISTR_EP_ID USB_ISTR_IDN
  #define USB_EPADDR_FIELD USB_CHEP_ADDR
  #define USB_CNTR_LPMODE USB_CNTR_SUSPRDY
  #define USB_CNTR_FSUSP USB_CNTR_SUSPEN

#elif CFG_TUSB_MCU == OPT_MCU_STM32H5
  #include "stm32h5xx.h"
  #define FSDEV_BUS_32BIT

  #if !defined(USB_DRD_BASE) && defined(USB_DRD_FS_BASE)
  #define USB_DRD_BASE USB_DRD_FS_BASE
  #endif

  #define FSDEV_PMA_SIZE (2048u)
  #undef USB_PMAADDR
  #define USB_PMAADDR USB_DRD_PMAADDR
  #define USB_TypeDef USB_DRD_TypeDef
  #define EP0R CHEP0R
  #define USB_EP_CTR_RX USB_EP_VTRX
  #define USB_EP_CTR_TX USB_EP_VTTX
  #define USB_EP_T_FIELD USB_CHEP_UTYPE
  #define USB_EPREG_MASK USB_CHEP_REG_MASK
  #define USB_EPTX_DTOGMASK USB_CHEP_TX_DTOGMASK
  #define USB_EPRX_DTOGMASK USB_CHEP_RX_DTOGMASK
  #define USB_EPTX_DTOG1 USB_CHEP_TX_DTOG1
  #define USB_EPTX_DTOG2 USB_CHEP_TX_DTOG2
  #define USB_EPRX_DTOG1 USB_CHEP_RX_DTOG1
  #define USB_EPRX_DTOG2 USB_CHEP_RX_DTOG2
  #define USB_EPRX_STAT USB_CH_RX_VALID
  #define USB_EPKIND_MASK USB_EP_KIND_MASK
  #define USB USB_DRD_FS
  #define USB_CNTR_FRES USB_CNTR_USBRST
  #define USB_CNTR_RESUME USB_CNTR_L2RES
  #define USB_ISTR_EP_ID USB_ISTR_IDN
  #define USB_EPADDR_FIELD USB_CHEP_ADDR
  #define USB_CNTR_LPMODE USB_CNTR_SUSPRDY
  #define USB_CNTR_FSUSP USB_CNTR_SUSPEN

#elif CFG_TUSB_MCU == OPT_MCU_STM32WB
  #include "stm32wbxx.h"
  #define FSDEV_PMA_SIZE (1024u)
  /* ST provided header has incorrect value */
  #undef USB_PMAADDR
  #define USB_PMAADDR USB1_PMAADDR

#elif CFG_TUSB_MCU == OPT_MCU_STM32L4
  #include "stm32l4xx.h"
  #define FSDEV_PMA_SIZE (1024u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32L5
  #include "stm32l5xx.h"
  #define FSDEV_PMA_SIZE (1024u)

  #ifndef USB_PMAADDR
    #define USB_PMAADDR (USB_BASE + (USB_PMAADDR_NS - USB_BASE_NS))
  #endif

#else
  #error You are using an untested or unimplemented STM32 variant. Please update the driver.
  // This includes L1x0, L1x1, L1x2, L4x2 and L4x3, G1x1, G1x3, and G1x4
#endif

// This checks if the device has "LPM"
#if defined(USB_ISTR_L1REQ)
#define USB_ISTR_L1REQ_FORCED (USB_ISTR_L1REQ)
#else
#define USB_ISTR_L1REQ_FORCED ((uint16_t)0x0000U)
#endif

#define USB_ISTR_ALL_EVENTS (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_WKUP | USB_ISTR_SUSP | \
     USB_ISTR_RESET | USB_ISTR_SOF | USB_ISTR_ESOF | USB_ISTR_L1REQ_FORCED )

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

#if TU_CHECK_MCU(OPT_MCU_STM32L1) && !defined(USBWakeUp_IRQn)
  #define USBWakeUp_IRQn USB_FS_WKUP_IRQn
#endif

static const IRQn_Type fsdev_irq[] = {
  #if TU_CHECK_MCU(OPT_MCU_STM32F0, OPT_MCU_STM32L0, OPT_MCU_STM32L4)
    USB_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32F1
    USB_HP_CAN1_TX_IRQn,
    USB_LP_CAN1_RX0_IRQn,
    USBWakeUp_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32F3
    // USB remap handles dcd functions
    USB_HP_CAN_TX_IRQn,
    USB_LP_CAN_RX0_IRQn,
    USBWakeUp_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32G0
    #ifdef STM32G0B0xx
    USB_IRQn,
    #else
    USB_UCPD1_2_IRQn,
    #endif
  #elif TU_CHECK_MCU(OPT_MCU_STM32G4, OPT_MCU_STM32L1)
    USB_HP_IRQn,
    USB_LP_IRQn,
    USBWakeUp_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32H5
    USB_DRD_FS_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32L5
    USB_FS_IRQn,
  #elif CFG_TUSB_MCU == OPT_MCU_STM32WB
    USB_HP_IRQn,
    USB_LP_IRQn,
  #else
    #error Unknown arch in USB driver
  #endif
};
enum { FSDEV_IRQ_NUM = TU_ARRAY_SIZE(fsdev_irq) };

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;

  // forces write to RAM before allowing ISR to execute
  __DSB(); __ISB();

  #if CFG_TUSB_MCU == OPT_MCU_STM32F3 && defined(SYSCFG_CFGR1_USB_IT_RMP)
  // Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to enable the right IRQs.
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP) {
    NVIC_EnableIRQ(USB_HP_IRQn);
    NVIC_EnableIRQ(USB_LP_IRQn);
    NVIC_EnableIRQ(USBWakeUp_RMP_IRQn);
  } else
  #endif
  {
    for (uint8_t i = 0; i < FSDEV_IRQ_NUM; i++) {
      NVIC_EnableIRQ(fsdev_irq[i]);
    }
  }
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;

  #if CFG_TUSB_MCU == OPT_MCU_STM32F3 && defined(SYSCFG_CFGR1_USB_IT_RMP)
  // Some STM32F302/F303 devices allow to remap the USB interrupt vectors from
  // shared USB/CAN IRQs to separate CAN and USB IRQs.
  // This dynamically checks if this remap is active to enable the right IRQs.
  if (SYSCFG->CFGR1 & SYSCFG_CFGR1_USB_IT_RMP) {
    NVIC_DisableIRQ(USB_HP_IRQn);
    NVIC_DisableIRQ(USB_LP_IRQn);
    NVIC_DisableIRQ(USBWakeUp_RMP_IRQn);
  } else
  #endif
  {
    for (uint8_t i = 0; i < FSDEV_IRQ_NUM; i++) {
      NVIC_DisableIRQ(fsdev_irq[i]);
    }
  }

  // CMSIS has a membar after disabling interrupts
}

// Define only on MCU with internal pull-up. BSP can define on MCU without internal PU.
#if defined(USB_BCDR_DPPU)

void dcd_disconnect(uint8_t rhport) {
  (void)rhport;
  USB->BCDR &= ~(USB_BCDR_DPPU);
}

void dcd_connect(uint8_t rhport) {
  (void)rhport;
  USB->BCDR |= USB_BCDR_DPPU;
}

#elif defined(SYSCFG_PMC_USB_PU) // works e.g. on STM32L151

void dcd_disconnect(uint8_t rhport) {
  (void)rhport;
  SYSCFG->PMC &= ~(SYSCFG_PMC_USB_PU);
}

void dcd_connect(uint8_t rhport) {
  (void)rhport;
  SYSCFG->PMC |= SYSCFG_PMC_USB_PU;
}
#endif


#endif /* TUSB_FSDEV_STM32_H */
