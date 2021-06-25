/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach for Adafruit Industries
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

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED

#include "Adafruit_TinyUSB_API.h"

#include "Adafruit_USBD_CDC.h"
#include "Adafruit_USBD_Device.h"

// USB Information can be defined in variant file e.g pins_arduino.h
#include "Arduino.h"

#ifndef USB_VID
#define USB_VID 0xcafe
#endif

#ifndef USB_PID
#define USB_PID 0xcafe
#endif

#ifndef USB_MANUFACTURER
#define USB_MANUFACTURER "Unknown"
#endif

#ifndef USB_PRODUCT
#define USB_PRODUCT "Unknown"
#endif

#ifndef USB_LANGUAGE
#define USB_LANGUAGE 0x0409 // default is English
#endif

#ifndef USB_CONFIG_POWER
#define USB_CONFIG_POWER 100
#endif

enum { STRID_LANGUAGE = 0, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL };

Adafruit_USBD_Device USBDevice;

Adafruit_USBD_Device::Adafruit_USBD_Device(void) {}

void Adafruit_USBD_Device::clearConfiguration(void) {
  tusb_desc_device_t const desc_dev = {.bLength = sizeof(tusb_desc_device_t),
                                       .bDescriptorType = TUSB_DESC_DEVICE,
                                       .bcdUSB = 0x0200,
                                       .bDeviceClass = 0,
                                       .bDeviceSubClass = 0,
                                       .bDeviceProtocol = 0,
                                       .bMaxPacketSize0 =
                                           CFG_TUD_ENDPOINT0_SIZE,
                                       .idVendor = USB_VID,
                                       .idProduct = USB_PID,
                                       .bcdDevice = 0x0100,
                                       .iManufacturer = STRID_MANUFACTURER,
                                       .iProduct = STRID_PRODUCT,
                                       .iSerialNumber = STRID_SERIAL,
                                       .bNumConfigurations = 0x01};

  _desc_device = desc_dev;

  // Config number, interface count, string index, total length,
  // attribute (bit 7 set to 1), power in mA.
  // Note: Total Length Interface Number will be updated later
  uint8_t const dev_cfg[sizeof(tusb_desc_configuration_t)] = {
      TUD_CONFIG_DESCRIPTOR(1, 0, 0, sizeof(tusb_desc_configuration_t),
                            TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP | TU_BIT(7),
                            100),
  };

  memcpy(_desc_cfg_buffer, dev_cfg, sizeof(tusb_desc_configuration_t));
  _desc_cfg = _desc_cfg_buffer;
  _desc_cfg_maxlen = sizeof(_desc_cfg_buffer);
  _desc_cfg_len = sizeof(tusb_desc_configuration_t);

  _itf_count = 0;
  _epin_count = _epout_count = 1;

  memset(_desc_str_arr, 0, sizeof(_desc_str_arr));
  _desc_str_arr[STRID_LANGUAGE] = (const char *)((uint32_t)USB_LANGUAGE);
  _desc_str_arr[STRID_MANUFACTURER] = USB_MANUFACTURER;
  _desc_str_arr[STRID_PRODUCT] = USB_PRODUCT;
  // STRID_SERIAL is platform dependent

  _desc_str_count = 4;
}

// Add interface descriptor
// - Interface number will be updated to match current count
// - Endpoint number is updated to be unique
bool Adafruit_USBD_Device::addInterface(Adafruit_USBD_Interface &itf) {
  uint8_t *desc = _desc_cfg + _desc_cfg_len;
  uint16_t const len = itf.getInterfaceDescriptor(
      _itf_count, desc, _desc_cfg_maxlen - _desc_cfg_len);
  uint8_t *desc_end = desc + len;

  const char *desc_str = itf.getStringDescriptor();

  if (!len) {
    return false;
  }

  // Parse interface descriptor to update
  // - Interface Number & string descriptor
  // - Endpoint address
  while (desc < desc_end) {
    if (desc[1] == TUSB_DESC_INTERFACE) {
      tusb_desc_interface_t *desc_itf = (tusb_desc_interface_t *)desc;
      if (desc_itf->bAlternateSetting == 0) {
        _itf_count++;
        if (desc_str && (_desc_str_count < STRING_DESCRIPTOR_MAX)) {
          _desc_str_arr[_desc_str_count] = desc_str;
          desc_itf->iInterface = _desc_str_count;
          _desc_str_count++;

          // only assign string index to first interface
          desc_str = NULL;
        }
      }
    } else if (desc[1] == TUSB_DESC_ENDPOINT) {
      tusb_desc_endpoint_t *desc_ep = (tusb_desc_endpoint_t *)desc;
      desc_ep->bEndpointAddress |=
          (desc_ep->bEndpointAddress & 0x80) ? _epin_count++ : _epout_count++;
    }

    if (desc[0] == 0) {
      return false;
    }

    desc += desc[0]; // next
  }

  _desc_cfg_len += len;

  // Update configuration descriptor
  tusb_desc_configuration_t *config = (tusb_desc_configuration_t *)_desc_cfg;
  config->wTotalLength = _desc_cfg_len;
  config->bNumInterfaces = _itf_count;

  return true;
}

void Adafruit_USBD_Device::setConfigurationBuffer(uint8_t *buf,
                                                  uint32_t buflen) {
  if (buflen < _desc_cfg_maxlen) {
    return;
  }

  memcpy(buf, _desc_cfg, _desc_cfg_len);
  _desc_cfg = buf;
  _desc_cfg_maxlen = buflen;
}

void Adafruit_USBD_Device::setID(uint16_t vid, uint16_t pid) {
  _desc_device.idVendor = vid;
  _desc_device.idProduct = pid;
}

void Adafruit_USBD_Device::setVersion(uint16_t bcd) {
  _desc_device.bcdUSB = bcd;
}

void Adafruit_USBD_Device::setDeviceVersion(uint16_t bcd) {
  _desc_device.bcdDevice = bcd;
}

void Adafruit_USBD_Device::setLanguageDescriptor(uint16_t language_id) {
  _desc_str_arr[STRID_LANGUAGE] = (const char *)((uint32_t)language_id);
}

void Adafruit_USBD_Device::setManufacturerDescriptor(const char *s) {
  _desc_str_arr[STRID_MANUFACTURER] = s;
}

void Adafruit_USBD_Device::setProductDescriptor(const char *s) {
  _desc_str_arr[STRID_PRODUCT] = s;
}

uint8_t Adafruit_USBD_Device::getSerialDescriptor(uint16_t *serial_utf16) {
  uint8_t serial_id[16] __attribute__((aligned(4)));
  uint8_t const serial_len = TinyUSB_Port_GetSerialNumber(serial_id);

  for (uint8_t i = 0; i < serial_len; i++) {
    for (uint8_t j = 0; j < 2; j++) {
      const char nibble_to_hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                      '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

      uint8_t nibble = (serial_id[i] >> (j * 4)) & 0xf;
      serial_utf16[1 + i * 2 + (1 - j)] = nibble_to_hex[nibble]; // UTF-16-LE
    }
  }

  return 2 * serial_len;
}

bool Adafruit_USBD_Device::begin(uint8_t rhport) {
  clearConfiguration();

  // Serial is always added by default
  // Use Interface Association Descriptor (IAD) for CDC
  // As required by USB Specs IAD's subclass must be common class (2) and
  // protocol must be IAD (1)
  _desc_device.bDeviceClass = TUSB_CLASS_MISC;
  _desc_device.bDeviceSubClass = MISC_SUBCLASS_COMMON;
  _desc_device.bDeviceProtocol = MISC_PROTOCOL_IAD;

  SerialTinyUSB.begin(115200);

  // Init device hardware and call tusb_init()
  TinyUSB_Port_InitDevice(rhport);

  return true;
}

void Adafruit_USBD_Device::task(void) { tud_task(); }

bool Adafruit_USBD_Device::mounted(void) { return tud_mounted(); }

bool Adafruit_USBD_Device::suspended(void) { return tud_suspended(); }

bool Adafruit_USBD_Device::ready(void) { return tud_ready(); }

bool Adafruit_USBD_Device::remoteWakeup(void) { return tud_remote_wakeup(); }

bool Adafruit_USBD_Device::detach(void) { return tud_disconnect(); }

bool Adafruit_USBD_Device::attach(void) { return tud_connect(); }

static int strcpy_utf16(const char *s, uint16_t *buf, int bufsize);
uint16_t const *Adafruit_USBD_Device::descriptor_string_cb(uint8_t index,
                                                           uint16_t langid) {
  (void)langid;

  uint8_t chr_count;

  switch (index) {
  case STRID_LANGUAGE:
    _desc_str[1] = ((uint16_t)((uint32_t)_desc_str_arr[STRID_LANGUAGE]));
    chr_count = 1;
    break;

  case STRID_SERIAL:
    chr_count = getSerialDescriptor(_desc_str);
    break;

  default:
    // Invalid index
    if (index >= _desc_str_count) {
      return NULL;
    }

    chr_count = strcpy_utf16(_desc_str_arr[index], _desc_str + 1, 32);
    break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

  return _desc_str;
}

//--------------------------------------------------------------------+
// TinyUSB stack callbacks
//--------------------------------------------------------------------+
extern "C" {

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&USBDevice._desc_device;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return USBDevice._desc_cfg;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete Note: the 0xEE index string is a Microsoft
// OS 1.0 Descriptors.
// https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  return USBDevice.descriptor_string_cb(index, langid);
}

} // extern C

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
constexpr static inline bool isInvalidUtf8Octet(uint8_t t) {
  // see bullets in https://tools.ietf.org/html/rfc3629#section-1
  return (t == 0xc0) || (t == 0xC1) || (t >= 0xF5);
}

//
// This function has an UNWRITTEN CONTRACT that the buffer is either:
// 1. Pre-validated as legal UTF-8, -OR-
// 2. has a trailing zero-value octet/byte/uint8_t  (aka null-terminated string)
//
// If the above are not true, this decoder may read past the end of the
// allocated buffer, by up to three bytes.
//
// U+1F47F == 👿 ("IMP")
//         == 0001_1111_0100_0111_1111 ==> requires four-byte encoding in UTF-8
//            AABB BBBB CCCC CCDD DDDD ==> 0xF0 0x9F 0x91 0xBF
//
// Example sandwich and safety variables are there to cover the
// two most-common stack layouts for declared variables, in unoptimized
// code, so that the bytes surrounding those allocated for 'evilUTF8'
// are guaranteed to be non-zero and valid UTF8 continuation octets.
//     uint8_t safety1      = 0;
//     uint8_t sandwich1[4] = { 0x81, 0x82, 0x83, 0x84 };
//     uint8_t evilUTF8[5]  = { 0xF0, 0x9F, 0x91, 0xBF, 0xF9 };
//     uint8_t sandwich2[4] = { 0x85, 0x86, 0x87, 0x88 };
//     uint8_t safety2      = 0;
//
// NOTE: evilUTF8 could just contain a single byte 0xF9 ....
//
// Attempting to decode evilUTF8 will progress to whatever is next to it on the
// stack. The above should work when optimizations are turned
//
static int8_t utf8Codepoint(const uint8_t *utf8, uint32_t *codepointp) {
  const uint32_t CODEPOINT_LOWEST_SURROGATE_HALF = 0xD800;
  const uint32_t CODEPOINT_HIGHEST_SURROGATE_HALF = 0xDFFF;

  *codepointp = 0xFFFD; // always initialize output to known value ... 0xFFFD
                        // (REPLACEMENT CHARACTER) seems the natural choice
  uint32_t codepoint;
  int len;

  // The upper bits define both the length of additional bytes for the
  // multi-byte encoding, as well as defining how many bits of the first byte
  // are included in the codepoint. Each additional byte starts with 0b10xxxxxx,
  // encoding six additional bits for the codepoint.
  //
  // For key summary points, see:
  // * https://tools.ietf.org/html/rfc3629#section-3
  //
  if (isInvalidUtf8Octet(utf8[0])) {
    // do not allow illegal octet sequences (e.g., 0xC0 0x80
    // should NOT decode to NULL)
    return -1;
  }

  if (utf8[0] < 0x80) {
    // characters 0000 0000..0000 007F (up to  7 significant bits)
    len = 1;
    codepoint = utf8[0];
  } else if ((utf8[0] & 0xe0) == 0xc0) {
    // characters 0000 0080..0000 07FF
    // (up to 11 significant bits, so first byte encodes five bits)
    len = 2;
    codepoint = utf8[0] & 0x1f;
  } else if ((utf8[0] & 0xf0) == 0xe0) {
    // characters 0000 8000..0000 FFFF
    // (up to 16 significant bits, so first byte encodes four bits)
    len = 3;
    codepoint = utf8[0] & 0x0f;
  } else if ((utf8[0] & 0xf8) == 0xf0) {
    // characters 0001 0000..0010 FFFF
    // (up to 21 significantbits, so first byte encodes three bits)
    len = 4;
    codepoint = utf8[0] & 0x07;
  } else {
    // UTF-8 is defined to only map to Unicode -- 0x00000000..0x0010FFFF
    // 5-byte and 6-byte sequences are not legal per RFC3629
    return -1;
  }

  for (int i = 1; i < len; i++) {
    if ((utf8[i] & 0xc0) != 0x80) {
      // the additional bytes in a valid UTF-8 multi-byte encoding cannot have
      // either of the top two bits set This is more restrictive than
      // isInvalidUtf8Octet()
      return -1;
    }
    codepoint <<= 6; // each continuation byte adds six bits to the codepoint
    codepoint |= utf8[i] & 0x3f; // mask off the top two continuation bits, and
                                 // add the six relevant bits
  }

  // explicit validation to prevent overlong encodings
  if ((len == 1) && (codepoint > 0x00007F)) {
    return -1;
  } else if ((len == 2) && ((codepoint < 0x000080) || (codepoint > 0x0007FF))) {
    return -1;
  } else if ((len == 3) && ((codepoint < 0x000800) || (codepoint > 0x00FFFF))) {
    return -1;
  } else if ((len == 4) && ((codepoint < 0x010000) || (codepoint > 0x10FFFF))) {
    // "You might expect larger code points than U+10FFFF
    // to be expressible, but Unicode is limited in Sections 12
    // of RFC3629 to match the limits of UTF-16." -- Wikipedia UTF-8 note
    // See https://tools.ietf.org/html/rfc3629#section-12
    return -1;
  }

  // high and low surrogate halves (U+D800 through U+DFFF) used by UTF-16 are
  // not legal Unicode values ... see RFC 3629.
  if ((codepoint >= CODEPOINT_LOWEST_SURROGATE_HALF) &&
      (codepoint <= CODEPOINT_HIGHEST_SURROGATE_HALF)) {
    return -1;
  }

  *codepointp = codepoint;
  return len;
}

static int strcpy_utf16(const char *s, uint16_t *buf, int bufsize) {
  int i = 0;
  int buflen = 0;

  while (s[i] != 0) {
    uint32_t codepoint;
    int8_t utf8len = utf8Codepoint((const uint8_t *)s + i, &codepoint);

    if (utf8len < 0) {
      // Invalid utf8 sequence, skip it
      i++;
      continue;
    }

    i += utf8len;

    if (codepoint <= 0xffff) {
      if (buflen == bufsize)
        break;

      buf[buflen++] = codepoint;

    } else {
      if (buflen + 1 >= bufsize)
        break;

      // Surrogate pair
      codepoint -= 0x10000;
      buf[buflen++] = (codepoint >> 10) + 0xd800;
      buf[buflen++] = (codepoint & 0x3ff) + 0xdc00;
    }
  }

  return buflen;
}

#endif // TUSB_OPT_DEVICE_ENABLED
