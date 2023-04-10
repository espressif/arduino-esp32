/**
  * Copyright(c) 2016 STMicroelectronics
  * Copyright(c) N Conrad
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
  */

// This file contains source copied from ST's HAL, and thus should have their copyright statement.

// PMA_LENGTH is PMA buffer size in bytes.
// On 512-byte devices, access with a stride of two words (use every other 16-bit address)
// On 1024-byte devices, access with a stride of one word (use every 16-bit address)

#ifndef PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_
#define PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_

#if CFG_TUSB_MCU == OPT_MCU_STM32F0
  #include "stm32f0xx.h"
  #define PMA_LENGTH (1024u)
  // F0x2 models are crystal-less
  // All have internal D+ pull-up
  // 070RB:    2 x 16 bits/word memory     LPM Support, BCD Support
  // PMA dedicated to USB (no sharing with CAN)

#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  #include "stm32f1xx.h"
  #define PMA_LENGTH (512u)
  // NO internal Pull-ups
  //         *B, and *C:    2 x 16 bits/word

  // F1 names this differently from the rest
  #define USB_CNTR_LPMODE   USB_CNTR_LP_MODE

#elif defined(STM32F302xB) || defined(STM32F302xC) || \
      defined(STM32F303xB) || defined(STM32F303xC) || \
      defined(STM32F373xC)
  #include "stm32f3xx.h"
  #define PMA_LENGTH (512u)
  // NO internal Pull-ups
  //         *B, and *C:    1 x 16 bits/word
  // PMA dedicated to USB (no sharing with CAN)

#elif defined(STM32F302x6) || defined(STM32F302x8) || \
      defined(STM32F302xD) || defined(STM32F302xE) || \
      defined(STM32F303xD) || defined(STM32F303xE)
  #include "stm32f3xx.h"
  #define PMA_LENGTH (1024u)
  // NO internal Pull-ups
  // *6, *8, *D, and *E:    2 x 16 bits/word     LPM Support
  // When CAN clock is enabled, USB can use first 768 bytes ONLY.

#elif CFG_TUSB_MCU == OPT_MCU_STM32L0
  #include "stm32l0xx.h"
  #define PMA_LENGTH (1024u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32L1
  #include "stm32l1xx.h"
  #define PMA_LENGTH (512u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32G4
  #include "stm32g4xx.h"
  #define PMA_LENGTH (1024u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32WB
  #include "stm32wbxx.h"
  #define PMA_LENGTH (1024u)
  /* ST provided header has incorrect value */
  #undef USB_PMAADDR
  #define USB_PMAADDR USB1_PMAADDR

#elif CFG_TUSB_MCU == OPT_MCU_STM32L4
  #include "stm32l4xx.h"
  #define PMA_LENGTH (1024u)

#elif CFG_TUSB_MCU == OPT_MCU_STM32L5
  #include "stm32l5xx.h"
  #define PMA_LENGTH (1024u)

  #ifndef USB_PMAADDR
    #define USB_PMAADDR (USB_BASE + (USB_PMAADDR_NS - USB_BASE_NS))
  #endif

#else
  #error You are using an untested or unimplemented STM32 variant. Please update the driver.
  // This includes L1x0, L1x1, L1x2, L4x2 and L4x3, G1x1, G1x3, and G1x4
#endif

// For purposes of accessing the packet
#if ((PMA_LENGTH) == 512u)
  #define PMA_STRIDE  (2u)
#elif ((PMA_LENGTH) == 1024u)
  #define PMA_STRIDE  (1u)
#endif

// And for type-safety create a new macro for the volatile address of PMAADDR
// The compiler should warn us if we cast it to a non-volatile type?
// Volatile is also needed to prevent the optimizer from changing access to 32-bit (as 32-bit access is forbidden)
static __IO uint16_t * const pma = (__IO uint16_t*)USB_PMAADDR;

// prototypes
TU_ATTR_ALWAYS_INLINE static inline __IO uint16_t* pcd_ep_rx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpIdx);
TU_ATTR_ALWAYS_INLINE static inline __IO uint16_t* pcd_ep_tx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpIdx);
TU_ATTR_ALWAYS_INLINE static inline void pcd_set_endpoint(USB_TypeDef * USBx, uint32_t bEpIdx, uint32_t wRegValue);

/* Aligned buffer size according to hardware */
TU_ATTR_ALWAYS_INLINE static inline uint16_t pcd_aligned_buffer_size(uint16_t size)
{
  /* The STM32 full speed USB peripheral supports only a limited set of
   * buffer sizes given by the RX buffer entry format in the USB_BTABLE. */
  uint16_t blocksize = (size > 62) ? 32 : 2;

  // Round up while dividing requested size by blocksize
  uint16_t numblocks = (size + blocksize - 1) / blocksize ;

  return numblocks * blocksize;
}

/* SetENDPOINT */
TU_ATTR_ALWAYS_INLINE static inline void pcd_set_endpoint(USB_TypeDef * USBx, uint32_t bEpIdx, uint32_t wRegValue)
{
  __O uint16_t *reg = (__O uint16_t *)((&USBx->EP0R) + bEpIdx*2u);
  *reg = (uint16_t)wRegValue;
}

/* GetENDPOINT */
TU_ATTR_ALWAYS_INLINE static inline uint16_t pcd_get_endpoint(USB_TypeDef * USBx, uint32_t bEpIdx) {
  __I uint16_t *reg = (__I uint16_t *)((&USBx->EP0R) + bEpIdx*2u);
  return *reg;
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_eptype(USB_TypeDef * USBx, uint32_t bEpIdx, uint32_t wType)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= (uint32_t)USB_EP_T_MASK;
  regVal |= wType;
  regVal |= USB_EP_CTR_RX | USB_EP_CTR_TX; // These clear on write0, so must set high
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t pcd_get_eptype(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EP_T_FIELD;
  return regVal;
}
/**
  * @brief  Clears bit CTR_RX / CTR_TX in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_rx_ep_ctr(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal &= ~USB_EP_CTR_RX;
  regVal |= USB_EP_CTR_TX; // preserve CTR_TX (clears on writing 0)
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_tx_ep_ctr(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal &= ~USB_EP_CTR_TX;
  regVal |= USB_EP_CTR_RX; // preserve CTR_RX (clears on writing 0)
  pcd_set_endpoint(USBx, bEpIdx,regVal);
}
/**
  * @brief  gets counter of the tx buffer.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @retval Counter value
  */
TU_ATTR_ALWAYS_INLINE static inline uint32_t pcd_get_ep_tx_cnt(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  __I uint16_t *regPtr = pcd_ep_tx_cnt_ptr(USBx, bEpIdx);
  return *regPtr & 0x3ffU;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t pcd_get_ep_rx_cnt(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  __I uint16_t *regPtr = pcd_ep_rx_cnt_ptr(USBx, bEpIdx);
  return *regPtr & 0x3ffU;
}

/**
  * @brief  Sets counter of rx buffer with no. of blocks.
  * @param  dwReg Register
  * @param  wCount Counter.
  * @param  wNBlocks no. of Blocks.
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_cnt_reg(__O uint16_t * pdwReg, size_t wCount)
{
  /* We assume that the buffer size is already aligned to hardware requirements. */
  uint16_t blocksize = (wCount > 62) ? 1 : 0;
  uint16_t numblocks = wCount / (blocksize ? 32 : 2);

  /* There should be no remainder in the above calculation */
  TU_ASSERT((wCount - (numblocks * (blocksize ? 32 : 2))) == 0, /**/);

  /* Encode into register. When BLSIZE==1, we need to subtract 1 block count */
  *pdwReg = (blocksize << 15) | ((numblocks - blocksize) << 10);
}

/**
  * @brief  Sets address in an endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @param  bAddr Address.
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_address(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t bAddr)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal |= bAddr;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx,regVal);
}

TU_ATTR_ALWAYS_INLINE static inline __IO uint16_t * pcd_btable_word_ptr(USB_TypeDef * USBx, size_t x)
{
  size_t total_word_offset = (((USBx)->BTABLE)>>1) + x;
  total_word_offset *= PMA_STRIDE;
  return &(pma[total_word_offset]);
}

// Pointers to the PMA table entries (using the ARM address space)
TU_ATTR_ALWAYS_INLINE static inline __IO uint16_t* pcd_ep_tx_address_ptr(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  return pcd_btable_word_ptr(USBx,(bEpIdx)*4u + 0u);
}
TU_ATTR_ALWAYS_INLINE static inline __IO uint16_t* pcd_ep_tx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  return pcd_btable_word_ptr(USBx,(bEpIdx)*4u + 1u);
}

TU_ATTR_ALWAYS_INLINE static inline __IO uint16_t* pcd_ep_rx_address_ptr(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  return  pcd_btable_word_ptr(USBx,(bEpIdx)*4u + 2u);
}

TU_ATTR_ALWAYS_INLINE static inline __IO uint16_t* pcd_ep_rx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  return pcd_btable_word_ptr(USBx,(bEpIdx)*4u + 3u);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_tx_cnt(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wCount)
{
  __IO uint16_t * reg = pcd_ep_tx_cnt_ptr(USBx, bEpIdx);
  *reg = (uint16_t) (*reg & (uint16_t) ~0x3FFU) | (wCount & 0x3FFU);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_rx_cnt(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wCount)
{
  __IO uint16_t * reg = pcd_ep_rx_cnt_ptr(USBx, bEpIdx);
  *reg = (uint16_t) (*reg & (uint16_t) ~0x3FFU) | (wCount & 0x3FFU);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_tx_bufsize(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wCount)
{
  __IO uint16_t *pdwReg = pcd_ep_tx_cnt_ptr((USBx),(bEpIdx));
  wCount = pcd_aligned_buffer_size(wCount);
  pcd_set_ep_cnt_reg(pdwReg, wCount);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_rx_bufsize(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wCount)
{
  __IO uint16_t *pdwReg = pcd_ep_rx_cnt_ptr((USBx),(bEpIdx));
  wCount = pcd_aligned_buffer_size(wCount);
  pcd_set_ep_cnt_reg(pdwReg, wCount);
}

/**
  * @brief  sets the status for tx transfer (bits STAT_TX[1:0]).
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @param  wState new state
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_tx_status(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wState)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPTX_DTOGMASK;

  /* toggle first bit ? */
  if((USB_EPTX_DTOG1 & (wState))!= 0U)
  {
    regVal ^= USB_EPTX_DTOG1;
  }
  /* toggle second bit ?  */
  if((USB_EPTX_DTOG2 & ((uint32_t)(wState)))!= 0U)
  {
    regVal ^= USB_EPTX_DTOG2;
  }

  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
} /* pcd_set_ep_tx_status */

/**
  * @brief  sets the status for rx transfer (bits STAT_TX[1:0])
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @param  wState new state
  * @retval None
  */

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_rx_status(USB_TypeDef * USBx,  uint32_t bEpIdx, uint32_t wState)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPRX_DTOGMASK;

  /* toggle first bit ? */
  if((USB_EPRX_DTOG1 & wState)!= 0U)
  {
    regVal ^= USB_EPRX_DTOG1;
  }
  /* toggle second bit ? */
  if((USB_EPRX_DTOG2 & wState)!= 0U)
  {
    regVal ^= USB_EPRX_DTOG2;
  }

  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
} /* pcd_set_ep_rx_status */

TU_ATTR_ALWAYS_INLINE static inline uint32_t pcd_get_ep_rx_status(USB_TypeDef * USBx,  uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  return (regVal & USB_EPRX_STAT) >> (12u);
} /* pcd_get_ep_rx_status */


/**
  * @brief  Toggles DTOG_RX / DTOG_TX bit in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @retval None
  */
TU_ATTR_ALWAYS_INLINE static inline void pcd_rx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_RX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_tx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

/**
  * @brief  Clears DTOG_RX / DTOG_TX bit in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @retval None
  */

TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_rx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  if((regVal & USB_EP_DTOG_RX) != 0)
  {
    pcd_rx_dtog(USBx,bEpIdx);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_tx_dtog(USB_TypeDef * USBx,  uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  if((regVal & USB_EP_DTOG_TX) != 0)
  {
    pcd_tx_dtog(USBx,bEpIdx);
  }
}

/**
  * @brief  set & clear EP_KIND bit.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpIdx Endpoint Number.
  * @retval None
  */

TU_ATTR_ALWAYS_INLINE static inline void pcd_set_ep_kind(USB_TypeDef * USBx,  uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal |= USB_EP_KIND;
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}
TU_ATTR_ALWAYS_INLINE static inline void pcd_clear_ep_kind(USB_TypeDef * USBx, uint32_t bEpIdx)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpIdx);
  regVal &= USB_EPKIND_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpIdx, regVal);
}

// This checks if the device has "LPM"
#if defined(USB_ISTR_L1REQ)
#define USB_ISTR_L1REQ_FORCED (USB_ISTR_L1REQ)
#else
#define USB_ISTR_L1REQ_FORCED ((uint16_t)0x0000U)
#endif

#define USB_ISTR_ALL_EVENTS (USB_ISTR_PMAOVR | USB_ISTR_ERR | USB_ISTR_WKUP | USB_ISTR_SUSP | \
     USB_ISTR_RESET | USB_ISTR_SOF | USB_ISTR_ESOF | USB_ISTR_L1REQ_FORCED )

// Number of endpoints in hardware
// TODO should use TUP_DCD_ENDPOINT_MAX
#define STFSDEV_EP_COUNT (8u)

#endif /* PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_ */
