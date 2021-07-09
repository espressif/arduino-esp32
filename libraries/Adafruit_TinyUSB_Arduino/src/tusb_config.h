/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach for Adafruit
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
 */

#ifndef _TUSB_CONFIG_ARDUINO_H_
#define _TUSB_CONFIG_ARDUINO_H_

#ifdef __cplusplus
 extern "C" {
#endif

#if defined(ARDUINO_ARCH_SAMD)
  #include "arduino/ports/samd/tusb_config_samd.h"

#elif defined(ARDUINO_NRF52_ADAFRUIT)
  #include "arduino/ports/nrf/tusb_config_nrf.h"

#elif defined(ARDUINO_ARCH_RP2040)
  #include "arduino/ports/rp2040/tusb_config_rp2040.h"

#elif defined(ARDUINO_ARCH_ESP32)
  // Use the BSP sdk/include/arduino_tinyusb/include/tusb_config.h
  #include <tusb_config.h>

#else
  #error TinyUSB Arduino Library does not support your core yet
#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_ARDUINO_H_ */
