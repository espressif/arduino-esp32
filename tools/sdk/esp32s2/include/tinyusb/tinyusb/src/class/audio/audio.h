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

/** \ingroup group_class
 *  \defgroup ClassDriver_Audio Audio
 *            Currently only MIDI subclass is supported
 *  @{ */

#ifndef _TUSB_AUDIO_H__
#define _TUSB_AUDIO_H__

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

/// Audio Interface Subclass Codes
typedef enum
{
  AUDIO_SUBCLASS_CONTROL = 0x01  , ///< Audio Control
  AUDIO_SUBCLASS_STREAMING       , ///< Audio Streaming
  AUDIO_SUBCLASS_MIDI_STREAMING  , ///< MIDI Streaming
} audio_subclass_type_t;

/// Audio Protocol Codes
typedef enum
{
  AUDIO_PROTOCOL_V1                   = 0x00, ///< Version 1.0
  AUDIO_PROTOCOL_V2                   = 0x20, ///< Version 2.0
  AUDIO_PROTOCOL_V3                   = 0x30, ///< Version 3.0
} audio_protocol_type_t;

/// Audio Function Category Codes
typedef enum
{
  AUDIO_FUNC_DESKTOP_SPEAKER    = 0x01,
  AUDIO_FUNC_HOME_THEATER       = 0x02,
  AUDIO_FUNC_MICROPHONE         = 0x03,
  AUDIO_FUNC_HEADSET            = 0x04,
  AUDIO_FUNC_TELEPHONE          = 0x05,
  AUDIO_FUNC_CONVERTER          = 0x06,
  AUDIO_FUNC_SOUND_RECODER      = 0x07,
  AUDIO_FUNC_IO_BOX             = 0x08,
  AUDIO_FUNC_MUSICAL_INSTRUMENT = 0x09,
  AUDIO_FUNC_PRO_AUDIO          = 0x0A,
  AUDIO_FUNC_AUDIO_VIDEO        = 0x0B,
  AUDIO_FUNC_CONTROL_PANEL      = 0x0C
} audio_function_t;

/// Audio Class-Specific AC Interface Descriptor Subtypes
typedef enum
{
  AUDIO_CS_INTERFACE_HEADER                = 0x01,
  AUDIO_CS_INTERFACE_INPUT_TERMINAL        = 0x02,
  AUDIO_CS_INTERFACE_OUTPUT_TERMINAL       = 0x03,
  AUDIO_CS_INTERFACE_MIXER_UNIT            = 0x04,
  AUDIO_CS_INTERFACE_SELECTOR_UNIT         = 0x05,
  AUDIO_CS_INTERFACE_FEATURE_UNIT          = 0x06,
  AUDIO_CS_INTERFACE_EFFECT_UNIT           = 0x07,
  AUDIO_CS_INTERFACE_PROCESSING_UNIT       = 0x08,
  AUDIO_CS_INTERFACE_EXTENSION_UNIT        = 0x09,
  AUDIO_CS_INTERFACE_CLOCK_SOURCE          = 0x0A,
  AUDIO_CS_INTERFACE_CLOCK_SELECTOR        = 0x0B,
  AUDIO_CS_INTERFACE_CLOCK_MULTIPLIER      = 0x0C,
  AUDIO_CS_INTERFACE_SAMPLE_RATE_CONVERTER = 0x0D,
} audio_cs_interface_subtype_t;

/** @} */

#ifdef __cplusplus
 }
#endif

#endif

/** @} */
