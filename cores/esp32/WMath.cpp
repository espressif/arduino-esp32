/* -*- mode: jde; c-basic-offset: 2; indent-tabs-mode: nil -*- */

/*
 Part of the Wiring project - http://wiring.org.co
 Copyright (c) 2004-06 Hernando Barragan
 Modified 13 August 2006, David A. Mellis for Arduino - http://www.arduino.cc/

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General
 Public License along with this library; if not, write to the
 Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 Boston, MA  02111-1307  USA

 $Id$
 */

extern "C" {
#include <stdlib.h>
#include "esp_system.h"
}
#include "esp32-hal-log.h"
#include "esp_random.h"

// Allows the user to choose between Real Hardware
// or Software Pseudo random generators for the
// Arduino random() functions
static bool s_useRandomHW = true;
void useRealRandomGenerator(bool useRandomHW) {
  s_useRandomHW = useRandomHW;
}

// Calling randomSeed() will force the
// Pseudo Random generator like in
// Arduino mainstream API
void randomSeed(unsigned long seed) {
  if (seed != 0) {
    srand(seed);
    s_useRandomHW = false;
  }
}

long random(long howsmall, long howbig);
long random(long howbig) {
  if (howbig == 0) {
    return 0;
  }
  if (howbig < 0) {
    return (random(0, -howbig));
  }
  // if randomSeed was called, fall back to software PRNG
  uint32_t val = (s_useRandomHW) ? esp_random() : rand();
  return val % howbig;
}

long random(long howsmall, long howbig) {
  if (howsmall >= howbig) {
    return howsmall;
  }
  long diff = howbig - howsmall;
  return random(diff) + howsmall;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  const long run = in_max - in_min;
  if (run == 0) {
    log_e("map(): Invalid input range, min == max");
    return -1;  // AVR returns -1, SAM returns 0
  }
  const long rise = out_max - out_min;
  const long delta = x - in_min;
  return (delta * rise) / run + out_min;
}

uint16_t makeWord(uint16_t w) {
  return w;
}

uint16_t makeWord(uint8_t h, uint8_t l) {
  return (h << 8) | l;
}
