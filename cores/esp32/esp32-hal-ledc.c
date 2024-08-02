// Copyright 2015-2023 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "soc/soc_caps.h"

#if SOC_LEDC_SUPPORTED
#include "esp32-hal.h"
#include "esp32-hal-ledc.h"
#include "driver/ledc.h"
#include "esp32-hal-periman.h"
#include "soc/gpio_sig_map.h"
#include "esp_rom_gpio.h"
#include "hal/ledc_ll.h"

#ifdef SOC_LEDC_SUPPORT_HS_MODE
#define LEDC_CHANNELS (SOC_LEDC_CHANNEL_NUM << 1)
#else
#define LEDC_CHANNELS (SOC_LEDC_CHANNEL_NUM)
#endif

//Use XTAL clock if possible to avoid timer frequency error when setting APB clock < 80 Mhz
//Need to be fixed in ESP-IDF
#ifdef SOC_LEDC_SUPPORT_XTAL_CLOCK
#define LEDC_DEFAULT_CLK LEDC_USE_XTAL_CLK
#else
#define LEDC_DEFAULT_CLK LEDC_AUTO_CLK
#endif

#define LEDC_MAX_BIT_WIDTH SOC_LEDC_TIMER_BIT_WIDTH

typedef struct {
  int used_channels : LEDC_CHANNELS;  // Used channels as a bits
} ledc_periph_t;

ledc_periph_t ledc_handle = {0};

static bool fade_initialized = false;

static bool ledcDetachBus(void *bus) {
  ledc_channel_handle_t *handle = (ledc_channel_handle_t *)bus;
  bool channel_found = false;
  // Check if more pins are attached to the same ledc channel
  for (uint8_t i = 0; i < SOC_GPIO_PIN_COUNT; i++) {
    if (!perimanPinIsValid(i) || i == handle->pin) {
      continue;  //invalid pin or same pin
    }
    peripheral_bus_type_t type = perimanGetPinBusType(i);
    if (type == ESP32_BUS_TYPE_LEDC) {
      ledc_channel_handle_t *bus_check = (ledc_channel_handle_t *)perimanGetPinBus(i, ESP32_BUS_TYPE_LEDC);
      if (bus_check->channel == handle->channel) {
        channel_found = true;
        break;
      }
    }
  }
  pinMatrixOutDetach(handle->pin, false, false);
  if (!channel_found) {
    ledc_handle.used_channels &= ~(1UL << handle->channel);
  }
  free(handle);
  if (ledc_handle.used_channels == 0) {
    ledc_fade_func_uninstall();
    fade_initialized = false;
  }
  return true;
}

bool ledcAttachChannel(uint8_t pin, uint32_t freq, uint8_t resolution, uint8_t channel) {
  if (channel >= LEDC_CHANNELS) {
    log_e("Channel %u is not available (maximum %u)!", channel, LEDC_CHANNELS);
    return false;
  }
  if (freq == 0) {
    log_e("LEDC pin %u - frequency can't be zero.", pin);
    return false;
  }
  if (resolution == 0 || resolution > LEDC_MAX_BIT_WIDTH) {
    log_e("LEDC pin %u - resolution is zero or it is too big (maximum %u)", pin, LEDC_MAX_BIT_WIDTH);
    return false;
  }

  perimanSetBusDeinit(ESP32_BUS_TYPE_LEDC, ledcDetachBus);
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {
    log_e("Pin %u is already attached to LEDC (channel %u, resolution %u)", pin, bus->channel, bus->channel_resolution);
    return false;
  }

  if (!perimanClearPinBus(pin)) {
    log_e("Pin %u is already attached to another bus and failed to detach", pin);
    return false;
  }

  uint8_t group = (channel / 8), timer = ((channel / 2) % 4);
  bool channel_used = ledc_handle.used_channels & (1UL << channel);
  if (channel_used) {
    log_i("Channel %u is already set up, given frequency and resolution will be ignored", channel);
    if (ledc_set_pin(pin, group, channel % 8) != ESP_OK) {
      log_e("Attaching pin to already used channel failed!");
      return false;
    }
  } else {
    ledc_timer_config_t ledc_timer = {.speed_mode = group, .timer_num = timer, .duty_resolution = resolution, .freq_hz = freq, .clk_cfg = LEDC_DEFAULT_CLK};
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
      log_e("ledc setup failed!");
      return false;
    }

    uint32_t duty = ledc_get_duty(group, (channel % 8));

    ledc_channel_config_t ledc_channel = {
      .speed_mode = group, .channel = (channel % 8), .timer_sel = timer, .intr_type = LEDC_INTR_DISABLE, .gpio_num = pin, .duty = duty, .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
  }

  ledc_channel_handle_t *handle = (ledc_channel_handle_t *)malloc(sizeof(ledc_channel_handle_t));
  handle->pin = pin;
  handle->channel = channel;
#ifndef SOC_LEDC_SUPPORT_FADE_STOP
  handle->lock = NULL;
#endif

  //get resolution of selected channel when used
  if (channel_used) {
    uint32_t channel_resolution = 0;
    ledc_ll_get_duty_resolution(LEDC_LL_GET_HW(), group, timer, &channel_resolution);
    log_i("Channel %u frequency: %u, resolution: %u", channel, ledc_get_freq(group, timer), channel_resolution);
    handle->channel_resolution = (uint8_t)channel_resolution;
  } else {
    handle->channel_resolution = resolution;
    ledc_handle.used_channels |= 1UL << channel;
  }

  if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_LEDC, (void *)handle, group, channel)) {
    ledcDetachBus((void *)handle);
    return false;
  }

  log_i("LEDC attached to pin %u (channel %u, resolution %u)", pin, channel, resolution);
  return true;
}

bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution) {
  int free_channel = ~ledc_handle.used_channels & (ledc_handle.used_channels + 1);
  if (free_channel == 0) {
    log_e("No more LEDC channels available! (maximum is %u channels)", LEDC_CHANNELS);
    return false;
  }
  uint8_t channel = __builtin_ctz(free_channel);  // Convert the free_channel bit to channel number

  return ledcAttachChannel(pin, freq, resolution, channel);
}

bool ledcWrite(uint8_t pin, uint32_t duty) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {

    uint8_t group = (bus->channel / 8), channel = (bus->channel % 8);

    //Fixing if all bits in resolution is set = LEDC FULL ON
    uint32_t max_duty = (1 << bus->channel_resolution) - 1;

    if ((duty == max_duty) && (max_duty != 1)) {
      duty = max_duty + 1;
    }

    ledc_set_duty(group, channel, duty);
    ledc_update_duty(group, channel);

    return true;
  }
  return false;
}

bool ledcWriteChannel(uint8_t channel, uint32_t duty) {
  //check if channel is valid and used
  if (channel >= LEDC_CHANNELS || !(ledc_handle.used_channels & (1UL << channel))) {
    log_e("Channel %u is not available (maximum %u) or not used!", channel, LEDC_CHANNELS);
    return false;
  }
  uint8_t group = (channel / 8), timer = ((channel / 2) % 4);

  //Fixing if all bits in resolution is set = LEDC FULL ON
  uint32_t resolution = 0;
  ledc_ll_get_duty_resolution(LEDC_LL_GET_HW(), group, timer, &resolution);

  uint32_t max_duty = (1 << resolution) - 1;

  if ((duty == max_duty) && (max_duty != 1)) {
    duty = max_duty + 1;
  }

  ledc_set_duty(group, channel, duty);
  ledc_update_duty(group, channel);

  return true;
}

uint32_t ledcRead(uint8_t pin) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {

    uint8_t group = (bus->channel / 8), channel = (bus->channel % 8);
    return ledc_get_duty(group, channel);
  }
  return 0;
}

uint32_t ledcReadFreq(uint8_t pin) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {
    if (!ledcRead(pin)) {
      return 0;
    }
    uint8_t group = (bus->channel / 8), timer = ((bus->channel / 2) % 4);
    return ledc_get_freq(group, timer);
  }
  return 0;
}

uint32_t ledcWriteTone(uint8_t pin, uint32_t freq) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {

    if (!freq) {
      ledcWrite(pin, 0);
      return 0;
    }

    uint8_t group = (bus->channel / 8), timer = ((bus->channel / 2) % 4);

    ledc_timer_config_t ledc_timer = {.speed_mode = group, .timer_num = timer, .duty_resolution = 10, .freq_hz = freq, .clk_cfg = LEDC_DEFAULT_CLK};

    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
      log_e("ledcWriteTone configuration failed!");
      return 0;
    }
    bus->channel_resolution = 10;

    uint32_t res_freq = ledc_get_freq(group, timer);
    ledcWrite(pin, 0x1FF);
    return res_freq;
  }
  return 0;
}

uint32_t ledcWriteNote(uint8_t pin, note_t note, uint8_t octave) {
  const uint16_t noteFrequencyBase[12] = {//   C        C#       D        Eb       E        F       F#        G       G#        A       Bb        B
                                          4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902
  };

  if (octave > 8 || note >= NOTE_MAX) {
    return 0;
  }
  uint32_t noteFreq = (uint32_t)noteFrequencyBase[note] / (uint32_t)(1 << (8 - octave));
  return ledcWriteTone(pin, noteFreq);
}

bool ledcDetach(uint8_t pin) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {
    // will call ledcDetachBus
    return perimanClearPinBus(pin);
  } else {
    log_e("pin %u is not attached to LEDC", pin);
  }
  return false;
}

uint32_t ledcChangeFrequency(uint8_t pin, uint32_t freq, uint8_t resolution) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {
    if (freq == 0) {
      log_e("LEDC pin %u - frequency can't be zero.", pin);
      return 0;
    }
    if (resolution == 0 || resolution > LEDC_MAX_BIT_WIDTH) {
      log_e("LEDC pin %u - resolution is zero or it is too big (maximum %u)", pin, LEDC_MAX_BIT_WIDTH);
      return 0;
    }
    uint8_t group = (bus->channel / 8), timer = ((bus->channel / 2) % 4);

    ledc_timer_config_t ledc_timer = {.speed_mode = group, .timer_num = timer, .duty_resolution = resolution, .freq_hz = freq, .clk_cfg = LEDC_DEFAULT_CLK};

    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
      log_e("ledcChangeFrequency failed!");
      return 0;
    }
    bus->channel_resolution = resolution;
    return ledc_get_freq(group, timer);
  }
  return 0;
}

bool ledcOutputInvert(uint8_t pin, bool out_invert) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {
    gpio_set_level(pin, out_invert);
#ifdef SOC_LEDC_SUPPORT_HS_MODE
    esp_rom_gpio_connect_out_signal(pin, ((bus->channel / 8 == 0) ? LEDC_HS_SIG_OUT0_IDX : LEDC_LS_SIG_OUT0_IDX) + ((bus->channel) % 8), out_invert, 0);
#else
    esp_rom_gpio_connect_out_signal(pin, LEDC_LS_SIG_OUT0_IDX + ((bus->channel) % 8), out_invert, 0);
#endif
    return true;
  }
  return false;
}

static IRAM_ATTR bool ledcFnWrapper(const ledc_cb_param_t *param, void *user_arg) {
  if (param->event == LEDC_FADE_END_EVT) {
    ledc_channel_handle_t *bus = (ledc_channel_handle_t *)user_arg;
#ifndef SOC_LEDC_SUPPORT_FADE_STOP
    portBASE_TYPE xTaskWoken = 0;
    xSemaphoreGiveFromISR(bus->lock, &xTaskWoken);
#endif
    if (bus->fn) {
      if (bus->arg) {
        ((voidFuncPtrArg)bus->fn)(bus->arg);
      } else {
        bus->fn();
      }
    }
  }
  return true;
}

static bool ledcFadeConfig(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void *), void *arg) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {

#ifndef SOC_LEDC_SUPPORT_FADE_STOP
#if !CONFIG_DISABLE_HAL_LOCKS
    if (bus->lock == NULL) {
      bus->lock = xSemaphoreCreateBinary();
      if (bus->lock == NULL) {
        log_e("xSemaphoreCreateBinary failed");
        return false;
      }
      xSemaphoreGive(bus->lock);
    }
    //acquire lock
    if (xSemaphoreTake(bus->lock, 0) != pdTRUE) {
      log_e("LEDC Fade is still running on pin %u! SoC does not support stopping fade.", pin);
      return false;
    }
#endif
#endif
    uint8_t group = (bus->channel / 8), channel = (bus->channel % 8);

    // Initialize fade service.
    if (!fade_initialized) {
      ledc_fade_func_install(0);
      fade_initialized = true;
    }

    bus->fn = (voidFuncPtr)userFunc;
    bus->arg = arg;

    ledc_cbs_t callbacks = {.fade_cb = ledcFnWrapper};
    ledc_cb_register(group, channel, &callbacks, (void *)bus);

    //Fixing if all bits in resolution is set = LEDC FULL ON
    uint32_t max_duty = (1 << bus->channel_resolution) - 1;

    if ((target_duty == max_duty) && (max_duty != 1)) {
      target_duty = max_duty + 1;
    } else if ((start_duty == max_duty) && (max_duty != 1)) {
      start_duty = max_duty + 1;
    }

#if SOC_LEDC_SUPPORT_FADE_STOP
    ledc_fade_stop(group, channel);
#endif

    if (ledc_set_duty_and_update(group, channel, start_duty, 0) != ESP_OK) {
      log_e("ledc_set_duty_and_update failed");
      return false;
    }
    // Wait for LEDCs next PWM cycle to update duty (~ 1-2 ms)
    while (ledc_get_duty(group, channel) != start_duty);

    if (ledc_set_fade_time_and_start(group, channel, target_duty, max_fade_time_ms, LEDC_FADE_NO_WAIT) != ESP_OK) {
      log_e("ledc_set_fade_time_and_start failed");
      return false;
    }
  } else {
    log_e("Pin %u is not attached to LEDC. Call ledcAttach first!", pin);
    return false;
  }
  return true;
}

bool ledcFade(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms) {
  return ledcFadeConfig(pin, start_duty, target_duty, max_fade_time_ms, NULL, NULL);
}

bool ledcFadeWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, voidFuncPtr userFunc) {
  return ledcFadeConfig(pin, start_duty, target_duty, max_fade_time_ms, (voidFuncPtrArg)userFunc, NULL);
}

bool ledcFadeWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void *), void *arg) {
  return ledcFadeConfig(pin, start_duty, target_duty, max_fade_time_ms, userFunc, arg);
}

static uint8_t analog_resolution = 8;
static int analog_frequency = 1000;
void analogWrite(uint8_t pin, int value) {
  // Use ledc hardware for internal pins
  if (pin < SOC_GPIO_PIN_COUNT) {
    ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
    if (bus == NULL && perimanClearPinBus(pin)) {
      if (ledcAttach(pin, analog_frequency, analog_resolution) == 0) {
        log_e("analogWrite setup failed (freq = %u, resolution = %u). Try setting different resolution or frequency");
        return;
      }
    }
    ledcWrite(pin, value);
  }
}

void analogWriteFrequency(uint8_t pin, uint32_t freq) {
  if (ledcChangeFrequency(pin, freq, analog_resolution) == 0) {
    log_e("analogWrite frequency cant be set due to selected resolution! Try to adjust resolution first");
    return;
  }
  analog_frequency = freq;
}

void analogWriteResolution(uint8_t pin, uint8_t resolution) {
  if (ledcChangeFrequency(pin, analog_frequency, resolution) == 0) {
    log_e("analogWrite resolution cant be set due to selected frequency! Try to adjust frequency first");
    return;
  }
  analog_resolution = resolution;
}

#endif /* SOC_LEDC_SUPPORTED */
