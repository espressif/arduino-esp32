/* 
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the RaspberryPi core for Arduino environment.
 
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef PGMSPACE_INCLUDE
#define PGMSPACE_INCLUDE

typedef void prog_void;
typedef char prog_char;
typedef unsigned char prog_uchar;
typedef char prog_int8_t;
typedef unsigned char prog_uint8_t;
typedef short prog_int16_t;
typedef unsigned short prog_uint16_t;
typedef long prog_int32_t;
typedef unsigned long prog_uint32_t;

typedef char __FlashStringHelper;

#define SIZE_IRRELEVANT 0x7fffffff

#define PROGMEM
#define PGM_P         const char *
#define PGM_VOID_P    const void *
#define FPSTR(p)      ((const char *)(p))
#define PSTR(s)       (s)
#define F(s)          (s)
#define _SFR_BYTE(n)  (n)

#define pgm_read_byte(addr)   (*(const unsigned char *)(addr))
#define pgm_read_word(addr)   (*(const unsigned short *)(addr))
#define pgm_read_dword(addr)  (*(const unsigned long *)(addr))
#define pgm_read_float(addr)  (*(const float *)(addr))

#define pgm_read_byte_near(addr)  pgm_read_byte(addr)
#define pgm_read_word_near(addr)  pgm_read_word(addr)
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#define pgm_read_float_near(addr) pgm_read_float(addr)
#define pgm_read_byte_far(addr)   pgm_read_byte(addr)
#define pgm_read_word_far(addr)   pgm_read_word(addr)
#define pgm_read_dword_far(addr)  pgm_read_dword(addr)
#define pgm_read_float_far(addr)  pgm_read_float(addr)

#define memcmp_P      memcmp
#define memccpy_P     memccpy
#define memmem_P      memmem
#define memcpy_P      memcpy
#define strncpy_P     strncpy
#define strncat_P     strncat
#define strncmp_P     strncmp
#define strncasecmp_P strncasecmp
#define strnlen_P     strnlen
#define strstr_P      strstr
#define printf_P      printf
#define sprintf_P     sprintf
#define snprintf_P    snprintf
#define vsnprintf_P   vsnprintf

#define strlen_P(strP)  strnlen_P((strP), SIZE_IRRELEVANT)
#define strcasecmp_P(str1, str2P) strncasecmp_P((str1), (str2P), SIZE_IRRELEVANT)
#define strcmp_P(str1, str2P) strncmp_P((str1), (str2P), SIZE_IRRELEVANT)
#define strcat_P(dest, src) strncat_P((dest), (src), SIZE_IRRELEVANT)
#define strcpy_P(dest, src) strncpy_P((dest), (src), SIZE_IRRELEVANT)

#endif
