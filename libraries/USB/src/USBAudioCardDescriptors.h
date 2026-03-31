// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

//--------------------------------------------------------------------+
// UAC2 DESCRIPTOR TEMPLATES
//--------------------------------------------------------------------+

// Unit numbers are arbitrary selected
#define UAC2_ENTITY_CLOCK               0x04
// Speaker path
#define UAC2_ENTITY_SPK_INPUT_TERMINAL  0x01
#define UAC2_ENTITY_SPK_FEATURE_UNIT    0x02
#define UAC2_ENTITY_SPK_OUTPUT_TERMINAL 0x03
// Microphone path
#define UAC2_ENTITY_MIC_INPUT_TERMINAL  0x11
#define UAC2_ENTITY_MIC_OUTPUT_TERMINAL 0x13

// Stereo Speaker + Mic(s)
#define TUD_AUDIO20_HEADSET_STEREO_DESC_LEN (TUD_AUDIO20_DESC_IAD_LEN\
    + TUD_AUDIO20_DESC_STD_AC_LEN\
    + TUD_AUDIO20_DESC_CS_AC_LEN\
    + TUD_AUDIO20_DESC_CLK_SRC_LEN\
    + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2)\
    + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_STD_AC_INT_EP_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN\
    /* Interface 2, Alternate 0 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    /* Interface 2, Alternate 1 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO20_HEADSET_STEREO_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epint, _nMaxSampleRate, _nChannels_SPK, _nChannels_MIC, _nBytesPerSample, _nBitsUsedPerSample) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO20_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x03, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO20_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO20_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO20_FUNC_HEADSET, /*_totallen*/ TUD_AUDIO20_DESC_CLK_SRC_LEN+TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2)+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN, /*_ctrl*/ AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO20_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),    \
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Feature Unit Descriptor(4.7.2.8) */\
    TUD_AUDIO20_DESC_FEATURE_UNIT(/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlch0master*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch1*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch2*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS)),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ _nChannels_MIC, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */\
    TUD_AUDIO20_DESC_STD_AC_INT_EP(/*_ep*/ _epint, /*_interval*/ 0x01), \
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+2), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+2), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ _nChannels_MIC, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, _nMaxSampleRate, _nBytesPerSample, _nChannels_MIC), /*_interval*/ 0x01),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)

// Mono Speaker + Mic(s)
#define TUD_AUDIO20_HEADSET_MONO_DESC_LEN (TUD_AUDIO20_DESC_IAD_LEN\
    + TUD_AUDIO20_DESC_STD_AC_LEN\
    + TUD_AUDIO20_DESC_CS_AC_LEN\
    + TUD_AUDIO20_DESC_CLK_SRC_LEN\
    + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(1)\
    + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_STD_AC_INT_EP_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN\
    /* Interface 2, Alternate 0 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    /* Interface 2, Alternate 1 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO20_HEADSET_MONO_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epint, _nMaxSampleRate, _nChannels_SPK, _nChannels_MIC, _nBytesPerSample, _nBitsUsedPerSample) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO20_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x03, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO20_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO20_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO20_FUNC_HEADSET, /*_totallen*/ TUD_AUDIO20_DESC_CLK_SRC_LEN+TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(1)+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN, /*_ctrl*/ AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO20_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),    \
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Feature Unit Descriptor(4.7.2.8) */\
    TUD_AUDIO20_DESC_FEATURE_UNIT(/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlch0master*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch1*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS)),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ _nChannels_MIC, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */\
    TUD_AUDIO20_DESC_STD_AC_INT_EP(/*_ep*/ _epint, /*_interval*/ 0x01), \
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+2), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+2), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ _nChannels_MIC, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, _nMaxSampleRate, _nBytesPerSample, _nChannels_MIC), /*_interval*/ 0x01),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)

// Stereo Speaker
#define TUD_AUDIO20_SPEAKER_STEREO_DESC_LEN (TUD_AUDIO20_DESC_IAD_LEN\
    + TUD_AUDIO20_DESC_STD_AC_LEN\
    + TUD_AUDIO20_DESC_CS_AC_LEN\
    + TUD_AUDIO20_DESC_CLK_SRC_LEN\
    + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2)\
    + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_STD_AC_INT_EP_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO20_SPEAKER_STEREO_DESCRIPTOR(_itfnum, _stridx, _epout, _epint, _nMaxSampleRate, _nChannels_SPK, _nBytesPerSample, _nBitsUsedPerSample) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO20_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO20_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO20_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO20_FUNC_HEADSET, /*_totallen*/ TUD_AUDIO20_DESC_CLK_SRC_LEN+TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2)+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN, /*_ctrl*/ AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO20_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),    \
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Feature Unit Descriptor(4.7.2.8) */\
    TUD_AUDIO20_DESC_FEATURE_UNIT(/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlch0master*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch1*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch2*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS)),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */\
    TUD_AUDIO20_DESC_STD_AC_INT_EP(/*_ep*/ _epint, /*_interval*/ 0x01), \
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)

// Mono Speaker
#define TUD_AUDIO20_SPEAKER_MONO_DESC_LEN (TUD_AUDIO20_DESC_IAD_LEN\
    + TUD_AUDIO20_DESC_STD_AC_LEN\
    + TUD_AUDIO20_DESC_CS_AC_LEN\
    + TUD_AUDIO20_DESC_CLK_SRC_LEN\
    + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(1)\
    + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_STD_AC_INT_EP_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO20_SPEAKER_MONO_DESCRIPTOR(_itfnum, _stridx, _epout, _epint, _nMaxSampleRate, _nChannels_SPK, _nBytesPerSample, _nBitsUsedPerSample) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO20_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO20_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO20_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO20_FUNC_HEADSET, /*_totallen*/ TUD_AUDIO20_DESC_CLK_SRC_LEN+TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(1)+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN, /*_ctrl*/ AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO20_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),    \
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Feature Unit Descriptor(4.7.2.8) */\
    TUD_AUDIO20_DESC_FEATURE_UNIT(/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlch0master*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch1*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS)),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */\
    TUD_AUDIO20_DESC_STD_AC_INT_EP(/*_ep*/ _epint, /*_interval*/ 0x01), \
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ _nChannels_SPK, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)

// Microphone(s)
#define TUD_AUDIO20_MICROPHONE_DESC_LEN (TUD_AUDIO20_DESC_IAD_LEN\
    + TUD_AUDIO20_DESC_STD_AC_LEN\
    + TUD_AUDIO20_DESC_CS_AC_LEN\
    + TUD_AUDIO20_DESC_CLK_SRC_LEN\
    + TUD_AUDIO20_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO20_DESC_STD_AC_INT_EP_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO20_DESC_STD_AS_LEN\
    + TUD_AUDIO20_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO20_MICROPHONE_DESCRIPTOR(_itfnum, _stridx, _epin, _epint, _nMaxSampleRate, _nChannels_MIC, _nBytesPerSample, _nBitsUsedPerSample) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO20_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO20_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO20_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO20_FUNC_HEADSET, /*_totallen*/ TUD_AUDIO20_DESC_CLK_SRC_LEN+TUD_AUDIO20_DESC_INPUT_TERM_LEN+TUD_AUDIO20_DESC_OUTPUT_TERM_LEN, /*_ctrl*/ AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO20_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),    \
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ _nChannels_MIC, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO20_CTRL_R << AUDIO20_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */\
    TUD_AUDIO20_DESC_STD_AC_INT_EP(/*_ep*/ _epint, /*_interval*/ 0x01), \
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)(_itfnum+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ _nChannels_MIC, /*_channelcfg*/ AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, _nMaxSampleRate, _nBytesPerSample, _nChannels_MIC), /*_interval*/ 0x01),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO20_CTRL_NONE, /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)

//--------------------------------------------------------------------+
// UAC1 DESCRIPTOR TEMPLATES
//--------------------------------------------------------------------+

// UAC1 entity IDs for speaker and microphone
// Speaker path
#define UAC1_ENTITY_SPK_INPUT_TERMINAL  0x01
#define UAC1_ENTITY_SPK_FEATURE_UNIT    0x02
#define UAC1_ENTITY_SPK_OUTPUT_TERMINAL 0x03
// Microphone path
#define UAC1_ENTITY_MIC_INPUT_TERMINAL  0x11
#define UAC1_ENTITY_MIC_OUTPUT_TERMINAL 0x13

// Stereo Speaker + Mic(s)
#define TUD_AUDIO10_HEADSET_STEREO_DESC_LEN(_nfreqs) (\
    +TUD_AUDIO10_DESC_STD_AC_LEN\
    + TUD_AUDIO10_DESC_CS_AC_LEN(2)\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    /* Interface 1, Alternate 0 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN\
    /* Interface 2, Alternate 0 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 2, Alternate 1 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO10_HEADSET_STEREO_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _nMaxSampleRate, _nChannels_SPK, _nChannels_MIC, _nBytesPerSample, _nBitsUsedPerSample, ...) \
    /* Standard AC Interface Descriptor(4.3.1) */\
    TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
    TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN+TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN), /*_itf*/ (uint8_t)((_itfnum)+1), (uint8_t)((_itfnum)+2)),\
    /* Speaker Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_nchannels*/ _nChannels_SPK, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Speaker Feature Unit Descriptor(4.3.2.5) */\
    TUD_AUDIO10_DESC_FEATURE_UNIT(/*_unitid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlmaster*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch1*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch2*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME)),\
    /* Speaker Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_stridx*/ 0x00),\
    /* Microphone Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_nchannels*/ _nChannels_MIC, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Microphone Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_srcid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_SPK, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 2, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+2), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 2, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+2), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_MIC, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_MIC), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)

// Mono Speaker + Mic(s)
#define TUD_AUDIO10_HEADSET_MONO_DESC_LEN(_nfreqs) (\
    +TUD_AUDIO10_DESC_STD_AC_LEN\
    + TUD_AUDIO10_DESC_CS_AC_LEN(2)\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(1)\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    /* Interface 1, Alternate 0 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN\
    /* Interface 2, Alternate 0 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 2, Alternate 1 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO10_HEADSET_MONO_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _nMaxSampleRate, _nChannels_SPK, _nChannels_MIC, _nBytesPerSample, _nBitsUsedPerSample, ...) \
    /* Standard AC Interface Descriptor(4.3.1) */\
    TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
    TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(1)+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN+TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN), /*_itf*/ (uint8_t)((_itfnum)+1), (uint8_t)((_itfnum)+2)),\
    /* Speaker Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_nchannels*/ _nChannels_SPK, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Speaker Feature Unit Descriptor(4.3.2.5) */\
    TUD_AUDIO10_DESC_FEATURE_UNIT(/*_unitid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlmaster*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch1*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME)),\
    /* Speaker Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_stridx*/ 0x00),\
    /* Microphone Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_nchannels*/ _nChannels_MIC, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Microphone Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_srcid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_SPK, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 2, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+2), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 2, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+2), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_MIC, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_MIC), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)

// Stereo Speaker
#define TUD_AUDIO10_SPEAKER_STEREO_DESC_LEN(_nfreqs) (\
    +TUD_AUDIO10_DESC_STD_AC_LEN\
    + TUD_AUDIO10_DESC_CS_AC_LEN(1)\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    /* Interface 1, Alternate 0 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO10_SPEAKER_STEREO_DESCRIPTOR(_itfnum, _stridx, _epout, _nMaxSampleRate, _nChannels_SPK, _nBytesPerSample, _nBitsUsedPerSample, ...) \
    /* Standard AC Interface Descriptor(4.3.1) */\
    TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
    TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(2)+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN), /*_itf*/ (uint8_t)((_itfnum)+1)),\
    /* Speaker Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_nchannels*/ _nChannels_SPK, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Speaker Feature Unit Descriptor(4.3.2.5) */\
    TUD_AUDIO10_DESC_FEATURE_UNIT(/*_unitid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlmaster*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch1*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch2*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME)),\
    /* Speaker Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_stridx*/ 0x00),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_SPK, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)

// Mono Speaker
#define TUD_AUDIO10_SPEAKER_MONO_DESC_LEN(_nfreqs) (\
    +TUD_AUDIO10_DESC_STD_AC_LEN\
    + TUD_AUDIO10_DESC_CS_AC_LEN(1)\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(1)\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    /* Interface 1, Alternate 0 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 1, Alternate 1 (speaker) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO10_SPEAKER_MONO_DESCRIPTOR(_itfnum, _stridx, _epout, _nMaxSampleRate, _nChannels_SPK, _nBytesPerSample, _nBitsUsedPerSample, ...) \
    /* Standard AC Interface Descriptor(4.3.1) */\
    TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
    TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_FEATURE_UNIT_LEN(1)+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN), /*_itf*/ (uint8_t)((_itfnum)+1)),\
    /* Speaker Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_nchannels*/ _nChannels_SPK, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Speaker Feature Unit Descriptor(4.3.2.5) */\
    TUD_AUDIO10_DESC_FEATURE_UNIT(/*_unitid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, /*_ctrlmaster*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME), /*_ctrlch1*/ (AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME)),\
    /* Speaker Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_HEADPHONES, /*_assocTerm*/ 0x00, /*_srcid*/ UAC1_ENTITY_SPK_FEATURE_UNIT, /*_stridx*/ 0x00),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Speaker Interface 1, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_SPK_INPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_SPK, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ADAPTIVE), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_SPK), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)

// Microphone(s)
#define TUD_AUDIO10_MICROPHONE_DESC_LEN(_nfreqs) (\
    +TUD_AUDIO10_DESC_STD_AC_LEN\
    + TUD_AUDIO10_DESC_CS_AC_LEN(1)\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    /* Interface 2, Alternate 0 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 2, Alternate 1 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO10_MICROPHONE_DESCRIPTOR(_itfnum, _stridx, _epin, _nMaxSampleRate, _nChannels_MIC, _nBytesPerSample, _nBitsUsedPerSample, ...) \
    /* Standard AC Interface Descriptor(4.3.1) */\
    TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
    TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN), /*_itf*/ (uint8_t)((_itfnum)+1)),\
    /* Microphone Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_nchannels*/ _nChannels_MIC, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Microphone Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_srcid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 1, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 1, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_MIC, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_MIC), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)





