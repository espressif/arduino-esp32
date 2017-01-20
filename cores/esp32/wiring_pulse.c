/*
  pulse.c - wiring pulseIn implementation for esp8266
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
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
*/
//#include <limits.h>
#include "wiring_private.h"
#include "pins_arduino.h"


extern uint32_t xthal_get_ccount();

#define WAIT_FOR_PIN_STATE(state) \
    while (digitalRead(pin) != (state)) { \
        if (xthal_get_ccount() - start_cycle_count > timeout_cycles) { \
            return 0; \
        } \
    }

// max timeout is 27 seconds at 160MHz clock and 54 seconds at 80MHz clock
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout)
{
    const uint32_t max_timeout_us = clockCyclesToMicroseconds(UINT_MAX);
    if (timeout > max_timeout_us) {
        timeout = max_timeout_us;
    }
    const uint32_t timeout_cycles = microsecondsToClockCycles(timeout);
    const uint32_t start_cycle_count = xthal_get_ccount();
    WAIT_FOR_PIN_STATE(!state);
    WAIT_FOR_PIN_STATE(state);
    const uint32_t pulse_start_cycle_count = xthal_get_ccount();
    WAIT_FOR_PIN_STATE(!state);
    return clockCyclesToMicroseconds(xthal_get_ccount() - pulse_start_cycle_count);
}

unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout)
{
    return pulseIn(pin, state, timeout);
}
