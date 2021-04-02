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
 *  \defgroup ClassDriver_CDC Communication Device Class (CDC)
 *            Currently only Abstract Control Model subclass is supported
 *  @{ */

#ifndef _TUSB_MIDI_H__
#define _TUSB_MIDI_H__

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Specific Descriptor
//--------------------------------------------------------------------+

typedef enum
{
  MIDI_CS_INTERFACE_HEADER    = 0x01,
  MIDI_CS_INTERFACE_IN_JACK   = 0x02,
  MIDI_CS_INTERFACE_OUT_JACK  = 0x03,
  MIDI_CS_INTERFACE_ELEMENT   = 0x04,
} midi_cs_interface_subtype_t;

typedef enum
{
  MIDI_CS_ENDPOINT_GENERAL = 0x01
} midi_cs_endpoint_subtype_t;

typedef enum
{
  MIDI_JACK_EMBEDDED = 0x01,
  MIDI_JACK_EXTERNAL = 0x02
} midi_jack_type_t;

typedef enum
{
  MIDI_CIN_MISC              = 0,
  MIDI_CIN_CABLE_EVENT       = 1,
  MIDI_CIN_SYSCOM_2BYTE      = 2, // 2 byte system common message e.g MTC, SongSelect
  MIDI_CIN_SYSCOM_3BYTE      = 3, // 3 byte system common message e.g SPP
  MIDI_CIN_SYSEX_START       = 4, // SysEx starts or continue
  MIDI_CIN_SYSEX_END_1BYTE   = 5, // SysEx ends with 1 data, or 1 byte system common message
  MIDI_CIN_SYSEX_END_2BYTE   = 6, // SysEx ends with 2 data
  MIDI_CIN_SYSEX_END_3BYTE   = 7, // SysEx ends with 3 data
  MIDI_CIN_NOTE_ON           = 8,
  MIDI_CIN_NOTE_OFF          = 9,
  MIDI_CIN_POLY_KEYPRESS     = 10,
  MIDI_CIN_CONTROL_CHANGE    = 11,
  MIDI_CIN_PROGRAM_CHANGE    = 12,
  MIDI_CIN_CHANNEL_PRESSURE  = 13,
  MIDI_CIN_PITCH_BEND_CHANGE = 14,
  MIDI_CIN_1BYTE_DATA = 15
} midi_code_index_number_t;

// MIDI 1.0 status byte
enum
{
  //------------- System Exclusive -------------//
  MIDI_STATUS_SYSEX_START                    = 0xF0,
  MIDI_STATUS_SYSEX_END                      = 0xF7,

  //------------- System Common -------------//
  MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME = 0xF1,
  MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER   = 0xF2,
  MIDI_STATUS_SYSCOM_SONG_SELECT             = 0xF3,
  // F4, F5 is undefined
  MIDI_STATUS_SYSCOM_TUNE_REQUEST            = 0xF6,

  //------------- System RealTime  -------------//
  MIDI_STATUS_SYSREAL_TIMING_CLOCK           = 0xF8,
  // 0xF9 is undefined
  MIDI_STATUS_SYSREAL_START                  = 0xFA,
  MIDI_STATUS_SYSREAL_CONTINUE               = 0xFB,
  MIDI_STATUS_SYSREAL_STOP                   = 0xFC,
  // 0xFD is undefined
  MIDI_STATUS_SYSREAL_ACTIVE_SENSING         = 0xFE,
  MIDI_STATUS_SYSREAL_SYSTEM_RESET           = 0xFF,
};

/// MIDI Interface Header Descriptor
typedef struct TU_ATTR_PACKED
{
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType
  uint16_t bcdMSC            ; ///< MidiStreaming SubClass release number in Binary-Coded Decimal
  uint16_t wTotalLength      ;
} midi_desc_header_t;

/// MIDI In Jack Descriptor
typedef struct TU_ATTR_PACKED
{
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType
  uint8_t bJackType          ; ///< Embedded or External
  uint8_t bJackID            ; ///< Unique ID for MIDI IN Jack
  uint8_t iJack              ; ///< string descriptor
} midi_desc_in_jack_t;


/// MIDI Out Jack Descriptor with single pin
typedef struct TU_ATTR_PACKED
{
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType
  uint8_t bJackType          ; ///< Embedded or External
  uint8_t bJackID            ; ///< Unique ID for MIDI IN Jack
  uint8_t bNrInputPins;

  uint8_t baSourceID;
  uint8_t baSourcePin;

  uint8_t iJack              ; ///< string descriptor
} midi_desc_out_jack_t ;

/// MIDI Out Jack Descriptor with multiple pins
#define midi_desc_out_jack_n_t(input_num) \
  struct TU_ATTR_PACKED { \
    uint8_t bLength            ; \
    uint8_t bDescriptorType    ; \
    uint8_t bDescriptorSubType ; \
    uint8_t bJackType          ; \
    uint8_t bJackID            ; \
    uint8_t bNrInputPins       ; \
    struct TU_ATTR_PACKED {      \
        uint8_t baSourceID;      \
        uint8_t baSourcePin;     \
    } pins[input_num];           \
   uint8_t iJack              ;  \
  }

/// MIDI Element Descriptor
typedef struct TU_ATTR_PACKED
{
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType
  uint8_t bElementID;

  uint8_t bNrInputPins;
  uint8_t baSourceID;
  uint8_t baSourcePin;

  uint8_t bNrOutputPins;
  uint8_t bInTerminalLink;
  uint8_t bOutTerminalLink;
  uint8_t bElCapsSize;

  uint16_t bmElementCaps;
  uint8_t  iElement;
} midi_desc_element_t;

/// MIDI Element Descriptor with multiple pins
#define midi_desc_element_n_t(input_num) \
  struct TU_ATTR_PACKED {       \
    uint8_t bLength;            \
    uint8_t bDescriptorType;    \
    uint8_t bDescriptorSubType; \
    uint8_t bElementID;         \
    uint8_t bNrInputPins;       \
    struct TU_ATTR_PACKED {     \
        uint8_t baSourceID;     \
        uint8_t baSourcePin;    \
    } pins[input_num];          \
    uint8_t bNrOutputPins;      \
    uint8_t bInTerminalLink;    \
    uint8_t bOutTerminalLink;   \
    uint8_t bElCapsSize;        \
    uint16_t bmElementCaps;     \
    uint8_t  iElement;          \
 }

/** @} */

#ifdef __cplusplus
 }
#endif

#endif

/** @} */
