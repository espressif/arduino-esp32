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
#include "USBAudioCard.h"
#if SOC_USB_OTG_SUPPORTED
#if CONFIG_TINYUSB_AUDIO_ENABLED

#include "esp32-hal-tinyusb.h"
#include "USBAudioCardDescriptors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <inttypes.h>

ESP_EVENT_DEFINE_BASE(ARDUINO_USB_AUDIO_CARD_EVENTS);
esp_err_t arduino_usb_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
esp_err_t arduino_usb_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

static uint8_t _itf_num = 0;
static uint32_t _sample_rate = 48000;
static uint8_t _spk_channels = 2;
static uint8_t _mic_channels = 2;
static uint8_t _bits_per_sample = 24;
static uint8_t _bytes_per_sample = 4;
static bool _mute[3] = {false, false, false};
static int16_t _volume[3] = {0, 0, 0};
//static int32_t *_mic_buf = NULL; //[CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ / 4]
static int32_t *_spk_buf = NULL;  //[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 4]
static arduino_usb_audio_card_data_handler_t _cb = NULL;
static USBAudioCard *_uac = NULL;

uint16_t tusb_audio_load_descriptor(uint8_t *dst, uint8_t *itf) {
  _itf_num = *itf;
#if TUD_OPT_HIGH_SPEED
  uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB UAC2");
  if (_spk_channels == 2 && _mic_channels > 0) {
    // Stereo Headset
    uint8_t ep_num = tinyusb_get_free_duplex_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t int_ep_num = tinyusb_get_free_in_endpoint();
    TU_VERIFY(int_ep_num != 0);
    uint8_t descriptor[TUD_AUDIO20_HEADSET_STEREO_DESC_LEN] = {
      // Interface number, string index, EP Out & EP In & EP Interrupt address, max sample rate, speaker channels, mic channels, bytes per sample RX/TX, bits used per sample RX/TX
      TUD_AUDIO20_HEADSET_STEREO_DESCRIPTOR(
        _itf_num, str_index, ep_num, (uint8_t)(ep_num | 0x80), (uint8_t)(int_ep_num | 0x80), CFG_TUD_AUDIO_MAX_SAMPLE_RATE, _spk_channels, _mic_channels,
        _bytes_per_sample, _bits_per_sample
      )
    };
    *itf += 3;
    memcpy(dst, descriptor, TUD_AUDIO20_HEADSET_STEREO_DESC_LEN);
    return TUD_AUDIO20_HEADSET_STEREO_DESC_LEN;
  } else if (_spk_channels == 1 && _mic_channels > 0) {
    // Mono Headset
    uint8_t ep_num = tinyusb_get_free_duplex_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t int_ep_num = tinyusb_get_free_in_endpoint();
    TU_VERIFY(int_ep_num != 0);
    uint8_t descriptor[TUD_AUDIO20_HEADSET_MONO_DESC_LEN] = {
      // Interface number, string index, EP Out & EP In & EP Interrupt address, max sample rate, speaker channels, mic channels, bytes per sample RX/TX, bits used per sample RX/TX
      TUD_AUDIO20_HEADSET_MONO_DESCRIPTOR(
        _itf_num, str_index, ep_num, (uint8_t)(ep_num | 0x80), (uint8_t)(int_ep_num | 0x80), CFG_TUD_AUDIO_MAX_SAMPLE_RATE, _spk_channels, _mic_channels,
        _bytes_per_sample, _bits_per_sample
      )
    };
    *itf += 3;
    memcpy(dst, descriptor, TUD_AUDIO20_HEADSET_MONO_DESC_LEN);
    return TUD_AUDIO20_HEADSET_MONO_DESC_LEN;
  } else if (_spk_channels == 2 && _mic_channels == 0) {
    // Stereo Speaker
    uint8_t ep_num = tinyusb_get_free_out_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t int_ep_num = tinyusb_get_free_in_endpoint();
    TU_VERIFY(int_ep_num != 0);
    uint8_t descriptor[TUD_AUDIO20_SPEAKER_STEREO_DESC_LEN] = {
      // Interface number, string index, EP Out & EP In & EP Interrupt address, max sample rate, speaker channels, mic channels, bytes per sample RX/TX, bits used per sample RX/TX
      TUD_AUDIO20_SPEAKER_STEREO_DESCRIPTOR(
        _itf_num, str_index, ep_num, (uint8_t)(int_ep_num | 0x80), CFG_TUD_AUDIO_MAX_SAMPLE_RATE, _spk_channels, _bytes_per_sample, _bits_per_sample
      )
    };
    *itf += 2;
    memcpy(dst, descriptor, TUD_AUDIO20_SPEAKER_STEREO_DESC_LEN);
    return TUD_AUDIO20_SPEAKER_STEREO_DESC_LEN;
  } else if (_spk_channels == 1 && _mic_channels == 0) {
    // Mono Speaker
    uint8_t ep_num = tinyusb_get_free_out_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t int_ep_num = tinyusb_get_free_in_endpoint();
    TU_VERIFY(int_ep_num != 0);
    uint8_t descriptor[TUD_AUDIO20_SPEAKER_MONO_DESC_LEN] = {
      // Interface number, string index, EP Out & EP In & EP Interrupt address, max sample rate, speaker channels, mic channels, bytes per sample RX/TX, bits used per sample RX/TX
      TUD_AUDIO20_SPEAKER_MONO_DESCRIPTOR(
        _itf_num, str_index, ep_num, (uint8_t)(int_ep_num | 0x80), CFG_TUD_AUDIO_MAX_SAMPLE_RATE, _spk_channels, _bytes_per_sample, _bits_per_sample
      )
    };
    *itf += 2;
    memcpy(dst, descriptor, TUD_AUDIO20_SPEAKER_MONO_DESC_LEN);
    return TUD_AUDIO20_SPEAKER_MONO_DESC_LEN;
  } else if (_spk_channels == 0 && _mic_channels > 0) {
    // Microphone(s)
    uint8_t ep_num = tinyusb_get_free_in_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t int_ep_num = tinyusb_get_free_in_endpoint();
    TU_VERIFY(int_ep_num != 0);
    uint8_t descriptor[TUD_AUDIO20_MICROPHONE_DESC_LEN] = {
      // Interface number, string index, EP Out & EP In & EP Interrupt address, max sample rate, speaker channels, mic channels, bytes per sample RX/TX, bits used per sample RX/TX
      TUD_AUDIO20_MICROPHONE_DESCRIPTOR(
        _itf_num, str_index, (uint8_t)(ep_num | 0x80), (uint8_t)(int_ep_num | 0x80), CFG_TUD_AUDIO_MAX_SAMPLE_RATE, _mic_channels, _bytes_per_sample,
        _bits_per_sample
      )
    };
    *itf += 2;
    memcpy(dst, descriptor, TUD_AUDIO20_MICROPHONE_DESC_LEN);
    return TUD_AUDIO20_MICROPHONE_DESC_LEN;
  }
#else
  uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB UAC1");
  if (_spk_channels == 2 && _mic_channels > 0) {
    // Stereo Headset
    uint8_t ep_num = tinyusb_get_free_duplex_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t descriptor[TUD_AUDIO10_HEADSET_STEREO_DESC_LEN(1)] = {
      // Interface number, string index, EP Out & EP In address, max sample rate, speaker channels, mic channels, bytes per sample RX/TX, bits used per sample RX/TX, sample rate
      TUD_AUDIO10_HEADSET_STEREO_DESCRIPTOR(
        _itf_num, str_index, ep_num, (uint8_t)(ep_num | 0x80), 48000, _spk_channels, _mic_channels, _bytes_per_sample, _bits_per_sample, _sample_rate
      )
    };
    *itf += 3;
    memcpy(dst, descriptor, TUD_AUDIO10_HEADSET_STEREO_DESC_LEN(1));
    return TUD_AUDIO10_HEADSET_STEREO_DESC_LEN(1);
  } else if (_spk_channels == 1 && _mic_channels > 0) {
    // Mono Headset
    uint8_t ep_num = tinyusb_get_free_duplex_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t descriptor[TUD_AUDIO10_HEADSET_MONO_DESC_LEN(1)] = {
      // Interface number, string index, EP Out & EP In address, max sample rate, speaker channels, mic channels, bytes per sample RX/TX, bits used per sample RX/TX, sample rate
      TUD_AUDIO10_HEADSET_MONO_DESCRIPTOR(
        _itf_num, str_index, ep_num, (uint8_t)(ep_num | 0x80), 48000, _spk_channels, _mic_channels, _bytes_per_sample, _bits_per_sample, _sample_rate
      )
    };
    *itf += 3;
    memcpy(dst, descriptor, TUD_AUDIO10_HEADSET_MONO_DESC_LEN(1));
    return TUD_AUDIO10_HEADSET_MONO_DESC_LEN(1);
  } else if (_spk_channels == 2 && _mic_channels == 0) {
    // Stereo Speaker
    uint8_t ep_num = tinyusb_get_free_out_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t descriptor[TUD_AUDIO10_SPEAKER_STEREO_DESC_LEN(1)] = {
      // Interface number, string index, EP Out address, max sample rate, speaker channels, bytes per sample RX/TX, bits used per sample RX/TX, sample rate
      TUD_AUDIO10_SPEAKER_STEREO_DESCRIPTOR(_itf_num, str_index, ep_num, 48000, _spk_channels, _bytes_per_sample, _bits_per_sample, _sample_rate)
    };
    *itf += 2;
    memcpy(dst, descriptor, TUD_AUDIO10_SPEAKER_STEREO_DESC_LEN(1));
    return TUD_AUDIO10_SPEAKER_STEREO_DESC_LEN(1);
  } else if (_spk_channels == 1 && _mic_channels == 0) {
    // Mono Speaker
    uint8_t ep_num = tinyusb_get_free_out_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t descriptor[TUD_AUDIO10_SPEAKER_MONO_DESC_LEN(1)] = {
      // Interface number, string index, EP Out address, max sample rate, speaker channels, bytes per sample RX/TX, bits used per sample RX/TX, sample rate
      TUD_AUDIO10_SPEAKER_MONO_DESCRIPTOR(_itf_num, str_index, ep_num, 48000, _spk_channels, _bytes_per_sample, _bits_per_sample, _sample_rate)
    };
    *itf += 2;
    memcpy(dst, descriptor, TUD_AUDIO10_SPEAKER_MONO_DESC_LEN(1));
    return TUD_AUDIO10_SPEAKER_MONO_DESC_LEN(1);
  } else if (_spk_channels == 0 && _mic_channels > 0) {
    // Microphone(s)
    uint8_t ep_num = tinyusb_get_free_in_endpoint();
    TU_VERIFY(ep_num != 0);
    uint8_t descriptor[TUD_AUDIO10_MICROPHONE_DESC_LEN(1)] = {
      // Interface number, string index, EP In address, max sample rate, microphone channels, bytes per sample RX/TX, bits used per sample RX/TX, sample rate
      TUD_AUDIO10_MICROPHONE_DESCRIPTOR(_itf_num, str_index, (uint8_t)(ep_num | 0x80), 48000, _mic_channels, _bytes_per_sample, _bits_per_sample, _sample_rate)
    };
    *itf += 2;
    memcpy(dst, descriptor, TUD_AUDIO10_MICROPHONE_DESC_LEN(1));
    return TUD_AUDIO10_MICROPHONE_DESC_LEN(1);
  }
#endif
  return 0;
}

TaskHandle_t _uacReceiveTaskHandle = NULL;
void _uacReceiveTask(void *pvParameters) {
  for (;;) {
    uint16_t len = tud_audio_read(_spk_buf, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ);
    if (len > 0 && _cb != NULL) {
      _cb(_spk_buf, len);
    }
    delay(2);
  }
}

#define dump_control_request(p_request)                                                                                                          \
  log_v(                                                                                                                                         \
    "Control request: bRequest = 0x%x, wValue = 0x%x, wIndex = 0x%x, wLength = 0x%x", p_request->bRequest, p_request->wValue, p_request->wIndex, \
    p_request->wLength                                                                                                                           \
  )

// Invoked when audio class specific set request received for an EP
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
  (void)rhport;
  dump_control_request(p_request);
#if TUD_OPT_HIGH_SPEED
  (void)pBuff;
  (void)p_request;
  return false;
#else
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  if (ctrlSel == AUDIO10_EP_CTRL_SAMPLING_FREQ && p_request->bRequest == AUDIO10_CS_REQ_SET_CUR && p_request->wLength == 3) {
    _sample_rate = tu_unaligned_read32(pBuff) & 0x00FFFFFF;
    log_d("EP set current freq: %" PRIu32, _sample_rate);
    // Send SAMPLE RATE Event
    arduino_usb_audio_card_event_data_t p;
    p.sample_rate.rate = _sample_rate;
    arduino_usb_event_post(
      ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY
    );
    return true;
  }
  log_w("Set EP request not handled, ctrlSel = %d, bRequest = %d, wLength = %d", ctrlSel, p_request->bRequest, p_request->wLength);
  return false;
#endif
}

// Invoked when audio class specific get request received for an EP
bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  dump_control_request(p_request);
#if TUD_OPT_HIGH_SPEED
  (void)rhport;
  (void)p_request;
  return false;
#else
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  if (ctrlSel == AUDIO10_EP_CTRL_SAMPLING_FREQ && p_request->bRequest == AUDIO10_CS_REQ_GET_CUR) {
    log_d("EP get current freq");
    uint8_t freq[3];
    freq[0] = (uint8_t)(_sample_rate & 0xFF);
    freq[1] = (uint8_t)((_sample_rate >> 8) & 0xFF);
    freq[2] = (uint8_t)((_sample_rate >> 16) & 0xFF);
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, freq, sizeof(freq));
  }
  log_w("Get EP request not handled, ctrlSel = %d, bRequest = %d, wLength = %d", ctrlSel, p_request->bRequest, p_request->wLength);
  return false;
#endif
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  dump_control_request(p_request);
#if TUD_OPT_HIGH_SPEED
  audio20_control_request_t const *request = (audio20_control_request_t const *)p_request;
  if (request->bEntityID == UAC2_ENTITY_CLOCK) {
    if (request->bControlSelector == AUDIO20_CS_CTRL_SAM_FREQ) {
      if (request->bRequest == AUDIO20_CS_REQ_CUR) {
        log_d("Clock get current freq %" PRIu32, _sample_rate);
        audio20_control_cur_4_t curf = {(int32_t)tu_htole32(_sample_rate)};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &curf, sizeof(curf));
      } else if (request->bRequest == AUDIO20_CS_REQ_RANGE) {
        audio20_control_range_4_n_t(1) rangef = {.wNumSubRanges = tu_htole16(1)};
        rangef.subrange[0].bMin = (int32_t)tu_htole32(_sample_rate);
        rangef.subrange[0].bMax = (int32_t)tu_htole32(_sample_rate);
        rangef.subrange[0].bRes = (int32_t)tu_htole32(0);
        log_d("Clock Range %" PRIu32 ", %" PRIu32 ", %d", _sample_rate, _sample_rate, 0);
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &rangef, sizeof(rangef));
      }
    } else if (request->bControlSelector == AUDIO20_CS_CTRL_CLK_VALID && request->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_1_t cur_valid = {.bCur = 1};
      log_d("Clock get is valid %u", cur_valid.bCur);
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_valid, sizeof(cur_valid));
    }
    log_w("Clock get request not supported, entity = %u, selector = %u, request = %u", request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
  } else if (request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT) {
    uint8_t channel = request->bChannelNumber;
    if (channel != 0 && channel > _spk_channels) {
      log_w("Invalid speaker channel %u for feature unit get request (spk_channels=%u)", channel, _spk_channels);
      return false;
    }
    if (request->bControlSelector == AUDIO20_FU_CTRL_MUTE && request->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_1_t mute1 = {.bCur = _mute[request->bChannelNumber]};
      log_d("Get channel %u mute %d", request->bChannelNumber, mute1.bCur);
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &mute1, sizeof(mute1));
    } else if (request->bControlSelector == AUDIO20_FU_CTRL_VOLUME) {
      if (request->bRequest == AUDIO20_CS_REQ_RANGE) {
        audio20_control_range_2_n_t(1)
          range_vol = {.wNumSubRanges = tu_htole16(1), .subrange = {{.bMin = tu_htole16(-50 * 256), .bMax = tu_htole16(0), .bRes = tu_htole16(256)}}};
        log_d(
          "Get channel %u volume range (%d, %d, %u) dB", request->bChannelNumber, range_vol.subrange[0].bMin / 256, range_vol.subrange[0].bMax / 256,
          range_vol.subrange[0].bRes / 256
        );
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &range_vol, sizeof(range_vol));
      } else if (request->bRequest == AUDIO20_CS_REQ_CUR) {
        audio20_control_cur_2_t cur_vol = {.bCur = tu_htole16(_volume[request->bChannelNumber])};
        log_d("Get channel %u volume %d dB", request->bChannelNumber, cur_vol.bCur / 256);
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *)request, &cur_vol, sizeof(cur_vol));
      }
    }
    log_w("Feature unit get request not supported, entity = %u, selector = %u, request = %u", request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
  }
  log_w("Get request not handled, entity = %d, selector = %d, request = %d", request->bEntityID, request->bControlSelector, request->bRequest);
  return false;
#else
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

  if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT) {
    if (ctrlSel == AUDIO10_FU_CTRL_MUTE) {
      uint8_t muted = _mute[channelNum];
      log_d("Get Mute of channel: %u", channelNum);
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &muted, 1);
    } else if (ctrlSel == AUDIO10_FU_CTRL_VOLUME) {
      if (p_request->bRequest == AUDIO10_CS_REQ_GET_CUR) {
        log_d("Get Volume of channel: %u", channelNum);
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &_volume[channelNum], sizeof(_volume[channelNum]));
      } else if (p_request->bRequest == AUDIO10_CS_REQ_GET_MIN) {
        log_d("Get Volume min of channel: %u", channelNum);
        int16_t vol_min = (int16_t)(-50 * 256);
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &vol_min, sizeof(vol_min));
      } else if (p_request->bRequest == AUDIO10_CS_REQ_GET_MAX) {
        log_d("Get Volume max of channel: %u", channelNum);
        int16_t vol_max = 0;
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &vol_max, sizeof(vol_max));
      } else if (p_request->bRequest == AUDIO10_CS_REQ_GET_RES) {
        log_d("Get Volume res of channel: %u", channelNum);
        int16_t vol_res = 256;  // 1 dB in 1/256 dB units
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &vol_res, sizeof(vol_res));
      } else {
        log_w(
          "Get Volume request not supported, entityID = %d, ctrlSel = %d, bRequest = %d, wLength = %d", entityID, ctrlSel, p_request->bRequest,
          p_request->wLength
        );
        return false;
      }
    }
  }
  log_w("Get request not handled, entityID = %d, ctrlSel = %d, bRequest = %d, wLength = %d", entityID, ctrlSel, p_request->bRequest, p_request->wLength);
  return false;
#endif
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
  (void)rhport;
  dump_control_request(p_request);
#if TUD_OPT_HIGH_SPEED
  audio20_control_request_t const *request = (audio20_control_request_t const *)p_request;
  if (request->bEntityID == UAC2_ENTITY_SPK_FEATURE_UNIT && request->bRequest == AUDIO20_CS_REQ_CUR) {
    if (request->bControlSelector == AUDIO20_FU_CTRL_MUTE) {
      TU_VERIFY(request->wLength == sizeof(audio20_control_cur_1_t));
      _mute[request->bChannelNumber] = ((audio20_control_cur_1_t const *)buf)->bCur;
      log_d("Set channel %d Mute: %d", request->bChannelNumber, _mute[request->bChannelNumber]);
      // Send MUTE Event
      arduino_usb_audio_card_event_data_t p;
      p.mute.channel = (UAC_Channel)request->bChannelNumber;
      p.mute.muted = _mute[request->bChannelNumber];
      arduino_usb_event_post(ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_MUTE_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY);
      return true;
    } else if (request->bControlSelector == AUDIO20_FU_CTRL_VOLUME) {
      TU_VERIFY(request->wLength == sizeof(audio20_control_cur_2_t));
      _volume[request->bChannelNumber] = ((audio20_control_cur_2_t const *)buf)->bCur;
      log_d("Set channel %d volume: %d dB", request->bChannelNumber, _volume[request->bChannelNumber] / 256);
      // Send Volume Event
      arduino_usb_audio_card_event_data_t p;
      p.volume.channel = (UAC_Channel)request->bChannelNumber;
      p.volume.db = _volume[request->bChannelNumber] / 256;
      arduino_usb_event_post(
        ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY
      );
      return true;
    }
    log_w("Feature unit set request not supported, entity = %u, selector = %u, request = %u", request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
  } else if (request->bEntityID == UAC2_ENTITY_CLOCK && request->bRequest == AUDIO20_CS_REQ_CUR) {
    if (request->bControlSelector == AUDIO20_CS_CTRL_SAM_FREQ) {
      TU_VERIFY(request->wLength == sizeof(audio20_control_cur_4_t));
      _sample_rate = (uint32_t)((audio20_control_cur_4_t const *)buf)->bCur;
      log_d("Clock set current freq: %" PRIu32, _sample_rate);
      // Send SAMPLE RATE Event
      arduino_usb_audio_card_event_data_t p;
      p.sample_rate.rate = _sample_rate;
      arduino_usb_event_post(
        ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY
      );
      return true;
    }
    log_w("Clock set request not supported, entity = %u, selector = %u, request = %u", request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
  }
  log_w("Set request not handled, entity = %d, selector = %d, request = %d", request->bEntityID, request->bControlSelector, request->bRequest);
  return false;
#else
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

  if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT && p_request->bRequest == AUDIO10_CS_REQ_SET_CUR) {
    if (ctrlSel == AUDIO10_FU_CTRL_MUTE && p_request->wLength == 1) {
      _mute[channelNum] = buf[0];
      log_d("Set Mute: %d of channel: %u", _mute[channelNum], channelNum);
      // Send MUTE Event
      arduino_usb_audio_card_event_data_t p;
      p.mute.channel = (UAC_Channel)channelNum;
      p.mute.muted = _mute[channelNum];
      arduino_usb_event_post(ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_MUTE_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY);
      return true;
    } else if (ctrlSel == AUDIO10_FU_CTRL_VOLUME && p_request->wLength == 2) {
      _volume[channelNum] = (int16_t)tu_unaligned_read16(buf);
      log_d("Set Volume: %d dB of channel: %u", _volume[channelNum] / 256, channelNum);
      // Send Volume Event
      arduino_usb_audio_card_event_data_t p;
      p.volume.channel = (UAC_Channel)channelNum;
      p.volume.db = _volume[channelNum] / 256;
      arduino_usb_event_post(
        ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY
      );
      return true;
    }
  }
  log_w("Set request not handled, entityID = %d, ctrlSel = %d, bRequest = %d, wLength = %d", entityID, ctrlSel, p_request->bRequest, p_request->wLength);
  return false;
#endif
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void)rhport;
  dump_control_request(p_request);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex)) - _itf_num;
  uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));
  log_d("Close EP interface %d alt %d", itf, alt);
#endif
  return true;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void)rhport;
  dump_control_request(p_request);
  uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex)) - _itf_num;
  uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));
  log_d("Set interface %d alt %d", itf, alt);
  // Send Interface Event
  arduino_usb_audio_card_event_data_t p;
  if (_spk_channels > 0 && itf == 1) {
    p.interface_enable.interface = UAC_INTERFACE_SPK;
  } else if (_spk_channels == 0 && itf == 1) {
    p.interface_enable.interface = UAC_INTERFACE_MIC;
  } else {
    p.interface_enable.interface = UAC_INTERFACE_MIC;
  }
  p.interface_enable.enable = alt != 0;
  arduino_usb_event_post(
    ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY
  );
  return true;
}

USBAudioCard::USBAudioCard(uint32_t sample_rate, UAC_Bits_Per_Sample bps, UAC_SPK_Channels spk_channels, UAC_MIC_Channels mic_channels) {
  if (_uac == NULL) {
    if ((uint8_t)spk_channels > 2) {
      log_e("Maximum of 2 speaker channels supported!");
      return;
    }
    _uac = this;
    _sample_rate = sample_rate;
    _bits_per_sample = (uint8_t)bps;
    _bytes_per_sample = (_bits_per_sample <= 16) ? 2 : 4;
    _spk_channels = (uint8_t)spk_channels;
    _mic_channels = (uint8_t)mic_channels;

    uint16_t descriptor_len = 0;
#if TUD_OPT_HIGH_SPEED
    if (_sample_rate > CFG_TUD_AUDIO_MAX_SAMPLE_RATE) {
      log_e("Maximum %u sample rate supported!", CFG_TUD_AUDIO_MAX_SAMPLE_RATE);
      _uac = NULL;
      return;
    }
    if (_spk_channels == 2 && _mic_channels > 0) {
      // Stereo Headset
      descriptor_len = TUD_AUDIO20_HEADSET_STEREO_DESC_LEN;
    } else if (_spk_channels == 1 && _mic_channels > 0) {
      // Mono Headset
      descriptor_len = TUD_AUDIO20_HEADSET_MONO_DESC_LEN;
    } else if (_spk_channels == 2 && _mic_channels == 0) {
      // Stereo Speaker
      descriptor_len = TUD_AUDIO20_SPEAKER_STEREO_DESC_LEN;
    } else if (_spk_channels == 1 && _mic_channels == 0) {
      // Mono Speaker
      descriptor_len = TUD_AUDIO20_SPEAKER_MONO_DESC_LEN;
    } else if (_spk_channels == 0 && _mic_channels > 0) {
      // Microphone(s)
      descriptor_len = TUD_AUDIO20_MICROPHONE_DESC_LEN;
    }
#else
    if (_sample_rate > 48000) {
      log_e("Maximum 48000 sample rate supported!");
      _uac = NULL;
      return;
    }
    if (((_spk_channels + _mic_channels) * _bytes_per_sample) > 8) {
      log_e("Too many channels or too high bits per sample selected! Audio might not work!");
    }
    if (_spk_channels == 2 && _mic_channels > 0) {
      // Stereo Headset
      descriptor_len = TUD_AUDIO10_HEADSET_STEREO_DESC_LEN(1);
    } else if (_spk_channels == 1 && _mic_channels > 0) {
      // Mono Headset
      descriptor_len = TUD_AUDIO10_HEADSET_MONO_DESC_LEN(1);
    } else if (_spk_channels == 2 && _mic_channels == 0) {
      // Stereo Speaker
      descriptor_len = TUD_AUDIO10_SPEAKER_STEREO_DESC_LEN(1);
    } else if (_spk_channels == 1 && _mic_channels == 0) {
      // Mono Speaker
      descriptor_len = TUD_AUDIO10_SPEAKER_MONO_DESC_LEN(1);
    } else if (_spk_channels == 0 && _mic_channels > 0) {
      // Microphone(s)
      descriptor_len = TUD_AUDIO10_MICROPHONE_DESC_LEN(1);
    }
#endif
    tinyusb_enable_interface(USB_INTERFACE_AUDIO, descriptor_len, tusb_audio_load_descriptor);
  }
}

USBAudioCard::~USBAudioCard() {
  end();
  _uac = NULL;
}

bool USBAudioCard::begin() {
  if (_spk_channels > 0 && _spk_buf == NULL) {
    _spk_buf = (int32_t *)malloc(CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ);
    if (_spk_buf == NULL) {
      log_e("Failed to allocate speaker data buffer!");
      return false;
    }
    // spawn receive task
    BaseType_t created = xTaskCreate(_uacReceiveTask, "uacReceiveTask", 8092, this, 5, &_uacReceiveTaskHandle);
    if (created != pdPASS || _uacReceiveTaskHandle == NULL) {
      log_e("Failed to create receive task!");
      free(_spk_buf);
      _spk_buf = NULL;
      return false;
    }
  }
  return true;
}

void USBAudioCard::end() {
  if (_uacReceiveTaskHandle != NULL) {
    vTaskDelete(_uacReceiveTaskHandle);
    _uacReceiveTaskHandle = NULL;
  }
  if (_spk_buf != NULL) {
    free(_spk_buf);
    _spk_buf = NULL;
  }
}

UAC_SPK_Channels USBAudioCard::speakerChannels() {
  return (UAC_SPK_Channels)_spk_channels;
}
UAC_MIC_Channels USBAudioCard::micChannels() {
  return (UAC_MIC_Channels)_mic_channels;
}
UAC_Bits_Per_Sample USBAudioCard::bitsPerSample() {
  return (UAC_Bits_Per_Sample)_bits_per_sample;
}
uint8_t USBAudioCard::bytesPerSample() {
  return _bytes_per_sample;
}
uint32_t USBAudioCard::sampleRate() {
  return _sample_rate;
}

bool USBAudioCard::mute(UAC_Channel channel) {
  if (_spk_channels == 0 || channel > _spk_channels) {
    log_e("Bad speaker channel %u", channel);
    return false;
  }
  return _mute[channel];
}

bool USBAudioCard::mute(UAC_Channel channel, bool muted) {
#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
  if (_spk_channels == 0 || channel > _spk_channels) {
    log_e("Bad speaker channel %u", channel);
    return false;
  }

  _mute[channel] = muted;

  // 6.1 Interrupt Data Message
  const audio_interrupt_data_t data =
    {.v2 = {
       .bInfo = 0,                                        // Class-specific interrupt, originated from an interface
       .bAttribute = AUDIO20_CS_REQ_CUR,                  // Caused by current settings
       .wValue_cn_or_mcn = (uint8_t)channel,              // CH0: master volume
       .wValue_cs = AUDIO20_FU_CTRL_MUTE,                 // Volume change
       .wIndex_ep_or_int = 0,                             // From the interface itself
       .wIndex_entity_id = UAC2_ENTITY_SPK_FEATURE_UNIT,  // From feature unit
     }};

  if (tud_audio_int_write(&data)) {
    // Send MUTE Event
    arduino_usb_audio_card_event_data_t p;
    p.mute.channel = channel;
    p.mute.muted = _mute[channel];
    arduino_usb_event_post(ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_MUTE_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY);
    return true;
  }
  return false;
#else
  return false;
#endif
}

int8_t USBAudioCard::volume(UAC_Channel channel) {
  if (_spk_channels == 0 || channel > _spk_channels) {
    log_e("Bad speaker channel %u", channel);
    return INT8_MIN;
  }
  return _volume[channel] / 256;
}

bool USBAudioCard::volume(UAC_Channel channel, int8_t volume_db) {
#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
  if (_spk_channels == 0 || channel > _spk_channels) {
    log_e("Bad speaker channel %u", channel);
    return false;
  }
  _volume[channel] = volume_db * 256;

  // 6.1 Interrupt Data Message
  const audio_interrupt_data_t data =
    {.v2 = {
       .bInfo = 0,                                        // Class-specific interrupt, originated from an interface
       .bAttribute = AUDIO20_CS_REQ_CUR,                  // Caused by current settings
       .wValue_cn_or_mcn = (uint8_t)channel,              // CH0: master volume
       .wValue_cs = AUDIO20_FU_CTRL_VOLUME,               // Volume change
       .wIndex_ep_or_int = 0,                             // From the interface itself
       .wIndex_entity_id = UAC2_ENTITY_SPK_FEATURE_UNIT,  // From feature unit
     }};

  if (tud_audio_int_write(&data)) {
    // Send Volume Event
    arduino_usb_audio_card_event_data_t p;
    p.volume.channel = channel;
    p.volume.db = _volume[channel] / 256;
    arduino_usb_event_post(ARDUINO_USB_AUDIO_CARD_EVENTS, ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT, &p, sizeof(arduino_usb_audio_card_event_data_t), portMAX_DELAY);
    return true;
  }
  return false;
#else
  return false;
#endif
}

// Apply currently set channel volumes directly to audio data (if DAC does not support setting volume)
void USBAudioCard::applyVolume(void *data, uint16_t len) {
  if (_spk_channels == 0) {
    return;
  }
  // Convert volume from 1/256 dB to linear multiplier (Q16 fixed-point)
  // volume[0] ranges from -12800 (-50 dB) to 0 (0 dB) in 1/256 dB units
  if (_mute[0]) {
    memset(data, 0, len);
  } else if (_volume[0] != 0) {
    int32_t vol_mul = _mute[0] ? 0 : (int32_t)(powf(10.0f, _volume[0] / 256.0f / 20.0f) * 65536.0f);
    if (_bits_per_sample == 16) {
      int16_t *src = (int16_t *)data;
      int16_t *limit = (int16_t *)data + len / 2;
      while (src < limit) {
        *src = (int16_t)(((int32_t)*src * vol_mul) >> 16);
        src++;
      }
    } else {
      int32_t *src = (int32_t *)data;
      int32_t *limit = (int32_t *)data + len / 4;
      while (src < limit) {
        *src = (int32_t)(((int64_t)*src * vol_mul) >> 16);
        src++;
      }
    }
  }

  // Apply volume to secondary channels
  if (_volume[1] != 0 || _volume[2] != 0 || _mute[1] || _mute[2]) {
    int32_t vol_mul1 = _mute[1] ? 0 : (int32_t)(powf(10.0f, _volume[1] / 256.0f / 20.0f) * 65536.0f);
    int32_t vol_mul2 = _mute[2] ? 0 : (int32_t)(powf(10.0f, _volume[2] / 256.0f / 20.0f) * 65536.0f);
    if (_bits_per_sample == 16) {
      int16_t *src = (int16_t *)data;
      int16_t *limit = (int16_t *)data + len / 2;
      while (src < limit) {
        *src = (int16_t)(((int32_t)*src * vol_mul1) >> 16);
        src++;
        if (_spk_channels == 2) {
          *src = (int16_t)(((int32_t)*src * vol_mul2) >> 16);
          src++;
        }
      }
    } else {
      int32_t *src = (int32_t *)data;
      int32_t *limit = (int32_t *)data + len / 4;
      while (src < limit) {
        *src = (int32_t)(((int64_t)*src * vol_mul1) >> 16);
        src++;
        if (_spk_channels == 2) {
          *src = (int32_t)(((int64_t)*src * vol_mul2) >> 16);
          src++;
        }
      }
    }
  }
}

void USBAudioCard::onData(arduino_usb_audio_card_data_handler_t callback) {
  _cb = callback;
}

uint16_t USBAudioCard::write(const void *data, uint16_t len) {
  if (_uac != NULL) {
    return tud_audio_write(data, len);
  }
  return 0;
}

void USBAudioCard::onEvent(esp_event_handler_t callback) {
  onEvent(ARDUINO_USB_AUDIO_CARD_ANY_EVENT, callback);
}

void USBAudioCard::onEvent(arduino_usb_audio_card_event_t event, esp_event_handler_t callback) {
  arduino_usb_event_handler_register_with(ARDUINO_USB_AUDIO_CARD_EVENTS, event, callback, this);
}

#endif /* CONFIG_TINYUSB_AUDIO_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
