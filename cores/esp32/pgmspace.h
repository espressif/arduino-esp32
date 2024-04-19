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

#define PROGMEM
#define PGM_P        const char *
#define PGM_VOID_P   const void *
#define PSTR(s)      (s)
#define _SFR_BYTE(n) (n)

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr)           \
  ({                                  \
    typeof(addr) _addr = (addr);      \
    *(const unsigned short *)(_addr); \
  })
#define pgm_read_dword(addr)         \
  ({                                 \
    typeof(addr) _addr = (addr);     \
    *(const unsigned long *)(_addr); \
  })
#define pgm_read_float(addr)     \
  ({                             \
    typeof(addr) _addr = (addr); \
    *(const float *)(_addr);     \
  })
#define pgm_read_ptr(addr)       \
  ({                             \
    typeof(addr) _addr = (addr); \
    *(void *const *)(_addr);     \
  })

#define pgm_get_far_address(x) ((uint32_t)(&(x)))

#define pgm_read_byte_near(addr)  pgm_read_byte(addr)
#define pgm_read_word_near(addr)  pgm_read_word(addr)
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#define pgm_read_float_near(addr) pgm_read_float(addr)
#define pgm_read_ptr_near(addr)   pgm_read_ptr(addr)
#define pgm_read_byte_far(addr)   pgm_read_byte(addr)
#define pgm_read_word_far(addr)   pgm_read_word(addr)
#define pgm_read_dword_far(addr)  pgm_read_dword(addr)
#define pgm_read_float_far(addr)  pgm_read_float(addr)
#define pgm_read_ptr_far(addr)    pgm_read_ptr(addr)

#define memcmp_P      memcmp
#define memccpy_P     memccpy
#define memmem_P      memmem
#define memcpy_P      memcpy
#define strcpy_P      strcpy
#define strncpy_P     strncpy
#define strcat_P      strcat
#define strncat_P     strncat
#define strcmp_P      strcmp
#define strncmp_P     strncmp
#define strcasecmp_P  strcasecmp
#define strncasecmp_P strncasecmp
#define strlen_P      strlen
#define strnlen_P     strnlen
#define strstr_P      strstr
#define printf_P      printf
#define sprintf_P     sprintf
#define snprintf_P    snprintf
#define vsnprintf_P   vsnprintf

#endif
