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

void randomSeed(unsigned long seed)
{
    if(seed != 0) {
        srand(seed);
    }
}

long random(long howbig)
{
    uint32_t x = esp_random();
    uint64_t m = uint64_t(x) * uint64_t(howbig);
    uint32_t l = uint32_t(m);
    if (l < howbig) {
        uint32_t t = -howbig;
        if (t >= howbig) {
            t -= howbig;
            if (t >= howbig)
                t %= howbig;
        }
        while (l < t) {
            x = esp_random();
            m = uint64_t(x) * uint64_t(howbig);
            l = uint32_t(m);
        }
    }
    return m >> 32;
}

long random(long howsmall, long howbig)
{
    if(howsmall >= howbig) {
        return howsmall;
    }
    long diff = howbig - howsmall;
    return random(diff) + howsmall;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    const long dividend = out_max - out_min;
    const long divisor = in_max - in_min;
    const long delta = x - in_min;

    return (delta * dividend + (divisor / 2)) / divisor + out_min;
}

unsigned int makeWord(unsigned int w)
{
    return w;
}

unsigned int makeWord(unsigned char h, unsigned char l)
{
    return (h << 8) | l;
}
