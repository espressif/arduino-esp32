/**
  ******************************************************************************
  * @file    dcd_stm32f0_pvt_st.h
  * @brief   DCD utilities from ST code
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  * <h2><center>&copy; parts COPYRIGHT(c) N Conrad</center></h2>
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
  **********/

// This file contains source copied from ST's HAL, and thus should have their copyright statement.

// PMA_LENGTH is PMA buffer size in bytes.
// On 512-byte devices, access with a stride of two words (use every other 16-bit address)
// On 1024-byte devices, access with a stride of one word (use every 16-bit address)

#ifndef PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_
#define PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_

#if defined(STM32F042x6) || \
    defined(STM32F070x6) || defined(STM32F070xB) || \
    defined(STM32F072xB) || \
    defined(STM32F078xx)
  #include "stm32f0xx.h"
  #define PMA_LENGTH (1024u)
  // F0x2 models are crystal-less
  // All have internal D+ pull-up
  // 070RB:    2 x 16 bits/word memory     LPM Support, BCD Support
  // PMA dedicated to USB (no sharing with CAN)

#elif defined(STM32F1_FSDEV)
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
static inline __IO uint16_t* pcd_ep_rx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpNum);
static inline __IO uint16_t* pcd_ep_tx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpNum);
static inline void pcd_set_endpoint(USB_TypeDef * USBx, uint32_t bEpNum, uint32_t wRegValue);


/* SetENDPOINT */
static inline void pcd_set_endpoint(USB_TypeDef * USBx, uint32_t bEpNum, uint32_t wRegValue)
{
  __O uint16_t *reg = (__O uint16_t *)((&USBx->EP0R) + bEpNum*2u);
  *reg = (uint16_t)wRegValue;
}

/* GetENDPOINT */
static inline uint16_t pcd_get_endpoint(USB_TypeDef * USBx, uint32_t bEpNum) {
  __I uint16_t *reg = (__I uint16_t *)((&USBx->EP0R) + bEpNum*2u);
  return *reg;
}

static inline void pcd_set_eptype(USB_TypeDef * USBx, uint32_t bEpNum, uint32_t wType)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= (uint32_t)USB_EP_T_MASK;
  regVal |= wType;
  regVal |= USB_EP_CTR_RX | USB_EP_CTR_TX; // These clear on write0, so must set high
  pcd_set_endpoint(USBx, bEpNum, regVal);
}

static inline uint32_t pcd_get_eptype(USB_TypeDef * USBx, uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= USB_EP_T_FIELD;
  return regVal;
}
/**
  * @brief  Clears bit CTR_RX / CTR_TX in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */
static inline void pcd_clear_rx_ep_ctr(USB_TypeDef * USBx, uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= 0x7FFFu & USB_EPREG_MASK;
  pcd_set_endpoint(USBx, bEpNum, regVal);
}
static inline void pcd_clear_tx_ep_ctr(USB_TypeDef * USBx, uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= regVal & 0xFF7FU & USB_EPREG_MASK;
  pcd_set_endpoint(USBx, bEpNum,regVal);
}
/**
  * @brief  gets counter of the tx buffer.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval Counter value
  */
static inline uint32_t pcd_get_ep_tx_cnt(USB_TypeDef * USBx, uint32_t bEpNum)
{
  __I uint16_t *regPtr = pcd_ep_tx_cnt_ptr(USBx, bEpNum);
  return *regPtr & 0x3ffU;
}

static inline uint32_t pcd_get_ep_rx_cnt(USB_TypeDef * USBx, uint32_t bEpNum)
{
  __I uint16_t *regPtr = pcd_ep_rx_cnt_ptr(USBx, bEpNum);
  return *regPtr & 0x3ffU;
}

/**
  * @brief  Sets counter of rx buffer with no. of blocks.
  * @param  dwReg Register
  * @param  wCount Counter.
  * @param  wNBlocks no. of Blocks.
  * @retval None
  */

static inline void pcd_set_ep_cnt_rx_reg(__O uint16_t * pdwReg, size_t wCount)  {
  uint32_t wNBlocks;
  if(wCount > 62u)
  {
    wNBlocks = wCount >> 5u;
    if((wCount & 0x1fU) == 0u)
    {
      wNBlocks--;
    }
    wNBlocks = wNBlocks << 10u;
    wNBlocks |= 0x8000u; // Mark block size as 32byte
    *pdwReg = (uint16_t)wNBlocks;
  }
  else
  {
    wNBlocks = wCount >> 1u;
    if((wCount & 0x1U) != 0u)
    {
      wNBlocks++;
    }
    *pdwReg = (uint16_t)((wNBlocks) << 10u);
  }
}


/**
  * @brief  Sets address in an endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @param  bAddr Address.
  * @retval None
  */
static inline void pcd_set_ep_address(USB_TypeDef * USBx,  uint32_t bEpNum, uint32_t bAddr)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= USB_EPREG_MASK;
  regVal |= bAddr;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpNum,regVal);
}

static inline __IO uint16_t * pcd_btable_word_ptr(USB_TypeDef * USBx, size_t x)
{
  size_t total_word_offset = (((USBx)->BTABLE)>>1) + x;
  total_word_offset *= PMA_STRIDE;
  return &(pma[total_word_offset]);
}

// Pointers to the PMA table entries (using the ARM address space)
static inline __IO uint16_t* pcd_ep_tx_address_ptr(USB_TypeDef * USBx, uint32_t bEpNum)
{
  return pcd_btable_word_ptr(USBx,(bEpNum)*4u + 0u);
}
static inline __IO uint16_t* pcd_ep_tx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpNum)
{
  return pcd_btable_word_ptr(USBx,(bEpNum)*4u + 1u);
}

static inline __IO uint16_t* pcd_ep_rx_address_ptr(USB_TypeDef * USBx, uint32_t bEpNum)
{
  return  pcd_btable_word_ptr(USBx,(bEpNum)*4u + 2u);
}

static inline __IO uint16_t* pcd_ep_rx_cnt_ptr(USB_TypeDef * USBx, uint32_t bEpNum)
{
  return pcd_btable_word_ptr(USBx,(bEpNum)*4u + 3u);
}

static inline void pcd_set_ep_tx_cnt(USB_TypeDef * USBx,  uint32_t bEpNum, uint32_t wCount)
{
  *pcd_ep_tx_cnt_ptr(USBx, bEpNum) = (uint16_t)wCount;
}

static inline void pcd_set_ep_rx_cnt(USB_TypeDef * USBx,  uint32_t bEpNum, uint32_t wCount)
{
  __IO uint16_t *pdwReg = pcd_ep_rx_cnt_ptr((USBx),(bEpNum));
  pcd_set_ep_cnt_rx_reg(pdwReg, wCount);
}

/**
  * @brief  sets the status for tx transfer (bits STAT_TX[1:0]).
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @param  wState new state
  * @retval None
  */
static inline void pcd_set_ep_tx_status(USB_TypeDef * USBx,  uint32_t bEpNum, uint32_t wState)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
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
  pcd_set_endpoint(USBx, bEpNum, regVal);
} /* pcd_set_ep_tx_status */

/**
  * @brief  sets the status for rx transfer (bits STAT_TX[1:0])
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @param  wState new state
  * @retval None
  */

static inline void pcd_set_ep_rx_status(USB_TypeDef * USBx,  uint32_t bEpNum, uint32_t wState)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
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
  pcd_set_endpoint(USBx, bEpNum, regVal);
} /* pcd_set_ep_rx_status */

static inline uint32_t pcd_get_ep_rx_status(USB_TypeDef * USBx,  uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  return (regVal & USB_EPRX_STAT) >> (12u);
} /* pcd_get_ep_rx_status */


/**
  * @brief  Toggles DTOG_RX / DTOG_TX bit in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */
static inline void pcd_rx_dtog(USB_TypeDef * USBx,  uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_RX;
  pcd_set_endpoint(USBx, bEpNum, regVal);
}

static inline void pcd_tx_dtog(USB_TypeDef * USBx,  uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX|USB_EP_DTOG_TX;
  pcd_set_endpoint(USBx, bEpNum, regVal);
}

/**
  * @brief  Clears DTOG_RX / DTOG_TX bit in the endpoint register.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */

static inline void pcd_clear_rx_dtog(USB_TypeDef * USBx,  uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  if((regVal & USB_EP_DTOG_RX) != 0)
  {
    pcd_rx_dtog(USBx,bEpNum);
  }
}

static inline void pcd_clear_tx_dtog(USB_TypeDef * USBx,  uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  if((regVal & USB_EP_DTOG_TX) != 0)
  {
    pcd_tx_dtog(USBx,bEpNum);
  }
}

/**
  * @brief  set & clear EP_KIND bit.
  * @param  USBx USB peripheral instance register address.
  * @param  bEpNum Endpoint Number.
  * @retval None
  */

static inline void pcd_set_ep_kind(USB_TypeDef * USBx,  uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal |= USB_EP_KIND;
  regVal &= USB_EPREG_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpNum, regVal);
}
static inline void pcd_clear_ep_kind(USB_TypeDef * USBx, uint32_t bEpNum)
{
  uint32_t regVal = pcd_get_endpoint(USBx, bEpNum);
  regVal &= USB_EPKIND_MASK;
  regVal |= USB_EP_CTR_RX|USB_EP_CTR_TX;
  pcd_set_endpoint(USBx, bEpNum, regVal);
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
#define STFSDEV_EP_COUNT (8u)

#endif /* PORTABLE_ST_STM32F0_DCD_STM32F0_FSDEV_PVT_ST_H_ */
