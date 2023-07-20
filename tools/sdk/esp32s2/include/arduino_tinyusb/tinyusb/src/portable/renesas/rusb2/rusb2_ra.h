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

#ifndef _RUSB2_RA_H_
#define _RUSB2_RA_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wundef"

// extra push due to https://github.com/renesas/fsp/pull/278
#pragma GCC diagnostic push
#endif

/* renesas fsp api */
#include "bsp_api.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#define RUSB2_REG_BASE (0x40090000)

#if defined(__ICCARM__)
  #define __builtin_ctz(x)             __iar_builtin_CLZ(__iar_builtin_RBIT(x))
#endif

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_enable(uint8_t rhport)
{
  (void) rhport;
  NVIC_EnableIRQ(TU_IRQn);
}

TU_ATTR_ALWAYS_INLINE static inline void rusb2_int_disable(uint8_t rhport)
{
  (void) rhport;
  NVIC_DisableIRQ(TU_IRQn);
}

// MCU specific PHY init
TU_ATTR_ALWAYS_INLINE static inline void rusb2_phy_init(void)
{
}

#ifdef __cplusplus
}
#endif

#endif /* _RUSB2_RA_H_ */
