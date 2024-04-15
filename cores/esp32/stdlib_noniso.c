/*
 core_esp8266_noniso.c - nonstandard (but useful) conversion functions

 Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

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

 Modified 03 April 2015 by Markus Sattler

 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "stdlib_noniso.h"
#include "esp_system.h"

static void reverse(char* begin, char* end) {
  char* is = begin;
  char* ie = end - 1;
  while (is < ie) {
    char tmp = *ie;
    *ie = *is;
    *is = tmp;
    ++is;
    --ie;
  }
}

char* ltoa(long value, char* result, int base) {
  if (base < 2 || base > 16) {
    *result = 0;
    return result;
  }

  char* out = result;
  long quotient = abs(value);

  do {
    const long tmp = quotient / base;
    *out = "0123456789abcdef"[quotient - (tmp * base)];
    ++out;
    quotient = tmp;
  } while (quotient);

  // Apply negative sign
  if (value < 0)
    *out++ = '-';

  reverse(result, out);
  *out = 0;
  return result;
}

char* lltoa(long long val, char* result, int base) {
  if (base < 2 || base > 16) {
    *result = 0;
    return result;
  }

  char* out = result;
  long long quotient = val > 0 ? val : -val;

  do {
    const long long tmp = quotient / base;
    *out = "0123456789abcdef"[quotient - (tmp * base)];
    ++out;
    quotient = tmp;
  } while (quotient);

  // Apply negative sign
  if (val < 0)
    *out++ = '-';

  reverse(result, out);
  *out = 0;
  return result;
}

char* ultoa(unsigned long value, char* result, int base) {
  if (base < 2 || base > 16) {
    *result = 0;
    return result;
  }

  char* out = result;
  unsigned long quotient = value;

  do {
    const unsigned long tmp = quotient / base;
    *out = "0123456789abcdef"[quotient - (tmp * base)];
    ++out;
    quotient = tmp;
  } while (quotient);

  reverse(result, out);
  *out = 0;
  return result;
}

char* ulltoa(unsigned long long val, char* result, int base) {
  if (base < 2 || base > 16) {
    *result = 0;
    return result;
  }

  char* out = result;
  unsigned long long quotient = val;

  do {
    const unsigned long long tmp = quotient / base;
    *out = "0123456789abcdef"[quotient - (tmp * base)];
    ++out;
    quotient = tmp;
  } while (quotient);

  reverse(result, out);
  *out = 0;
  return result;
}

char* dtostrf(double number, signed int width, unsigned int prec, char* s) {
  bool negative = false;

  if (isnan(number)) {
    strcpy(s, "nan");
    return s;
  }
  if (isinf(number)) {
    strcpy(s, "inf");
    return s;
  }

  char* out = s;

  int fillme = width;  // how many cells to fill for the integer part
  if (prec > 0) {
    fillme -= (prec + 1);
  }

  // Handle negative numbers
  if (number < 0.0) {
    negative = true;
    fillme--;
    number = -number;
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  // I optimized out most of the divisions
  double rounding = 2.0;
  for (unsigned int i = 0; i < prec; ++i)
    rounding *= 10.0;
  rounding = 1.0 / rounding;

  number += rounding;

  // Figure out how big our number really is
  double tenpow = 1.0;
  unsigned int digitcount = 1;
  while (number >= 10.0 * tenpow) {
    tenpow *= 10.0;
    digitcount++;
  }

  number /= tenpow;
  fillme -= digitcount;

  // Pad unused cells with spaces
  while (fillme-- > 0) {
    *out++ = ' ';
  }

  // Handle negative sign
  if (negative) *out++ = '-';

  // Print the digits, and if necessary, the decimal point
  digitcount += prec;
  int8_t digit = 0;
  while (digitcount-- > 0) {
    digit = (int8_t)number;
    if (digit > 9) digit = 9;  // insurance
    *out++ = (char)('0' | digit);
    if ((digitcount == prec) && (prec > 0)) {
      *out++ = '.';
    }
    number -= digit;
    number *= 10.0;
  }

  // make sure the string is terminated
  *out = 0;
  return s;
}
