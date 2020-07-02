/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Ha Thach (tinyusb.org)
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


#ifndef BOARD_MCU_H_
#define BOARD_MCU_H_

#include "tusb_option.h"

//--------------------------------------------------------------------+
// Low Level MCU header include. TinyUSB stack and example should be
// platform independent and mostly doens't need to include this file.
// However there are still certain situation where this file is needed:
// - FreeRTOSConfig.h to set up correct clock and NVIC interrupts for ARM Cortex
// - SWO logging for Cortex M with ITM_SendChar() / ITM_ReceiveChar()
//--------------------------------------------------------------------+

// Include order follows OPT_MCU_ number
#if   CFG_TUSB_MCU == OPT_MCU_LPC11UXX   || CFG_TUSB_MCU == OPT_MCU_LPC13XX    || \
      CFG_TUSB_MCU == OPT_MCU_LPC15XX    || CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || \
      CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC18XX    || \
      CFG_TUSB_MCU == OPT_MCU_LPC40XX    || CFG_TUSB_MCU == OPT_MCU_LPC43XX
  #include "chip.h"

#elif CFG_TUSB_MCU == OPT_MCU_LPC51UXX || CFG_TUSB_MCU == OPT_MCU_LPC54XXX || \
      CFG_TUSB_MCU == OPT_MCU_LPC55XX
  #include "fsl_device_registers.h"

#elif CFG_TUSB_MCU == OPT_MCU_NRF5X
  #include "nrf.h"

#elif CFG_TUSB_MCU == OPT_MCU_SAMD21 || CFG_TUSB_MCU == OPT_MCU_SAMD51
  #include "sam.h"

#elif CFG_TUSB_MCU == OPT_MCU_SAMG
  #undef LITTLE_ENDIAN // hack to suppress "LITTLE_ENDIAN" redefined
  #include "sam.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32F0
  #include "stm32f0xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32F1
  #include "stm32f1xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32F2
  #include "stm32f2xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32F3
  #include "stm32f3xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32F4
  #include "stm32f4xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32F7
  #include "stm32f7xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32H7
  #include "stm32h7xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32L0
  #include "stm32l0xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32L1
  #include "stm32l1xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_STM32L4
  #include "stm32l4xx.h"

#elif CFG_TUSB_MCU == OPT_MCU_CXD56
  // no header needed

#elif CFG_TUSB_MCU == OPT_MCU_MSP430x5xx
  #include "msp430.h"

#elif CFG_TUSB_MCU == OPT_MCU_VALENTYUSB_EPTRI
  // no header needed

#elif CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  #include "fsl_device_registers.h"

#elif CFG_TUSB_MCU == OPT_MCU_NUC120
  #include "NUC100Series.h"

#elif CFG_TUSB_MCU == OPT_MCU_NUC121 || CFG_TUSB_MCU == OPT_MCU_NUC126
  #include "NuMicro.h"

#elif CFG_TUSB_MCU == OPT_MCU_NUC505
  #include "NUC505Series.h"

#elif CFG_TUSB_MCU == OPT_MCU_ESP32S2
  // no header needed

#elif CFG_TUSB_MCU == OPT_MCU_DA1469X
  #include "DA1469xAB.h"

#else
  #error "Missing MCU header"
#endif


#endif /* BOARD_MCU_H_ */
