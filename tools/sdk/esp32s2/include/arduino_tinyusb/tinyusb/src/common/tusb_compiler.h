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

/** \ingroup Group_Common
 *  \defgroup Group_Compiler Compiler
 *  \brief Group_Compiler brief
 *  @{ */

#ifndef _TUSB_COMPILER_H_
#define _TUSB_COMPILER_H_

#define TU_TOKEN(x)           x
#define TU_STRING(x)          #x                  ///< stringify without expand
#define TU_XSTRING(x)         TU_STRING(x)        ///< expand then stringify

#define TU_STRCAT(a, b)       a##b                ///< concat without expand
#define TU_STRCAT3(a, b, c)   a##b##c             ///< concat without expand

#define TU_XSTRCAT(a, b)      TU_STRCAT(a, b)     ///< expand then concat
#define TU_XSTRCAT3(a, b, c)  TU_STRCAT3(a, b, c) ///< expand then concat 3 tokens

#define TU_INCLUDE_PATH(_dir,_file) TU_XSTRING( TU_TOKEN(_dir)TU_TOKEN(_file) )

#if defined __COUNTER__ && __COUNTER__ != __COUNTER__
  #define _TU_COUNTER_ __COUNTER__
#else
  #define _TU_COUNTER_ __LINE__
#endif

// Compile-time Assert
#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  #define TU_VERIFY_STATIC   _Static_assert
#elif defined (__cplusplus) && __cplusplus >= 201103L
  #define TU_VERIFY_STATIC   static_assert
#elif defined(__CCRX__)
  #define TU_VERIFY_STATIC(const_expr, _mess) typedef char TU_XSTRCAT(Line, __LINE__)[(const_expr) ? 1 : 0];
#else
  #define TU_VERIFY_STATIC(const_expr, _mess) enum { TU_XSTRCAT(_verify_static_, _TU_COUNTER_) = 1/(!!(const_expr)) }
#endif

// for declaration of reserved field, make use of _TU_COUNTER_
#define TU_RESERVED           TU_XSTRCAT(reserved, _TU_COUNTER_)

#define TU_LITTLE_ENDIAN (0x12u)
#define TU_BIG_ENDIAN (0x21u)

//--------------------------------------------------------------------+
// Compiler porting with Attribute and Endian
//--------------------------------------------------------------------+

// TODO refactor since __attribute__ is supported across many compiler
#if defined(__GNUC__)
  #define TU_ATTR_ALIGNED(Bytes)        __attribute__ ((aligned(Bytes)))
  #define TU_ATTR_SECTION(sec_name)     __attribute__ ((section(#sec_name)))
  #define TU_ATTR_PACKED                __attribute__ ((packed))
  #define TU_ATTR_WEAK                  __attribute__ ((weak))
  #define TU_ATTR_ALWAYS_INLINE         __attribute__ ((always_inline))
  #define TU_ATTR_DEPRECATED(mess)      __attribute__ ((deprecated(mess))) // warn if function with this attribute is used
  #define TU_ATTR_UNUSED                __attribute__ ((unused))           // Function/Variable is meant to be possibly unused
  #define TU_ATTR_USED                  __attribute__ ((used))             // Function/Variable is meant to be used

  #define TU_ATTR_PACKED_BEGIN
  #define TU_ATTR_PACKED_END
  #define TU_ATTR_BIT_FIELD_ORDER_BEGIN
  #define TU_ATTR_BIT_FIELD_ORDER_END

  // Endian conversion use well-known host to network (big endian) naming
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define TU_BYTE_ORDER TU_LITTLE_ENDIAN
  #else
    #define TU_BYTE_ORDER TU_BIG_ENDIAN
  #endif

  #define TU_BSWAP16(u16) (__builtin_bswap16(u16))
  #define TU_BSWAP32(u32) (__builtin_bswap32(u32))

  // List of obsolete callback function that is renamed and should not be defined.
  // Put it here since only gcc support this pragma
  #pragma GCC poison tud_vendor_control_request_cb

#elif defined(__TI_COMPILER_VERSION__)
  #define TU_ATTR_ALIGNED(Bytes)        __attribute__ ((aligned(Bytes)))
  #define TU_ATTR_SECTION(sec_name)     __attribute__ ((section(#sec_name)))
  #define TU_ATTR_PACKED                __attribute__ ((packed))
  #define TU_ATTR_WEAK                  __attribute__ ((weak))
  #define TU_ATTR_ALWAYS_INLINE         __attribute__ ((always_inline))
  #define TU_ATTR_DEPRECATED(mess)      __attribute__ ((deprecated(mess))) // warn if function with this attribute is used
  #define TU_ATTR_UNUSED                __attribute__ ((unused))           // Function/Variable is meant to be possibly unused
  #define TU_ATTR_USED                  __attribute__ ((used))

  #define TU_ATTR_PACKED_BEGIN
  #define TU_ATTR_PACKED_END
  #define TU_ATTR_BIT_FIELD_ORDER_BEGIN
  #define TU_ATTR_BIT_FIELD_ORDER_END

  // __BYTE_ORDER is defined in the TI ARM compiler, but not MSP430 (which is little endian)
  #if ((__BYTE_ORDER__) == (__ORDER_LITTLE_ENDIAN__)) || defined(__MSP430__)
    #define TU_BYTE_ORDER TU_LITTLE_ENDIAN
  #else
    #define TU_BYTE_ORDER TU_BIG_ENDIAN
  #endif

  #define TU_BSWAP16(u16) (__builtin_bswap16(u16))
  #define TU_BSWAP32(u32) (__builtin_bswap32(u32))

#elif defined(__ICCARM__)
  #include <intrinsics.h>
  #define TU_ATTR_ALIGNED(Bytes)        __attribute__ ((aligned(Bytes)))
  #define TU_ATTR_SECTION(sec_name)     __attribute__ ((section(#sec_name)))
  #define TU_ATTR_PACKED                __attribute__ ((packed))
  #define TU_ATTR_WEAK                  __attribute__ ((weak))
  #define TU_ATTR_ALWAYS_INLINE         __attribute__ ((always_inline))
  #define TU_ATTR_DEPRECATED(mess)      __attribute__ ((deprecated(mess))) // warn if function with this attribute is used
  #define TU_ATTR_UNUSED                __attribute__ ((unused))           // Function/Variable is meant to be possibly unused
  #define TU_ATTR_USED                  __attribute__ ((used))             // Function/Variable is meant to be used

  #define TU_ATTR_PACKED_BEGIN
  #define TU_ATTR_PACKED_END
  #define TU_ATTR_BIT_FIELD_ORDER_BEGIN
  #define TU_ATTR_BIT_FIELD_ORDER_END

  // Endian conversion use well-known host to network (big endian) naming
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define TU_BYTE_ORDER TU_LITTLE_ENDIAN
  #else
    #define TU_BYTE_ORDER TU_BIG_ENDIAN
  #endif

  #define TU_BSWAP16(u16) (__iar_builtin_REV16(u16))
  #define TU_BSWAP32(u32) (__iar_builtin_REV(u32))

#elif defined(__CCRX__)
  #define TU_ATTR_ALIGNED(Bytes)
  #define TU_ATTR_SECTION(sec_name)
  #define TU_ATTR_PACKED
  #define TU_ATTR_WEAK
  #define TU_ATTR_ALWAYS_INLINE
  #define TU_ATTR_DEPRECATED(mess)
  #define TU_ATTR_UNUSED
  #define TU_ATTR_USED

  #define TU_ATTR_PACKED_BEGIN          _Pragma("pack")
  #define TU_ATTR_PACKED_END            _Pragma("packoption")
  #define TU_ATTR_BIT_FIELD_ORDER_BEGIN _Pragma("bit_order right")
  #define TU_ATTR_BIT_FIELD_ORDER_END   _Pragma("bit_order")

  // Endian conversion use well-known host to network (big endian) naming
  #if defined(__LIT)
    #define TU_BYTE_ORDER TU_LITTLE_ENDIAN
  #else
    #define TU_BYTE_ORDER TU_BIG_ENDIAN
  #endif

  #define TU_BSWAP16(u16) ((unsigned short)_builtin_revw((unsigned long)u16))
  #define TU_BSWAP32(u32) (_builtin_revl(u32))

#else 
  #error "Compiler attribute porting is required"
#endif

#if (TU_BYTE_ORDER == TU_LITTLE_ENDIAN)

  #define tu_htons(u16)  (TU_BSWAP16(u16))
  #define tu_ntohs(u16)  (TU_BSWAP16(u16))

  #define tu_htonl(u32)  (TU_BSWAP32(u32))
  #define tu_ntohl(u32)  (TU_BSWAP32(u32))

  #define tu_htole16(u16) (u16)
  #define tu_le16toh(u16) (u16)

  #define tu_htole32(u32) (u32)
  #define tu_le32toh(u32) (u32)

#elif (TU_BYTE_ORDER == TU_BIG_ENDIAN)

  #define tu_htons(u16)  (u16)
  #define tu_ntohs(u16)  (u16)

  #define tu_htonl(u32)  (u32)
  #define tu_ntohl(u32)  (u32)

  #define tu_htole16(u16) (TU_BSWAP16(u16))
  #define tu_le16toh(u16) (TU_BSWAP16(u16))

  #define tu_htole32(u32) (TU_BSWAP32(u32))
  #define tu_le32toh(u32) (TU_BSWAP32(u32))

#else
  #error Byte order is undefined
#endif

#endif /* _TUSB_COMPILER_H_ */

/// @}
