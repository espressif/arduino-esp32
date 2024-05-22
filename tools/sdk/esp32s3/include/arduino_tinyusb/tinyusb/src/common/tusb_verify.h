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
#ifndef TUSB_VERIFY_H_
#define TUSB_VERIFY_H_

#include <stdbool.h>
#include <stdint.h>
#include "tusb_option.h"
#include "tusb_compiler.h"

/*------------------------------------------------------------------*/
/* This file use an advanced macro technique to mimic the default parameter
 * as C++ for the sake of code simplicity. Beware of a headache macro
 * manipulation that you are told to stay away.
 *
 * This contains macros for both VERIFY and ASSERT:
 *
 *   VERIFY: Used when there is an error condition which is not the
 *           fault of the MCU. For example, bounds checking on data
 *           sent to the micro over USB should use this function.
 *           Another example is checking for buffer overflows, where
 *           returning from the active function causes a NAK.
 *
 *   ASSERT: Used for error conditions that are caused by MCU firmware
 *           bugs. This is used to discover bugs in the code more
 *           quickly. One example would be adding assertions in library
 *           function calls to confirm a function's (untainted)
 *           parameters are valid.
 *
 * The difference in behavior is that ASSERT triggers a breakpoint while
 * verify does not.
 *
 *   #define TU_VERIFY(cond)                  if(cond) return false;
 *   #define TU_VERIFY(cond,ret)              if(cond) return ret;
 *
 *   #define TU_ASSERT(cond)                  if(cond) {_MESS_FAILED(); TU_BREAKPOINT(), return false;}
 *   #define TU_ASSERT(cond,ret)              if(cond) {_MESS_FAILED(); TU_BREAKPOINT(), return ret;}
 *------------------------------------------------------------------*/

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TU_VERIFY Helper
//--------------------------------------------------------------------+

#if CFG_TUSB_DEBUG
  #include <stdio.h>
  #define _MESS_FAILED()    tu_printf("%s %d: ASSERT FAILED\r\n", __func__, __LINE__)
#else
  #define _MESS_FAILED() do {} while (0)
#endif

// Halt CPU (breakpoint) when hitting error, only apply for Cortex M3, M4, M7, M33. M55
#if defined(__ARM_ARCH_7M__) || defined (__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8_1M_MAIN__) || \
    defined(__ARM7M__) || defined (__ARM7EM__) || defined(__ARM8M_MAINLINE__) || defined(__ARM8EM_MAINLINE__)
  #define TU_BREAKPOINT() do {                                                                              \
    volatile uint32_t* ARM_CM_DHCSR =  ((volatile uint32_t*) 0xE000EDF0UL); /* Cortex M CoreDebug->DHCSR */ \
    if ( (*ARM_CM_DHCSR) & 1UL ) __asm("BKPT #0\n"); /* Only halt mcu if debugger is attached */            \
  } while(0)

#elif defined(__riscv) && !TUP_MCU_ESPRESSIF
  #define TU_BREAKPOINT() do { __asm("ebreak\n"); } while(0)

#elif defined(_mips)
  #define TU_BREAKPOINT() do { __asm("sdbbp 0"); } while (0)

#else
  #define TU_BREAKPOINT() do {} while (0)
#endif

// Helper to implement optional parameter for TU_VERIFY Macro family
#define _GET_3RD_ARG(arg1, arg2, arg3, ...)        arg3

/*------------------------------------------------------------------*/
/* TU_VERIFY
 * - TU_VERIFY_1ARGS : return false if failed
 * - TU_VERIFY_2ARGS : return provided value if failed
 *------------------------------------------------------------------*/
#define TU_VERIFY_DEFINE(_cond, _ret)    \
  do {                                   \
    if ( !(_cond) ) { return _ret; }     \
  } while(0)

#define TU_VERIFY_1ARGS(_cond)         TU_VERIFY_DEFINE(_cond, false)
#define TU_VERIFY_2ARGS(_cond, _ret)   TU_VERIFY_DEFINE(_cond, _ret)

#define TU_VERIFY(...)                 _GET_3RD_ARG(__VA_ARGS__, TU_VERIFY_2ARGS, TU_VERIFY_1ARGS, _dummy)(__VA_ARGS__)

/*------------------------------------------------------------------*/
/* ASSERT
 * basically TU_VERIFY with TU_BREAKPOINT() as handler
 * - 1 arg : return false if failed
 * - 2 arg : return error if failed
 *------------------------------------------------------------------*/
#define TU_ASSERT_DEFINE(_cond, _ret)                                 \
  do {                                                                \
    if ( !(_cond) ) { _MESS_FAILED(); TU_BREAKPOINT(); return _ret; } \
  } while(0)

#define TU_ASSERT_1ARGS(_cond)         TU_ASSERT_DEFINE(_cond, false)
#define TU_ASSERT_2ARGS(_cond, _ret)   TU_ASSERT_DEFINE(_cond, _ret)

#ifndef TU_ASSERT
#define TU_ASSERT(...)                 _GET_3RD_ARG(__VA_ARGS__, TU_ASSERT_2ARGS, TU_ASSERT_1ARGS, _dummy)(__VA_ARGS__)
#endif

#ifdef __cplusplus
 }
#endif

#endif
