/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
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

#ifndef TUSB_VIDEO_H_
#define TUSB_VIDEO_H_

#include "common/tusb_common.h"

// Table 3-19 Color Matching Descriptor
typedef enum {
  VIDEO_COLOR_PRIMARIES_UNDEFINED = 0x00,
  VIDEO_COLOR_PRIMARIES_BT709, // sRGB (default)
  VIDEO_COLOR_PRIMARIES_BT470_2M,
  VIDEO_COLOR_PRIMARIES_BT470_2BG,
  VIDEO_COLOR_PRIMARIES_SMPTE170M,
  VIDEO_COLOR_PRIMARIES_SMPTE240M,
} video_color_primaries_t;

// Table 3-19 Color Matching Descriptor
typedef enum {
  VIDEO_COLOR_XFER_CH_UNDEFINED = 0x00,
  VIDEO_COLOR_XFER_CH_BT709, // default
  VIDEO_COLOR_XFER_CH_BT470_2M,
  VIDEO_COLOR_XFER_CH_BT470_2BG,
  VIDEO_COLOR_XFER_CH_SMPTE170M,
  VIDEO_COLOR_XFER_CH_SMPTE240M,
  VIDEO_COLOR_XFER_CH_LINEAR,
  VIDEO_COLOR_XFER_CH_SRGB,
} video_color_transfer_characteristics_t;

// Table 3-19 Color Matching Descriptor
typedef enum {
  VIDEO_COLOR_COEF_UNDEFINED = 0x00,
  VIDEO_COLOR_COEF_BT709,
  VIDEO_COLOR_COEF_FCC,
  VIDEO_COLOR_COEF_BT470_2BG,
  VIDEO_COLOR_COEF_SMPTE170M, // BT.601 default
  VIDEO_COLOR_COEF_SMPTE240M,
} video_color_matrix_coefficients_t;

/* 4.2.1.2 Request Error Code Control */
typedef enum {
  VIDEO_ERROR_NONE = 0, /* The request succeeded. */
  VIDEO_ERROR_NOT_READY,
  VIDEO_ERROR_WRONG_STATE,
  VIDEO_ERROR_POWER,
  VIDEO_ERROR_OUT_OF_RANGE,
  VIDEO_ERROR_INVALID_UNIT,
  VIDEO_ERROR_INVALID_CONTROL,
  VIDEO_ERROR_INVALID_REQUEST,
  VIDEO_ERROR_INVALID_VALUE_WITHIN_RANGE,
  VIDEO_ERROR_UNKNOWN = 0xFF,
} video_error_code_t;

/* A.2 Interface Subclass */
typedef enum {
  VIDEO_SUBCLASS_UNDEFINED = 0x00,
  VIDEO_SUBCLASS_CONTROL,
  VIDEO_SUBCLASS_STREAMING,
  VIDEO_SUBCLASS_INTERFACE_COLLECTION,
} video_subclass_type_t;

/* A.3 Interface Protocol */
typedef enum {
  VIDEO_ITF_PROTOCOL_UNDEFINED = 0x00,
  VIDEO_ITF_PROTOCOL_15,
} video_interface_protocol_code_t;

/* A.5 Class-Specific VideoControl Interface Descriptor Subtypes */
typedef enum {
  VIDEO_CS_ITF_VC_UNDEFINED = 0x00,
  VIDEO_CS_ITF_VC_HEADER,
  VIDEO_CS_ITF_VC_INPUT_TERMINAL,
  VIDEO_CS_ITF_VC_OUTPUT_TERMINAL,
  VIDEO_CS_ITF_VC_SELECTOR_UNIT,
  VIDEO_CS_ITF_VC_PROCESSING_UNIT,
  VIDEO_CS_ITF_VC_EXTENSION_UNIT,
  VIDEO_CS_ITF_VC_ENCODING_UNIT,
  VIDEO_CS_ITF_VC_MAX,
} video_cs_vc_interface_subtype_t;

/* A.6 Class-Specific VideoStreaming Interface Descriptor Subtypes */
typedef enum {
  VIDEO_CS_ITF_VS_UNDEFINED             = 0x00,
  VIDEO_CS_ITF_VS_INPUT_HEADER          = 0x01,
  VIDEO_CS_ITF_VS_OUTPUT_HEADER         = 0x02,
  VIDEO_CS_ITF_VS_STILL_IMAGE_FRAME     = 0x03,
  VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED   = 0x04,
  VIDEO_CS_ITF_VS_FRAME_UNCOMPRESSED    = 0x05,
  VIDEO_CS_ITF_VS_FORMAT_MJPEG          = 0x06,
  VIDEO_CS_ITF_VS_FRAME_MJPEG           = 0x07,
  VIDEO_CS_ITF_VS_FORMAT_MPEG2TS        = 0x0A,
  VIDEO_CS_ITF_VS_FORMAT_DV             = 0x0C,
  VIDEO_CS_ITF_VS_COLORFORMAT           = 0x0D,
  VIDEO_CS_ITF_VS_FORMAT_FRAME_BASED    = 0x10,
  VIDEO_CS_ITF_VS_FRAME_FRAME_BASED     = 0x11,
  VIDEO_CS_ITF_VS_FORMAT_STREAM_BASED   = 0x12,
  VIDEO_CS_ITF_VS_FORMAT_H264           = 0x13,
  VIDEO_CS_ITF_VS_FRAME_H264            = 0x14,
  VIDEO_CS_ITF_VS_FORMAT_H264_SIMULCAST = 0x15,
  VIDEO_CS_ITF_VS_FORMAT_VP8            = 0x16,
  VIDEO_CS_ITF_VS_FRAME_VP8             = 0x17,
  VIDEO_CS_ITF_VS_FORMAT_VP8_SIMULCAST  = 0x18,
} video_cs_vs_interface_subtype_t;

/* A.7. Class-Specific Endpoint Descriptor Subtypes */
typedef enum {
  VIDEO_CS_EP_UNDEFINED = 0x00,
  VIDEO_CS_EP_GENERAL,
  VIDEO_CS_EP_ENDPOINT,
  VIDEO_CS_EP_INTERRUPT
} video_cs_ep_subtype_t;

/* A.8 Class-Specific Request Codes */
typedef enum {
  VIDEO_REQUEST_UNDEFINED   = 0x00,
  VIDEO_REQUEST_SET_CUR     = 0x01,
  VIDEO_REQUEST_SET_CUR_ALL = 0x11,
  VIDEO_REQUEST_GET_CUR     = 0x81,
  VIDEO_REQUEST_GET_MIN     = 0x82,
  VIDEO_REQUEST_GET_MAX     = 0x83,
  VIDEO_REQUEST_GET_RES     = 0x84,
  VIDEO_REQUEST_GET_LEN     = 0x85,
  VIDEO_REQUEST_GET_INFO    = 0x86,
  VIDEO_REQUEST_GET_DEF     = 0x87,
  VIDEO_REQUEST_GET_CUR_ALL = 0x91,
  VIDEO_REQUEST_GET_MIN_ALL = 0x92,
  VIDEO_REQUEST_GET_MAX_ALL = 0x93,
  VIDEO_REQUEST_GET_RES_ALL = 0x94,
  VIDEO_REQUEST_GET_DEF_ALL = 0x97
} video_control_request_t;

/* A.9.1 VideoControl Interface Control Selectors */
typedef enum {
  VIDEO_VC_CTL_UNDEFINED = 0x00,
  VIDEO_VC_CTL_VIDEO_POWER_MODE,
  VIDEO_VC_CTL_REQUEST_ERROR_CODE,
} video_interface_control_selector_t;

/* A.9.8 VideoStreaming Interface Control Selectors */
typedef enum {
  VIDEO_VS_CTL_UNDEFINED = 0x00,
  VIDEO_VS_CTL_PROBE,
  VIDEO_VS_CTL_COMMIT,
  VIDEO_VS_CTL_STILL_PROBE,
  VIDEO_VS_CTL_STILL_COMMIT,
  VIDEO_VS_CTL_STILL_IMAGE_TRIGGER,
  VIDEO_VS_CTL_STREAM_ERROR_CODE,
  VIDEO_VS_CTL_GENERATE_KEY_FRAME,
  VIDEO_VS_CTL_UPDATE_FRAME_SEGMENT,
  VIDEO_VS_CTL_SYNCH_DELAY_CONTROL,
} video_interface_streaming_selector_t;

/* B. Terminal Types */
typedef enum {
  // Terminal
  VIDEO_TT_VENDOR_SPECIFIC         = 0x0100,
  VIDEO_TT_STREAMING               = 0x0101,

  // Input
  VIDEO_ITT_VENDOR_SPECIFIC        = 0x0200,
  VIDEO_ITT_CAMERA                 = 0x0201,
  VIDEO_ITT_MEDIA_TRANSPORT_INPUT  = 0x0202,

  // Output
  VIDEO_OTT_VENDOR_SPECIFIC        = 0x0300,
  VIDEO_OTT_DISPLAY                = 0x0301,
  VIDEO_OTT_MEDIA_TRANSPORT_OUTPUT = 0x0302,

  // External
  VIDEO_ETT_VENDOR_SPEIFIC         = 0x0400,
  VIDEO_ETT_COMPOSITE_CONNECTOR    = 0x0401,
  VIDEO_ETT_SVIDEO_CONNECTOR       = 0x0402,
  VIDEO_ETT_COMPONENT_CONNECTOR    = 0x0403,
} video_terminal_type_t;

//--------------------------------------------------------------------+
// Descriptors
//--------------------------------------------------------------------+

/* 2.3.4.2 */
typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint16_t bcdUVC;
  uint16_t wTotalLength;
  uint32_t dwClockFrequency;
  uint8_t  bInCollection;
  uint8_t  baInterfaceNr[];
} tusb_desc_cs_video_ctl_itf_hdr_t;

/* 2.4.3.3 */
typedef struct TU_ATTR_PACKED {
  uint8_t bHeaderLength;
  union {
    uint8_t bmHeaderInfo;
    struct {
      uint8_t FrameID:              1;
      uint8_t EndOfFrame:           1;
      uint8_t PresentationTime:     1;
      uint8_t SourceClockReference: 1;
      uint8_t PayloadSpecific:      1;
      uint8_t StillImage:           1;
      uint8_t Error:                1;
      uint8_t EndOfHeader:          1;
    };
  };
} tusb_video_payload_header_t;

/* 3.9.2.1 */
typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bNumFormats;
  uint16_t wTotalLength;
  uint8_t  bEndpointAddress;
  uint8_t  bmInfo;
  uint8_t  bTerminalLink;
  uint8_t  bStillCaptureMethod;
  uint8_t  bTriggerSupport;
  uint8_t  bTriggerUsage;
  uint8_t  bControlSize;
  uint8_t  bmaControls[];
} tusb_desc_cs_video_stm_itf_in_hdr_t;

/* 3.9.2.2 */
typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bNumFormats;
  uint16_t wTotalLength;
  uint8_t  bEndpointAddress;
  uint8_t  bTerminalLink;
  uint8_t  bControlSize;
  uint8_t  bmaControls[];
} tusb_desc_cs_video_stm_itf_out_hdr_t;

typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bNumFormats;
  uint16_t wTotalLength;
  uint8_t  bEndpointAddress;
  union {
    struct {
      uint8_t  bmInfo;
      uint8_t  bTerminalLink;
      uint8_t  bStillCaptureMethod;
      uint8_t  bTriggerSupport;
      uint8_t  bTriggerUsage;
      uint8_t  bControlSize;
      uint8_t  bmaControls[];
    } input;
    struct {
      uint8_t  bEndpointAddress;
      uint8_t  bTerminalLink;
      uint8_t  bControlSize;
      uint8_t  bmaControls[];
    } output;
  };
} tusb_desc_cs_video_stm_itf_hdr_t;

typedef struct TU_ATTR_PACKED {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t bFormatIndex;
  uint8_t bNumFrameDescriptors;
  uint8_t guidFormat[16];
  uint8_t bBitsPerPixel;
  uint8_t bDefaultFrameIndex;
  uint8_t bAspectRatioX;
  uint8_t bAspectRatioY;
  uint8_t bmInterlaceFlags;
  uint8_t bCopyProtect;
} tusb_desc_cs_video_fmt_uncompressed_t;

typedef struct TU_ATTR_PACKED {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t bFormatIndex;
  uint8_t bNumFrameDescriptors;
  uint8_t bmFlags;
  uint8_t bDefaultFrameIndex;
  uint8_t bAspectRatioX;
  uint8_t bAspectRatioY;
  uint8_t bmInterlaceFlags;
  uint8_t bCopyProtect;
} tusb_desc_cs_video_fmt_mjpeg_t;

typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bFormatIndex;
  uint32_t dwMaxVideoFrameBufferSize; /* deprecated */
  uint8_t  bFormatType;
} tusb_desc_cs_video_fmt_dv_t;

typedef struct TU_ATTR_PACKED {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t bFormatIndex;
  uint8_t bNumFrameDescriptors;
  uint8_t guidFormat[16];
  uint8_t bBitsPerPixel;
  uint8_t bDefaultFrameIndex;
  uint8_t bAspectRatioX;
  uint8_t bAspectRatioY;
  uint8_t bmInterlaceFlags;
  uint8_t bCopyProtect;
  uint8_t bVaribaleSize;
} tusb_desc_cs_video_fmt_frame_based_t;

typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bFrameIndex;
  uint8_t  bmCapabilities;
  uint16_t wWidth;
  uint16_t wHeight;
  uint32_t dwMinBitRate;
  uint32_t dwMaxBitRate;
  uint32_t dwMaxVideoFrameBufferSize; /* deprecated */
  uint32_t dwDefaultFrameInterval;
  uint8_t  bFrameIntervalType;
  uint32_t dwFrameInterval[];
} tusb_desc_cs_video_frm_uncompressed_t;

typedef tusb_desc_cs_video_frm_uncompressed_t tusb_desc_cs_video_frm_mjpeg_t;

typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bFrameIndex;
  uint8_t  bmCapabilities;
  uint16_t wWidth;
  uint16_t wHeight;
  uint32_t dwMinBitRate;
  uint32_t dwMaxBitRate;
  uint32_t dwDefaultFrameInterval;
  uint8_t  bFrameIntervalType;
  uint32_t dwBytesPerLine;
  uint32_t dwFrameInterval[];
} tusb_desc_cs_video_frm_frame_based_t;

//--------------------------------------------------------------------+
// Requests
//--------------------------------------------------------------------+

/* 4.3.1.1 */
typedef struct TU_ATTR_PACKED {
  union {
    uint8_t bmHint;
    struct TU_ATTR_PACKED {
      uint16_t dwFrameInterval: 1;
      uint16_t wKeyFrameRatel : 1;
      uint16_t wPFrameRate    : 1;
      uint16_t wCompQuality   : 1;
      uint16_t wCompWindowSize: 1;
      uint16_t                : 0;
    } Hint;
  };
  uint8_t  bFormatIndex;
  uint8_t  bFrameIndex;
  uint32_t dwFrameInterval;
  uint16_t wKeyFrameRate;
  uint16_t wPFrameRate;
  uint16_t wCompQuality;
  uint16_t wCompWindowSize;
  uint16_t wDelay;
  uint32_t dwMaxVideoFrameSize;
  uint32_t dwMaxPayloadTransferSize;
  uint32_t dwClockFrequency;
  union {
    uint8_t bmFramingInfo;
    struct TU_ATTR_PACKED {
      uint8_t FrameID   : 1;
      uint8_t EndOfFrame: 1;
      uint8_t EndOfSlice: 1;
      uint8_t           : 0;
    } FramingInfo;
  };
  uint8_t  bPreferedVersion;
  uint8_t  bMinVersion;
  uint8_t  bMaxVersion;
  uint8_t  bUsage;
  uint8_t  bBitDepthLuma;
  uint8_t  bmSettings;
  uint8_t  bMaxNumberOfRefFramesPlus1;
  uint16_t bmRateControlModes;
  uint64_t bmLayoutPerStream;
} video_probe_and_commit_control_t;

TU_VERIFY_STATIC( sizeof(video_probe_and_commit_control_t) == 48, "size is not correct");

#define TUD_VIDEO_DESC_IAD_LEN                    8
#define TUD_VIDEO_DESC_STD_VC_LEN                 9
#define TUD_VIDEO_DESC_CS_VC_LEN                  12
#define TUD_VIDEO_DESC_INPUT_TERM_LEN             8
#define TUD_VIDEO_DESC_OUTPUT_TERM_LEN            9
#define TUD_VIDEO_DESC_CAMERA_TERM_LEN            18
#define TUD_VIDEO_DESC_STD_VS_LEN                 9
#define TUD_VIDEO_DESC_CS_VS_IN_LEN               13
#define TUD_VIDEO_DESC_CS_VS_OUT_LEN              9
#define TUD_VIDEO_DESC_CS_VS_FMT_UNCOMPR_LEN      27
#define TUD_VIDEO_DESC_CS_VS_FMT_MJPEG_LEN        11
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_CONT_LEN 38
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_DISC_LEN 26
#define TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_LEN   38
#define TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_DISC_LEN   26
#define TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING_LEN   6

/* 2.2 compression formats */
#define TUD_VIDEO_GUID_YUY2   0x59,0x55,0x59,0x32,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71
#define TUD_VIDEO_GUID_NV12   0x4E,0x56,0x31,0x32,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71
#define TUD_VIDEO_GUID_M420   0x4D,0x34,0x32,0x30,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71
#define TUD_VIDEO_GUID_I420   0x49,0x34,0x32,0x30,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71

#define TUD_VIDEO_DESC_IAD(_firstitfs, _nitfs, _stridx) \
  TUD_VIDEO_DESC_IAD_LEN, TUSB_DESC_INTERFACE_ASSOCIATION, \
  _firstitfs, _nitfs, TUSB_CLASS_VIDEO, VIDEO_SUBCLASS_INTERFACE_COLLECTION, \
  VIDEO_ITF_PROTOCOL_UNDEFINED, _stridx

#define TUD_VIDEO_DESC_STD_VC(_itfnum, _nEPs, _stridx) \
  TUD_VIDEO_DESC_STD_VC_LEN, TUSB_DESC_INTERFACE, _itfnum, /* fixed to zero */ 0x00, \
  _nEPs, TUSB_CLASS_VIDEO, VIDEO_SUBCLASS_CONTROL, VIDEO_ITF_PROTOCOL_15, _stridx

/* 3.7.2 */
#define TUD_VIDEO_DESC_CS_VC(_bcdUVC, _totallen, _clkfreq, ...)	\
  TUD_VIDEO_DESC_CS_VC_LEN + (TU_ARGS_NUM(__VA_ARGS__)), TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VC_HEADER, \
  U16_TO_U8S_LE(_bcdUVC), U16_TO_U8S_LE((_totallen) + TUD_VIDEO_DESC_CS_VC_LEN + (TU_ARGS_NUM(__VA_ARGS__))), \
  U32_TO_U8S_LE(_clkfreq), TU_ARGS_NUM(__VA_ARGS__), __VA_ARGS__

/* 3.7.2.1 */
#define TUD_VIDEO_DESC_INPUT_TERM(_tid, _tt, _at, _stridx) \
  TUD_VIDEO_DESC_INPUT_TERM_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VC_INPUT_TERMINAL, \
    _tid, U16_TO_U8S_LE(_tt), _at, _stridx

/* 3.7.2.2 */
#define TUD_VIDEO_DESC_OUTPUT_TERM(_tid, _tt, _at, _srcid, _stridx) \
  TUD_VIDEO_DESC_OUTPUT_TERM_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VC_OUTPUT_TERMINAL, \
    _tid, U16_TO_U8S_LE(_tt), _at, _srcid, _stridx

/* 3.7.2.3 */
#define TUD_VIDEO_DESC_CAMERA_TERM(_tid, _at, _stridx, _focal_min, _focal_max, _focal, _ctls) \
  TUD_VIDEO_DESC_CAMERA_TERM_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VC_INPUT_TERMINAL, \
    _tid, U16_TO_U8S_LE(VIDEO_ITT_CAMERA), _at, _stridx, \
    U16_TO_U8S_LE(_focal_min), U16_TO_U8S_LE(_focal_max), U16_TO_U8S_LE(_focal), 3, \
    TU_U32_BYTE0(_ctls), TU_U32_BYTE1(_ctls), TU_U32_BYTE2(_ctls)

/* 3.9.1 */
#define TUD_VIDEO_DESC_STD_VS(_itfnum, _alt, _epn, _stridx) \
  TUD_VIDEO_DESC_STD_VS_LEN, TUSB_DESC_INTERFACE, _itfnum, _alt, \
  _epn, TUSB_CLASS_VIDEO, VIDEO_SUBCLASS_STREAMING, VIDEO_ITF_PROTOCOL_15, _stridx

/* 3.9.2.1 */
#define TUD_VIDEO_DESC_CS_VS_INPUT(_numfmt, _totallen, _ep, _inf, _termlnk, _sticaptmeth, _trgspt, _trgusg, ...) \
  TUD_VIDEO_DESC_CS_VS_IN_LEN + (_numfmt) * (TU_ARGS_NUM(__VA_ARGS__)), TUSB_DESC_CS_INTERFACE, \
  VIDEO_CS_ITF_VS_INPUT_HEADER, _numfmt, \
  U16_TO_U8S_LE((_totallen) + TUD_VIDEO_DESC_CS_VS_IN_LEN + (_numfmt) * (TU_ARGS_NUM(__VA_ARGS__))), \
  _ep, _inf, _termlnk, _sticaptmeth, _trgspt, _trgusg, (TU_ARGS_NUM(__VA_ARGS__)), __VA_ARGS__

/* 3.9.2.2 */
#define TUD_VIDEO_DESC_CS_VS_OUTPUT(_numfmt, _totallen, _ep, _inf, _termlnk, ...) \
  TUD_VIDEO_DESC_CS_VS_OUT_LEN + (_numfmt) * (TU_ARGS_NUM(__VA_ARGS__)), TUSB_DESC_CS_INTERFACE, \
  VIDEO_CS_ITF_VS_OUTPUT_HEADER, _numfmt, \
  U16_TO_U8S_LE((_totallen) + TUD_VIDEO_DESC_CS_VS_OUT_LEN + (_numfmt) * (TU_ARGS_NUM(__VA_ARGS__))), \
  _ep, _inf, _termlnk, (TU_ARGS_NUM(__VA_ARGS__)), __VA_ARGS__

/* Uncompressed 3.1.1 */
#define TUD_VIDEO_GUID(_g0,_g1,_g2,_g3,_g4,_g5,_g6,_g7,_g8,_g9,_g10,_g11,_g12,_g13,_g14,_g15) _g0,_g1,_g2,_g3,_g4,_g5,_g6,_g7,_g8,_g9,_g10,_g11,_g12,_g13,_g14,_g15

#define TUD_VIDEO_DESC_CS_VS_FMT_UNCOMPR(_fmtidx, _numfrmdesc, \
  _guid, _bitsperpix, _frmidx, _asrx, _asry, _interlace, _cp) \
  TUD_VIDEO_DESC_CS_VS_FMT_UNCOMPR_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED, \
  _fmtidx, _numfrmdesc, TUD_VIDEO_GUID(_guid), \
  _bitsperpix, _frmidx, _asrx,  _asry, _interlace, _cp

/* Uncompressed 3.1.2 Table 3-3 */
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_CONT(_frmidx, _cap, _width, _height, _minbr, _maxbr, _maxfrmbufsz, _frminterval, _minfrminterval, _maxfrminterval, _frmintervalstep) \
  TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_CONT_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VS_FRAME_UNCOMPRESSED, \
  _frmidx, _cap, U16_TO_U8S_LE(_width), U16_TO_U8S_LE(_height), U32_TO_U8S_LE(_minbr), U32_TO_U8S_LE(_maxbr), \
  U32_TO_U8S_LE(_maxfrmbufsz), U32_TO_U8S_LE(_frminterval), 0, \
  U32_TO_U8S_LE(_minfrminterval), U32_TO_U8S_LE(_maxfrminterval), U32_TO_U8S_LE(_frmintervalstep)

/* Uncompressed 3.1.2 Table 3-4 */
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_DISC(_frmidx, _cap, _width, _height, _minbr, _maxbr, _maxfrmbufsz, _frminterval, ...) \
  TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_DISC_LEN + (TU_ARGS_NUM(__VA_ARGS__)) * 4, \
  TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VS_FRAME_UNCOMPRESSED, \
  _frmidx, _cap, U16_TO_U8S_LE(_width), U16_TO_U8S_LE(_height), U32_TO_U8S_LE(_minbr), U32_TO_U8S_LE(_maxbr), \
  U32_TO_U8S_LE(_maxfrmbufsz), U32_TO_U8S_LE(_frminterval), (TU_ARGS_NUM(__VA_ARGS__)), __VA_ARGS__

/* Motion-JPEG 3.1.1 Table 3-1 */
#define TUD_VIDEO_DESC_CS_VS_FMT_MJPEG(_fmtidx, _numfrmdesc, _fixed_sz, _frmidx, _asrx, _asry, _interlace, _cp) \
  TUD_VIDEO_DESC_CS_VS_FMT_MJPEG_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VS_FORMAT_MJPEG, \
  _fmtidx, _numfrmdesc, _fixed_sz, _frmidx, _asrx,  _asry, _interlace, _cp

/* Motion-JPEG 3.1.1 Table 3-2 and 3-3 */
#define TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT(_frmidx, _cap, _width, _height, _minbr, _maxbr, _maxfrmbufsz, _frminterval, _minfrminterval, _maxfrminterval, _frmintervalstep) \
  TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VS_FRAME_MJPEG, \
  _frmidx, _cap, U16_TO_U8S_LE(_width), U16_TO_U8S_LE(_height), U32_TO_U8S_LE(_minbr), U32_TO_U8S_LE(_maxbr), \
  U32_TO_U8S_LE(_maxfrmbufsz), U32_TO_U8S_LE(_frminterval), 0, \
  U32_TO_U8S_LE(_minfrminterval), U32_TO_U8S_LE(_maxfrminterval), U32_TO_U8S_LE(_frmintervalstep)

/* Motion-JPEG 3.1.1 Table 3-2 and 3-4 */
#define TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_DISC(_frmidx, _cap, _width, _height, _minbr, _maxbr, _maxfrmbufsz, _frminterval, ...) \
  TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_DISC_LEN + (TU_ARGS_NUM(__VA_ARGS__)) * 4, \
  TUSB_DESC_CS_INTERFACE, VIDEO_CS_VS_INTERFACE_FRAME_MJPEG, \
  _frmidx, _cap, U16_TO_U8S_LE(_width), U16_TO_U8S_LE(_height), U32_TO_U8S_LE(_minbr), U32_TO_U8S_LE(_maxbr), \
  U32_TO_U8S_LE(_maxfrmbufsz), U32_TO_U8S_LE(_frminterval), (TU_ARGS_NUM(__VA_ARGS__)), __VA_ARGS__

/* 3.9.2.6 */
#define TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING(_color, _trns, _mat) \
  TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING_LEN, \
  TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VS_COLORFORMAT, \
  _color, _trns, _mat

/* 3.10.1.1 */
#define TUD_VIDEO_DESC_EP_ISO(_ep, _epsize, _ep_interval) \
  7, TUSB_DESC_ENDPOINT, _ep, TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS,\
  U16_TO_U8S_LE(_epsize), _ep_interval

/* 3.10.1.2 */
#define TUD_VIDEO_DESC_EP_BULK(_ep, _epsize, _ep_interval) \
  7, TUSB_DESC_ENDPOINT, _ep, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), _ep_interval

#endif
