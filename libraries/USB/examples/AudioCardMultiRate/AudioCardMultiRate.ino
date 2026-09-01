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

/*
  USB Audio Card with multiple sample rates (device mode)

  Based on the AudioCard example, this example demonstrates UAC1 with multiple
  discrete sample rates, 44.1 kHz and 48 kHz, allowing the host to switch
  between them.

  When the host changes the sample rate, USBAudioCard raises the
  ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT. The example re-tunes the I2S TX
  interface with configureTX() so the DAC follows the new sample rate.

  NOTE: Multiple sample rate support is currently implemented for UAC1.
  UAC2 uses a different clock and sample-rate control mechanism and is outside
  the scope of this example.
*/

#include <Arduino.h>
#include "ESP_I2S.h"
#include "USB.h"
#include "USBAudioCard.h"

// The UAC1 implementation used by this example runs in Full-Speed mode,
// so this example is limited to ESP32-S2/S3.
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
#define I2S_BCLK  4
#define I2S_LRCK  5
#define I2S_DOUT  6
#define I2S_DIN   7
#define I2S_WIDTH I2S_DATA_BIT_WIDTH_16BIT
#define UAC_BPS   UAC_BPS_16
#else
#error This example requires an ESP32-S2 or an ESP32-S3 (UAC1 Full-Speed)
#endif

// Advertise 48 kHz (default) and 44.1 kHz. The first array entry is the initial
// UAC rate, so I2S starts from UAC_SAMPLE_RATES[0] to stay in sync with the USB side.
static const uint32_t UAC_SAMPLE_RATES[] = {48000, 44100};
constexpr size_t UAC_SAMPLE_RATE_COUNT = sizeof(UAC_SAMPLE_RATES) / sizeof(UAC_SAMPLE_RATES[0]);

USBAudioCard uac(UAC_SAMPLE_RATES, UAC_SAMPLE_RATE_COUNT, UAC_BPS, UAC_SPK_STEREO, UAC_MIC_NONE);

I2SClass i2s;

// Re-tune I2S TX to the USB sample rate selected by the host.
void configureI2SForSampleRate(uint32_t rate) {
  bool configured = i2s.configureTX(rate, I2S_WIDTH, I2S_SLOT_MODE_STEREO);

  Serial.printf("I2S CONFIG: %lu Hz | TX: %s\r\n", rate, configured ? "OK" : "FAIL");
}

// Handle USB link state and UAC sample-rate events.
static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT: Serial.println("USB PLUGGED"); break;
      case ARDUINO_USB_STOPPED_EVENT: Serial.println("USB UNPLUGGED"); break;
      case ARDUINO_USB_SUSPEND_EVENT: Serial.printf("USB SUSPENDED: remote_wakeup_en: %u\r\n", data->suspend.remote_wakeup_en); break;
      case ARDUINO_USB_RESUME_EVENT:  Serial.println("USB RESUMED"); break;
      default:                        break;
    }
  } else if (event_base == ARDUINO_USB_AUDIO_CARD_EVENTS) {
    arduino_usb_audio_card_event_data_t *data = (arduino_usb_audio_card_event_data_t *)event_data;
    switch (event_id) {
      case ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT:
        Serial.printf("AUDIO SAMPLE RATE: %lu\r\n", data->sample_rate.rate);
        configureI2SForSampleRate(data->sample_rate.rate);
        break;
      default: break;
    }
  }
}

// Invoked when the host sends playback PCM: apply UAC volume curve, then output on I2S.
void onSpkData(void *data, uint16_t len) {
  uac.applyVolume(data, len);
  i2s.write((const uint8_t *)data, len);
}

void setup() {
  Serial.begin(115200);

  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_DOUT, I2S_DIN);
  i2s.begin(I2S_MODE_STD, UAC_SAMPLE_RATES[0], I2S_WIDTH, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);

  uac.onEvent(usbEventCallback);
  uac.onData(onSpkData);
  uac.begin();

  // Stack-wide USB events (same handler also receives ARDUINO_USB_AUDIO_CARD_EVENTS above).
  USB.onEvent(usbEventCallback);
  USB.begin();
}

void loop() {}
