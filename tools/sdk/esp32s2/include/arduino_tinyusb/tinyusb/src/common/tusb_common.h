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

#ifndef _TUSB_COMMON_H_
#define _TUSB_COMMON_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Macros Helper
//--------------------------------------------------------------------+
#define TU_ARRAY_SIZE(_arr)   ( sizeof(_arr) / sizeof(_arr[0]) )
#define TU_MIN(_x, _y)        ( ( (_x) < (_y) ) ? (_x) : (_y) )
#define TU_MAX(_x, _y)        ( ( (_x) > (_y) ) ? (_x) : (_y) )

#define TU_U16_HIGH(u16)      ((uint8_t) (((u16) >> 8) & 0x00ff))
#define TU_U16_LOW(u16)       ((uint8_t) ((u16)       & 0x00ff))
#define U16_TO_U8S_BE(u16)    TU_U16_HIGH(u16), TU_U16_LOW(u16)
#define U16_TO_U8S_LE(u16)    TU_U16_LOW(u16), TU_U16_HIGH(u16)

#define TU_U32_BYTE3(u32)     ((uint8_t) ((((uint32_t) u32) >> 24) & 0x000000ff)) // MSB
#define TU_U32_BYTE2(u32)     ((uint8_t) ((((uint32_t) u32) >> 16) & 0x000000ff))
#define TU_U32_BYTE1(u32)     ((uint8_t) ((((uint32_t) u32) >>  8) & 0x000000ff))
#define TU_U32_BYTE0(u32)     ((uint8_t) (((uint32_t)  u32)        & 0x000000ff)) // LSB

#define U32_TO_U8S_BE(u32)    TU_U32_BYTE3(u32), TU_U32_BYTE2(u32), TU_U32_BYTE1(u32), TU_U32_BYTE0(u32)
#define U32_TO_U8S_LE(u32)    TU_U32_BYTE0(u32), TU_U32_BYTE1(u32), TU_U32_BYTE2(u32), TU_U32_BYTE3(u32)

#define TU_BIT(n)             (1U << (n))

//--------------------------------------------------------------------+
// Includes
//--------------------------------------------------------------------+

// Standard Headers
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// Tinyusb Common Headers
#include "tusb_option.h"
#include "tusb_compiler.h"
#include "tusb_verify.h"
#include "tusb_types.h"

#include "tusb_error.h"   // TODO remove
#include "tusb_timeout.h" // TODO remove

//------------- Mem -------------//
#define tu_memclr(buffer, size)  memset((buffer), 0, (size))
#define tu_varclr(_var)          tu_memclr(_var, sizeof(*(_var)))

//------------- Bytes -------------//
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_u32(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0)
{
  return ( ((uint32_t) b3) << 24) | ( ((uint32_t) b2) << 16) | ( ((uint32_t) b1) << 8) | b0;
}

TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_u16(uint8_t high, uint8_t low)
{
  return (uint16_t) ((((uint16_t) high) << 8) | low);
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t tu_u32_byte3(uint32_t u32) { return TU_U32_BYTE3(u32); }
TU_ATTR_ALWAYS_INLINE static inline uint8_t tu_u32_byte2(uint32_t u32) { return TU_U32_BYTE2(u32); }
TU_ATTR_ALWAYS_INLINE static inline uint8_t tu_u32_byte1(uint32_t u32) { return TU_U32_BYTE1(u32); }
TU_ATTR_ALWAYS_INLINE static inline uint8_t tu_u32_byte0(uint32_t u32) { return TU_U32_BYTE0(u32); }

TU_ATTR_ALWAYS_INLINE static inline uint8_t tu_u16_high(uint16_t u16) { return TU_U16_HIGH(u16); }
TU_ATTR_ALWAYS_INLINE static inline uint8_t tu_u16_low (uint16_t u16) { return TU_U16_LOW(u16); }

//------------- Bits -------------//
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_bit_set  (uint32_t value, uint8_t pos) { return value | TU_BIT(pos);                  }
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_bit_clear(uint32_t value, uint8_t pos) { return value & (~TU_BIT(pos));               }
TU_ATTR_ALWAYS_INLINE static inline bool     tu_bit_test (uint32_t value, uint8_t pos) { return (value & TU_BIT(pos)) ? true : false; }

//------------- Min -------------//
TU_ATTR_ALWAYS_INLINE static inline uint8_t  tu_min8  (uint8_t  x, uint8_t y ) { return (x < y) ? x : y; }
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_min16 (uint16_t x, uint16_t y) { return (x < y) ? x : y; }
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_min32 (uint32_t x, uint32_t y) { return (x < y) ? x : y; }

//------------- Max -------------//
TU_ATTR_ALWAYS_INLINE static inline uint8_t  tu_max8  (uint8_t  x, uint8_t y ) { return (x > y) ? x : y; }
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_max16 (uint16_t x, uint16_t y) { return (x > y) ? x : y; }
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_max32 (uint32_t x, uint32_t y) { return (x > y) ? x : y; }

//------------- Align -------------//
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_align(uint32_t value, uint32_t alignment)
{
  return value & ((uint32_t) ~(alignment-1));
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_align16 (uint32_t value) { return (value & 0xFFFFFFF0UL); }
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_align32 (uint32_t value) { return (value & 0xFFFFFFE0UL); }
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_align4k (uint32_t value) { return (value & 0xFFFFF000UL); }
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_offset4k(uint32_t value) { return (value & 0xFFFUL); }

//------------- Mathematics -------------//
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_abs(int32_t value) { return (uint32_t)((value < 0) ? (-value) : value); }

/// inclusive range checking TODO remove
TU_ATTR_ALWAYS_INLINE static inline bool tu_within(uint32_t lower, uint32_t value, uint32_t upper)
{
  return (lower <= value) && (value <= upper);
}

// log2 of a value is its MSB's position
// TODO use clz TODO remove
static inline uint8_t tu_log2(uint32_t value)
{
  uint8_t result = 0;
  while (value >>= 1) { result++; }
  return result;
}

//------------- Unaligned Access -------------//
#if TUP_ARCH_STRICT_ALIGN

// Rely on compiler to generate correct code for unaligned access

typedef struct { uint16_t val; } TU_ATTR_PACKED tu_unaligned_uint16_t;
typedef struct { uint32_t val; } TU_ATTR_PACKED tu_unaligned_uint32_t;

TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_unaligned_read32(const void* mem)
{
  tu_unaligned_uint32_t const* ua32 = (tu_unaligned_uint32_t const*) mem;
  return ua32->val;
}

TU_ATTR_ALWAYS_INLINE static inline void tu_unaligned_write32(void* mem, uint32_t value)
{
  tu_unaligned_uint32_t* ua32 = (tu_unaligned_uint32_t*) mem;
  ua32->val = value;
}

TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_unaligned_read16(const void* mem)
{
  tu_unaligned_uint16_t const* ua16 = (tu_unaligned_uint16_t const*) mem;
  return ua16->val;
}

TU_ATTR_ALWAYS_INLINE static inline void tu_unaligned_write16(void* mem, uint16_t value)
{
  tu_unaligned_uint16_t* ua16 = (tu_unaligned_uint16_t*) mem;
  ua16->val = value;
}

#elif TUP_MCU_STRICT_ALIGN

// MCU such as LPC_IP3511 Highspeed cannot access unaligned memory on USB_RAM although it is ARM M4.
// We have to manually pick up bytes since tu_unaligned_uint32_t will still generate unaligned code
// NOTE: volatile cast to memory to prevent compiler to optimize and generate unaligned code
// TODO Big Endian may need minor changes
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_unaligned_read32(const void* mem)
{
  volatile uint8_t const* buf8 = (uint8_t const*) mem;
  return tu_u32(buf8[3], buf8[2], buf8[1], buf8[0]);
}

TU_ATTR_ALWAYS_INLINE static inline void tu_unaligned_write32(void* mem, uint32_t value)
{
  volatile uint8_t* buf8 = (uint8_t*) mem;
  buf8[0] = tu_u32_byte0(value);
  buf8[1] = tu_u32_byte1(value);
  buf8[2] = tu_u32_byte2(value);
  buf8[3] = tu_u32_byte3(value);
}

TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_unaligned_read16(const void* mem)
{
  volatile uint8_t const* buf8 = (uint8_t const*) mem;
  return tu_u16(buf8[1], buf8[0]);
}

TU_ATTR_ALWAYS_INLINE static inline void tu_unaligned_write16(void* mem, uint16_t value)
{
  volatile uint8_t* buf8 = (uint8_t*) mem;
  buf8[0] = tu_u16_low(value);
  buf8[1] = tu_u16_high(value);
}


#else

// MCU that could access unaligned memory natively
TU_ATTR_ALWAYS_INLINE static inline uint32_t tu_unaligned_read32  (const void* mem           ) { return *((uint32_t*) mem);  }
TU_ATTR_ALWAYS_INLINE static inline uint16_t tu_unaligned_read16  (const void* mem           ) { return *((uint16_t*) mem);  }

TU_ATTR_ALWAYS_INLINE static inline void     tu_unaligned_write32 (void* mem, uint32_t value ) { *((uint32_t*) mem) = value; }
TU_ATTR_ALWAYS_INLINE static inline void     tu_unaligned_write16 (void* mem, uint16_t value ) { *((uint16_t*) mem) = value; }

#endif

/*------------------------------------------------------------------*/
/* Count number of arguments of __VA_ARGS__
 * - reference https://groups.google.com/forum/#!topic/comp.std.c/d-6Mj5Lko_s
 * - _GET_NTH_ARG() takes args >= N (64) but only expand to Nth one (64th)
 * - _RSEQ_N() is reverse sequential to N to add padding to have
 * Nth position is the same as the number of arguments
 * - ##__VA_ARGS__ is used to deal with 0 paramerter (swallows comma)
 *------------------------------------------------------------------*/
#ifndef TU_ARGS_NUM

#define TU_ARGS_NUM(...) 	 _TU_NARG(_0, ##__VA_ARGS__,_RSEQ_N())

#define _TU_NARG(...)        _GET_NTH_ARG(__VA_ARGS__)
#define _GET_NTH_ARG( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define _RSEQ_N() \
         62,61,60,                      \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0
#endif

// To be removed
//------------- Binary constant -------------//
#if defined(__GNUC__) && !defined(__CC_ARM)

#define TU_BIN8(x)               ((uint8_t)  (0b##x))
#define TU_BIN16(b1, b2)         ((uint16_t) (0b##b1##b2))
#define TU_BIN32(b1, b2, b3, b4) ((uint32_t) (0b##b1##b2##b3##b4))

#else

//  internal macro of B8, B16, B32
#define _B8__(x) (((x&0x0000000FUL)?1:0) \
                +((x&0x000000F0UL)?2:0) \
                +((x&0x00000F00UL)?4:0) \
                +((x&0x0000F000UL)?8:0) \
                +((x&0x000F0000UL)?16:0) \
                +((x&0x00F00000UL)?32:0) \
                +((x&0x0F000000UL)?64:0) \
                +((x&0xF0000000UL)?128:0))

#define TU_BIN8(d) ((uint8_t) _B8__(0x##d##UL))
#define TU_BIN16(dmsb,dlsb) (((uint16_t)TU_BIN8(dmsb)<<8) + TU_BIN8(dlsb))
#define TU_BIN32(dmsb,db2,db3,dlsb) \
            (((uint32_t)TU_BIN8(dmsb)<<24) \
            + ((uint32_t)TU_BIN8(db2)<<16) \
            + ((uint32_t)TU_BIN8(db3)<<8) \
            + TU_BIN8(dlsb))
#endif

//--------------------------------------------------------------------+
// Debug Function
//--------------------------------------------------------------------+

// CFG_TUSB_DEBUG for debugging
// 0 : no debug
// 1 : print error
// 2 : print warning
// 3 : print info
#if CFG_TUSB_DEBUG

void tu_print_mem(void const *buf, uint32_t count, uint8_t indent);

#ifdef CFG_TUSB_DEBUG_PRINTF
  extern int CFG_TUSB_DEBUG_PRINTF(const char *format, ...);
  #define tu_printf    CFG_TUSB_DEBUG_PRINTF
#else
  #define tu_printf    printf
#endif

static inline
void tu_print_var(uint8_t const* buf, uint32_t bufsize)
{
  for(uint32_t i=0; i<bufsize; i++) tu_printf("%02X ", buf[i]);
}

// Log with Level
#define TU_LOG(n, ...)        TU_LOG##n(__VA_ARGS__)
#define TU_LOG_MEM(n, ...)    TU_LOG##n##_MEM(__VA_ARGS__)
#define TU_LOG_VAR(n, ...)    TU_LOG##n##_VAR(__VA_ARGS__)
#define TU_LOG_INT(n, ...)    TU_LOG##n##_INT(__VA_ARGS__)
#define TU_LOG_HEX(n, ...)    TU_LOG##n##_HEX(__VA_ARGS__)
#define TU_LOG_LOCATION()     tu_printf("%s: %d:\r\n", __PRETTY_FUNCTION__, __LINE__)
#define TU_LOG_FAILED()       tu_printf("%s: %d: Failed\r\n", __PRETTY_FUNCTION__, __LINE__)

// Log Level 1: Error
#define TU_LOG1               tu_printf
#define TU_LOG1_MEM           tu_print_mem
#define TU_LOG1_VAR(_x)       tu_print_var((uint8_t const*)(_x), sizeof(*(_x)))
#define TU_LOG1_INT(_x)       tu_printf(#_x " = %ld\n", (uint32_t) (_x) )
#define TU_LOG1_HEX(_x)       tu_printf(#_x " = %lX\n", (uint32_t) (_x) )

// Log Level 2: Warn
#if CFG_TUSB_DEBUG >= 2
  #define TU_LOG2             TU_LOG1
  #define TU_LOG2_MEM         TU_LOG1_MEM
  #define TU_LOG2_VAR         TU_LOG1_VAR
  #define TU_LOG2_INT         TU_LOG1_INT
  #define TU_LOG2_HEX         TU_LOG1_HEX
#endif

// Log Level 3: Info
#if CFG_TUSB_DEBUG >= 3
  #define TU_LOG3             TU_LOG1
  #define TU_LOG3_MEM         TU_LOG1_MEM
  #define TU_LOG3_VAR         TU_LOG1_VAR
  #define TU_LOG3_INT         TU_LOG1_INT
  #define TU_LOG3_HEX         TU_LOG1_HEX
#endif

typedef struct
{
  uint32_t key;
  const char* data;
} tu_lookup_entry_t;

typedef struct
{
  uint16_t count;
  tu_lookup_entry_t const* items;
} tu_lookup_table_t;

static inline const char* tu_lookup_find(tu_lookup_table_t const* p_table, uint32_t key)
{
  for(uint16_t i=0; i<p_table->count; i++)
  {
    if (p_table->items[i].key == key) return p_table->items[i].data;
  }

  return NULL;
}

#endif // CFG_TUSB_DEBUG

#ifndef TU_LOG
#define TU_LOG(n, ...)
#define TU_LOG_MEM(n, ...)
#define TU_LOG_VAR(n, ...)
#define TU_LOG_INT(n, ...)
#define TU_LOG_HEX(n, ...)
#define TU_LOG_LOCATION()
#define TU_LOG_FAILED()
#endif

// TODO replace all TU_LOGn with TU_LOG(n)
#ifndef TU_LOG1
  #define TU_LOG1(...)
  #define TU_LOG1_MEM(...)
  #define TU_LOG1_VAR(...)
  #define TU_LOG1_INT(...)
  #define TU_LOG1_HEX(...)
#endif

#ifndef TU_LOG2
  #define TU_LOG2(...)
  #define TU_LOG2_MEM(...)
  #define TU_LOG2_VAR(...)
  #define TU_LOG2_INT(...)
  #define TU_LOG2_HEX(...)
#endif

#ifndef TU_LOG3
  #define TU_LOG3(...)
  #define TU_LOG3_MEM(...)
  #define TU_LOG3_VAR(...)
  #define TU_LOG3_INT(...)
  #define TU_LOG3_HEX(...)
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_COMMON_H_ */
