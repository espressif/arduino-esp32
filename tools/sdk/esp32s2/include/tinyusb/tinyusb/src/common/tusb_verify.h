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
 * e.g
 *
 * - TU_VERIFY( cond ) will return false if cond is false
 * - TU_VERIFY( cond, err) will return err instead if cond is false
 *------------------------------------------------------------------*/

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TU_VERIFY Helper
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG
  #include <stdio.h>
  #define _MESS_ERR(_err)         printf("%s: %d: failed, error = %s\n", __func__, __LINE__, tusb_strerr[_err])
  #define _MESS_FAILED()          printf("%s: %d: failed\n", __func__, __LINE__)
#else
  #define _MESS_ERR(_err)
  #define _MESS_FAILED()
#endif

// Halt CPU (breakpoint) when hitting error, only apply for Cortex M3, M4, M7
#if defined(__ARM_ARCH_7M__) || defined (__ARM_ARCH_7EM__)

#define TU_BREAKPOINT() \
  do {\
    volatile uint32_t* ARM_CM_DHCSR =  ((volatile uint32_t*) 0xE000EDF0UL); /* Cortex M CoreDebug->DHCSR */ \
    if ( (*ARM_CM_DHCSR) & 1UL ) __asm("BKPT #0\n"); /* Only halt mcu if debugger is attached */\
  } while(0)

#else
#define TU_BREAKPOINT()
#endif

/*------------------------------------------------------------------*/
/* Macro Generator
 *------------------------------------------------------------------*/

// Helper to implement optional parameter for TU_VERIFY Macro family
#define GET_3RD_ARG(arg1, arg2, arg3, ...)        arg3
#define GET_4TH_ARG(arg1, arg2, arg3, arg4, ...)  arg4

/*------------- Generator for TU_VERIFY and TU_VERIFY_HDLR -------------*/
#define TU_VERIFY_DEFINE(_cond, _handler, _ret)  do { if ( !(_cond) ) { _handler; return _ret;  } } while(0)

/*------------- Generator for TU_VERIFY_ERR and TU_VERIFY_ERR_HDLR -------------*/
#define TU_VERIFY_ERR_DEF2(_error, _handler)  \
    do {                                              \
      uint32_t _err = (uint32_t)(_error);             \
      if ( 0 != _err ) { _MESS_ERR(_err); _handler; return _err; }\
    } while(0)

#define TU_VERIFY_ERR_DEF3(_error, _handler, _ret)  \
    do {                                              \
      uint32_t _err = (uint32_t)(_error);             \
      if ( 0 != _err ) { _MESS_ERR(_err); _handler; return _ret; }\
    } while(0)




/*------------------------------------------------------------------*/
/* TU_VERIFY
 * - TU_VERIFY_1ARGS : return false if failed
 * - TU_VERIFY_2ARGS : return provided value if failed
 *------------------------------------------------------------------*/
#define TU_VERIFY_1ARGS(_cond)                         TU_VERIFY_DEFINE(_cond, , false)
#define TU_VERIFY_2ARGS(_cond, _ret)                   TU_VERIFY_DEFINE(_cond, , _ret)

#define TU_VERIFY(...)                   GET_3RD_ARG(__VA_ARGS__, TU_VERIFY_2ARGS, TU_VERIFY_1ARGS)(__VA_ARGS__)


/*------------------------------------------------------------------*/
/* TU_VERIFY WITH HANDLER
 * - TU_VERIFY_HDLR_2ARGS : execute handler, return false if failed
 * - TU_VERIFY_HDLR_3ARGS : execute handler, return provided error if failed
 *------------------------------------------------------------------*/
#define TU_VERIFY_HDLR_2ARGS(_cond, _handler)           TU_VERIFY_DEFINE(_cond, _handler, false)
#define TU_VERIFY_HDLR_3ARGS(_cond, _handler, _ret)     TU_VERIFY_DEFINE(_cond, _handler, _ret)

#define TU_VERIFY_HDLR(...)              GET_4TH_ARG(__VA_ARGS__, TU_VERIFY_HDLR_3ARGS, TU_VERIFY_HDLR_2ARGS)(__VA_ARGS__)


/*------------------------------------------------------------------*/
/* TU_VERIFY STATUS
 * - TU_VERIFY_ERR_1ARGS : return status of condition if failed
 * - TU_VERIFY_ERR_2ARGS : return provided status code if failed
 *------------------------------------------------------------------*/
#define TU_VERIFY_ERR_1ARGS(_error)                       TU_VERIFY_ERR_DEF2(_error, )
#define TU_VERIFY_ERR_2ARGS(_error, _ret)                 TU_VERIFY_ERR_DEF3(_error, ,_ret)

#define TU_VERIFY_ERR(...)            GET_3RD_ARG(__VA_ARGS__, TU_VERIFY_ERR_2ARGS, TU_VERIFY_ERR_1ARGS)(__VA_ARGS__)

/*------------------------------------------------------------------*/
/* TU_VERIFY STATUS WITH HANDLER
 * - TU_VERIFY_ERR_HDLR_2ARGS : execute handler, return status if failed
 * - TU_VERIFY_ERR_HDLR_3ARGS : execute handler, return provided error if failed
 *------------------------------------------------------------------*/
#define TU_VERIFY_ERR_HDLR_2ARGS(_error, _handler)        TU_VERIFY_ERR_DEF2(_error, _handler)
#define TU_VERIFY_ERR_HDLR_3ARGS(_error, _handler, _ret)  TU_VERIFY_ERR_DEF3(_error, _handler, _ret)

#define TU_VERIFY_ERR_HDLR(...)       GET_4TH_ARG(__VA_ARGS__, TU_VERIFY_ERR_HDLR_3ARGS, TU_VERIFY_ERR_HDLR_2ARGS)(__VA_ARGS__)



/*------------------------------------------------------------------*/
/* ASSERT
 * basically TU_VERIFY with TU_BREAKPOINT() as handler
 * - 1 arg : return false if failed
 * - 2 arg : return error if failed
 *------------------------------------------------------------------*/
#define ASSERT_1ARGS(_cond)            TU_VERIFY_DEFINE(_cond, _MESS_FAILED(); TU_BREAKPOINT(), false)
#define ASSERT_2ARGS(_cond, _ret)      TU_VERIFY_DEFINE(_cond, _MESS_FAILED(); TU_BREAKPOINT(), _ret)

#define TU_ASSERT(...)             GET_3RD_ARG(__VA_ARGS__, ASSERT_2ARGS, ASSERT_1ARGS)(__VA_ARGS__)

/*------------------------------------------------------------------*/
/* ASSERT Error
 * basically TU_VERIFY Error with TU_BREAKPOINT() as handler
 *------------------------------------------------------------------*/
#define ASERT_ERR_1ARGS(_error)         TU_VERIFY_ERR_DEF2(_error, TU_BREAKPOINT())
#define ASERT_ERR_2ARGS(_error, _ret)   TU_VERIFY_ERR_DEF3(_error, TU_BREAKPOINT(), _ret)

#define TU_ASSERT_ERR(...)         GET_3RD_ARG(__VA_ARGS__, ASERT_ERR_2ARGS, ASERT_ERR_1ARGS)(__VA_ARGS__)

/*------------------------------------------------------------------*/
/* ASSERT HDLR
 *------------------------------------------------------------------*/

#ifdef __cplusplus
 }
#endif

#endif /* TUSB_VERIFY_H_ */
