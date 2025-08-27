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
#if SOC_LEDC_GAMMA_CURVE_FADE_SUPPORTED
#include <math.h>
#endif

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

// Helper function to find a timer with matching frequency and resolution
static bool find_matching_timer(uint8_t speed_mode, uint32_t freq, uint8_t resolution, uint8_t *timer_num) {
  log_d("Searching for timer with freq=%u, resolution=%u", freq, resolution);
  // Check all channels to find one with matching frequency and resolution
  for (uint8_t i = 0; i < SOC_GPIO_PIN_COUNT; i++) {
    if (!perimanPinIsValid(i)) {
      continue;
    }
    peripheral_bus_type_t type = perimanGetPinBusType(i);
    if (type == ESP32_BUS_TYPE_LEDC) {
      ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(i, ESP32_BUS_TYPE_LEDC);
      if (bus != NULL && (bus->channel / SOC_LEDC_CHANNEL_NUM) == speed_mode && bus->freq_hz == freq && bus->channel_resolution == resolution) {
        log_d("Found matching timer %u for freq=%u, resolution=%u", bus->timer_num, freq, resolution);
        *timer_num = bus->timer_num;
        return true;
      }
    }
  }
  log_d("No matching timer found for freq=%u, resolution=%u", freq, resolution);
  return false;
}

// Helper function to find an unused timer
static bool find_free_timer(uint8_t speed_mode, uint8_t *timer_num) {
  // Check which timers are in use
  uint8_t used_timers = 0;
  for (uint8_t i = 0; i < SOC_GPIO_PIN_COUNT; i++) {
    if (!perimanPinIsValid(i)) {
      continue;
    }
    peripheral_bus_type_t type = perimanGetPinBusType(i);
    if (type == ESP32_BUS_TYPE_LEDC) {
      ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(i, ESP32_BUS_TYPE_LEDC);
      if (bus != NULL && (bus->channel / SOC_LEDC_CHANNEL_NUM) == speed_mode) {
        log_d("Timer %u is in use by channel %u", bus->timer_num, bus->channel);
        used_timers |= (1 << bus->timer_num);
      }
    }
  }

#ifndef SOC_LEDC_TIMER_NUM
#define SOC_LEDC_TIMER_NUM 4
#endif
  // Find first unused timer
  for (uint8_t i = 0; i < SOC_LEDC_TIMER_NUM; i++) {
    if (!(used_timers & (1 << i))) {
      log_d("Found free timer %u", i);
      *timer_num = i;
      return true;
    }
  }
  log_e("No free timers available");
  return false;
}

// Helper function to remove a channel from a timer and clear timer if no channels are using it
static void remove_channel_from_timer(uint8_t speed_mode, uint8_t timer_num, uint8_t channel) {
  log_d("Removing channel %u from timer %u in speed_mode %u", channel, timer_num, speed_mode);

  // Check if any other channels are using this timer
  bool timer_in_use = false;
  for (uint8_t i = 0; i < SOC_GPIO_PIN_COUNT; i++) {
    if (!perimanPinIsValid(i)) {
      continue;
    }
    peripheral_bus_type_t type = perimanGetPinBusType(i);
    if (type == ESP32_BUS_TYPE_LEDC) {
      ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(i, ESP32_BUS_TYPE_LEDC);
      if (bus != NULL && (bus->channel / SOC_LEDC_CHANNEL_NUM) == speed_mode && bus->timer_num == timer_num && bus->channel != channel) {
        log_d("Timer %u is still in use by channel %u", timer_num, bus->channel);
        timer_in_use = true;
        break;
      }
    }
  }

  if (!timer_in_use) {
    log_d("No other channels using timer %u, deconfiguring timer", timer_num);
    // Stop the timer
    ledc_timer_pause(speed_mode, timer_num);
    // Deconfigure the timer
    ledc_timer_config_t ledc_timer;
    memset((void *)&ledc_timer, 0, sizeof(ledc_timer_config_t));
    ledc_timer.speed_mode = speed_mode;
    ledc_timer.timer_num = timer_num;
    ledc_timer.deconfigure = true;
    ledc_timer_config(&ledc_timer);
  }
}

static bool fade_initialized = false;

static ledc_clk_cfg_t clock_source = LEDC_DEFAULT_CLK;

ledc_clk_cfg_t ledcGetClockSource(void) {
  return clock_source;
}

bool ledcSetClockSource(ledc_clk_cfg_t source) {
  if (ledc_handle.used_channels) {
    log_e("Cannot change LEDC clock source! LEDC channels in use.");
    return false;
  }
  clock_source = source;
  return true;
}

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
    uint8_t group = (handle->channel / SOC_LEDC_CHANNEL_NUM);
    remove_channel_from_timer(group, handle->timer_num, handle->channel % SOC_LEDC_CHANNEL_NUM);
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

  uint8_t group = (channel / SOC_LEDC_CHANNEL_NUM);
  uint8_t timer = 0;
  bool channel_used = ledc_handle.used_channels & (1UL << channel);

  if (channel_used) {
    log_i("Channel %u is already set up, given frequency and resolution will be ignored", channel);
    if (ledc_set_pin(pin, group, channel % SOC_LEDC_CHANNEL_NUM) != ESP_OK) {
      log_e("Attaching pin to already used channel failed!");
      return false;
    }
  } else {
    // Find a timer with matching frequency and resolution, or a free timer
    if (!find_matching_timer(group, freq, resolution, &timer)) {
      if (!find_free_timer(group, &timer)) {
        log_w("No free timers available for speed mode %u", group);
        return false;
      }

      // Configure the timer if we're using a new one
      ledc_timer_config_t ledc_timer;
      memset((void *)&ledc_timer, 0, sizeof(ledc_timer_config_t));
      ledc_timer.speed_mode = group;
      ledc_timer.timer_num = timer;
      ledc_timer.duty_resolution = resolution;
      ledc_timer.freq_hz = freq;
      ledc_timer.clk_cfg = clock_source;

      if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        log_e("ledc setup failed!");
        return false;
      }
    }

    uint32_t duty = ledc_get_duty(group, (channel % SOC_LEDC_CHANNEL_NUM));

    ledc_channel_config_t ledc_channel;
    memset((void *)&ledc_channel, 0, sizeof(ledc_channel_config_t));
    ledc_channel.speed_mode = group;
    ledc_channel.channel = (channel % SOC_LEDC_CHANNEL_NUM);
    ledc_channel.timer_sel = timer;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num = pin;
    ledc_channel.duty = duty;
    ledc_channel.hpoint = 0;

    ledc_channel_config(&ledc_channel);
  }

  ledc_channel_handle_t *handle = (ledc_channel_handle_t *)malloc(sizeof(ledc_channel_handle_t));
  handle->pin = pin;
  handle->channel = channel;
  handle->timer_num = timer;
  handle->freq_hz = freq;
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

  if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_LEDC, (void *)handle, channel, timer)) {
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

  // Try the first available channel
  if (ledcAttachChannel(pin, freq, resolution, channel)) {
    return true;
  }

#ifdef SOC_LEDC_SUPPORT_HS_MODE
  // If first attempt failed and HS mode is supported, try to find a free channel in group 1
  if ((channel / SOC_LEDC_CHANNEL_NUM) == 0) {  // First attempt was in group 0
    log_d("LEDC: Group 0 channel %u failed, trying to find a free channel in group 1", channel);
    // Find free channels specifically in group 1
    uint32_t group1_mask = ((1UL << SOC_LEDC_CHANNEL_NUM) - 1) << SOC_LEDC_CHANNEL_NUM;
    int group1_free_channel = (~ledc_handle.used_channels) & group1_mask;
    if (group1_free_channel != 0) {
      uint8_t group1_channel = __builtin_ctz(group1_free_channel);
      if (ledcAttachChannel(pin, freq, resolution, group1_channel)) {
        return true;
      }
    }
  }
#endif

  log_e(
    "No free timers available for freq=%u, resolution=%u. To attach a new channel, use the same frequency and resolution as an already attached channel to "
    "share its timer.",
    freq, resolution
  );
  return false;
}

bool ledcWrite(uint8_t pin, uint32_t duty) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {

    uint8_t group = (bus->channel / SOC_LEDC_CHANNEL_NUM), channel = (bus->channel % SOC_LEDC_CHANNEL_NUM);

    //Fixing if all bits in resolution is set = LEDC FULL ON
    uint32_t max_duty = (1 << bus->channel_resolution) - 1;

    if ((duty == max_duty) && (max_duty != 1)) {
      duty = max_duty + 1;
    }

    if (ledc_set_duty(group, channel, duty) != ESP_OK) {
      log_e("ledc_set_duty failed");
      return false;
    }
    if (ledc_update_duty(group, channel) != ESP_OK) {
      log_e("ledc_update_duty failed");
      return false;
    }

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
  uint8_t group = (channel / SOC_LEDC_CHANNEL_NUM);
  ledc_timer_t timer;

  // Get the actual timer being used by this channel
  ledc_ll_get_channel_timer(LEDC_LL_GET_HW(), group, (channel % SOC_LEDC_CHANNEL_NUM), &timer);

  //Fixing if all bits in resolution is set = LEDC FULL ON
  uint32_t resolution = 0;
  ledc_ll_get_duty_resolution(LEDC_LL_GET_HW(), group, timer, &resolution);

  uint32_t max_duty = (1 << resolution) - 1;

  if ((duty == max_duty) && (max_duty != 1)) {
    duty = max_duty + 1;
  }

  if (ledc_set_duty(group, channel, duty) != ESP_OK) {
    log_e("ledc_set_duty failed");
    return false;
  }
  if (ledc_update_duty(group, channel) != ESP_OK) {
    log_e("ledc_update_duty failed");
    return false;
  }

  return true;
}

uint32_t ledcRead(uint8_t pin) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {

    uint8_t group = (bus->channel / SOC_LEDC_CHANNEL_NUM), channel = (bus->channel % SOC_LEDC_CHANNEL_NUM);
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
    uint8_t group = (bus->channel / SOC_LEDC_CHANNEL_NUM);
    return ledc_get_freq(group, bus->timer_num);
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

    uint8_t group = (bus->channel / SOC_LEDC_CHANNEL_NUM);

    ledc_timer_config_t ledc_timer;
    memset((void *)&ledc_timer, 0, sizeof(ledc_timer_config_t));
    ledc_timer.speed_mode = group;
    ledc_timer.timer_num = bus->timer_num;
    ledc_timer.duty_resolution = 10;
    ledc_timer.freq_hz = freq;
    ledc_timer.clk_cfg = clock_source;

    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
      log_e("ledcWriteTone configuration failed!");
      return 0;
    }
    bus->channel_resolution = 10;

    uint32_t res_freq = ledc_get_freq(group, bus->timer_num);
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
    uint8_t group = (bus->channel / SOC_LEDC_CHANNEL_NUM);

    ledc_timer_config_t ledc_timer;
    memset((void *)&ledc_timer, 0, sizeof(ledc_timer_config_t));
    ledc_timer.speed_mode = group;
    ledc_timer.timer_num = bus->timer_num;
    ledc_timer.duty_resolution = resolution;
    ledc_timer.freq_hz = freq;
    ledc_timer.clk_cfg = clock_source;

    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
      log_e("ledcChangeFrequency failed!");
      return 0;
    }
    bus->channel_resolution = resolution;
    return ledc_get_freq(group, bus->timer_num);
  }
  return 0;
}

bool ledcOutputInvert(uint8_t pin, bool out_invert) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {
    gpio_set_level(pin, out_invert);

#ifdef CONFIG_IDF_TARGET_ESP32P4
    esp_rom_gpio_connect_out_signal(pin, LEDC_LS_SIG_OUT_PAD_OUT0_IDX + ((bus->channel) % SOC_LEDC_CHANNEL_NUM), out_invert, 0);
#else
#ifdef SOC_LEDC_SUPPORT_HS_MODE
    esp_rom_gpio_connect_out_signal(
      pin, ((bus->channel / SOC_LEDC_CHANNEL_NUM == 0) ? LEDC_HS_SIG_OUT0_IDX : LEDC_LS_SIG_OUT0_IDX) + ((bus->channel) % SOC_LEDC_CHANNEL_NUM), out_invert, 0
    );
#else
    esp_rom_gpio_connect_out_signal(pin, LEDC_LS_SIG_OUT0_IDX + ((bus->channel) % SOC_LEDC_CHANNEL_NUM), out_invert, 0);
#endif
#endif  // ifdef CONFIG_IDF_TARGET_ESP32P4
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
    uint8_t group = (bus->channel / SOC_LEDC_CHANNEL_NUM), channel = (bus->channel % SOC_LEDC_CHANNEL_NUM);

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

#ifdef SOC_LEDC_GAMMA_CURVE_FADE_SUPPORTED
// Default gamma factor for gamma correction (common value for LEDs)
static float ledcGammaFactor = 2.8;
// Gamma correction LUT support
static const float *ledcGammaLUT = NULL;
static uint16_t ledcGammaLUTSize = 0;
// Global variable to store current resolution for gamma callback
static uint8_t ledcGammaResolution = 13;

bool ledcSetGammaTable(const float *gamma_table, uint16_t size) {
  if (gamma_table == NULL || size == 0) {
    log_e("Invalid gamma table or size");
    return false;
  }
  ledcGammaLUT = gamma_table;
  ledcGammaLUTSize = size;
  log_i("Custom gamma LUT set with %u entries", size);
  return true;
}

void ledcClearGammaTable(void) {
  ledcGammaLUT = NULL;
  ledcGammaLUTSize = 0;
  log_i("Gamma LUT cleared, using mathematical calculation");
}

void ledcSetGammaFactor(float factor) {
  ledcGammaFactor = factor;
}

// Gamma correction calculator function
static uint32_t ledcGammaCorrection(uint32_t duty) {
  if (duty == 0) {
    return 0;
  }

  uint32_t max_duty = (1U << ledcGammaResolution) - 1;
  if (duty >= (1U << ledcGammaResolution)) {
    return max_duty;
  }

  // Use LUT if provided, otherwise use mathematical calculation
  if (ledcGammaLUT != NULL && ledcGammaLUTSize > 0) {
    // LUT-based gamma correction
    uint32_t lut_index = (duty * (ledcGammaLUTSize - 1)) / max_duty;
    if (lut_index >= ledcGammaLUTSize) {
      lut_index = ledcGammaLUTSize - 1;
    }

    float corrected_normalized = ledcGammaLUT[lut_index];
    return (uint32_t)(corrected_normalized * max_duty);
  } else {
    // Mathematical gamma correction
    double normalized = (double)duty / (1U << ledcGammaResolution);
    double corrected = pow(normalized, ledcGammaFactor);
    return (uint32_t)(corrected * (1U << ledcGammaResolution));
  }
}

static bool ledcFadeGammaConfig(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void *), void *arg) {
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
    uint8_t group = (bus->channel / SOC_LEDC_CHANNEL_NUM), channel = (bus->channel % SOC_LEDC_CHANNEL_NUM);

    // Initialize fade service.
    if (!fade_initialized) {
      ledc_fade_func_install(0);
      fade_initialized = true;
    }

    bus->fn = (voidFuncPtr)userFunc;
    bus->arg = arg;

    ledc_cbs_t callbacks = {.fade_cb = ledcFnWrapper};
    ledc_cb_register(group, channel, &callbacks, (void *)bus);

    // Prepare gamma curve fade parameters
    ledc_fade_param_config_t fade_params[SOC_LEDC_GAMMA_CURVE_FADE_RANGE_MAX];
    uint32_t actual_fade_ranges = 0;

    // Use a moderate number of linear segments for smooth gamma curve
    const uint32_t linear_fade_segments = 12;

    // Set the global resolution for gamma correction
    ledcGammaResolution = bus->channel_resolution;

    // Fill multi-fade parameter list using ESP-IDF API
    esp_err_t err = ledc_fill_multi_fade_param_list(
      group, channel, start_duty, target_duty, linear_fade_segments, max_fade_time_ms, ledcGammaCorrection, SOC_LEDC_GAMMA_CURVE_FADE_RANGE_MAX, fade_params,
      &actual_fade_ranges
    );

    if (err != ESP_OK) {
      log_e("ledc_fill_multi_fade_param_list failed: %s", esp_err_to_name(err));
      return false;
    }

    // Apply the gamma-corrected start duty
    uint32_t gamma_start_duty = ledcGammaCorrection(start_duty);

    // Set multi-fade parameters
    err = ledc_set_multi_fade(group, channel, gamma_start_duty, fade_params, actual_fade_ranges);
    if (err != ESP_OK) {
      log_e("ledc_set_multi_fade failed: %s", esp_err_to_name(err));
      return false;
    }

    // Start the gamma curve fade
    err = ledc_fade_start(group, channel, LEDC_FADE_NO_WAIT);
    if (err != ESP_OK) {
      log_e("ledc_fade_start failed: %s", esp_err_to_name(err));
      return false;
    }

    log_d("Gamma curve fade started on pin %u: %u -> %u over %dms", pin, start_duty, target_duty, max_fade_time_ms);

  } else {
    log_e("Pin %u is not attached to LEDC. Call ledcAttach first!", pin);
    return false;
  }
  return true;
}

bool ledcFadeGamma(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms) {
  return ledcFadeGammaConfig(pin, start_duty, target_duty, max_fade_time_ms, NULL, NULL);
}

bool ledcFadeGammaWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, voidFuncPtr userFunc) {
  return ledcFadeGammaConfig(pin, start_duty, target_duty, max_fade_time_ms, (voidFuncPtrArg)userFunc, NULL);
}

bool ledcFadeGammaWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void *), void *arg) {
  return ledcFadeGammaConfig(pin, start_duty, target_duty, max_fade_time_ms, userFunc, arg);
}

#endif /* SOC_LEDC_GAMMA_CURVE_FADE_SUPPORTED */

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
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {  // if pin is attached to LEDC change frequency, otherwise update the global frequency
    if (ledcChangeFrequency(pin, freq, analog_resolution) == 0) {
      log_e("analogWrite frequency cant be set due to selected resolution! Try to adjust resolution first");
      return;
    }
  }
  analog_frequency = freq;
}

void analogWriteResolution(uint8_t pin, uint8_t resolution) {
  ledc_channel_handle_t *bus = (ledc_channel_handle_t *)perimanGetPinBus(pin, ESP32_BUS_TYPE_LEDC);
  if (bus != NULL) {  // if pin is attached to LEDC change resolution, otherwise update the global resolution
    if (ledcChangeFrequency(pin, analog_frequency, resolution) == 0) {
      log_e("analogWrite resolution cant be set due to selected frequency! Try to adjust frequency first");
      return;
    }
  }
  analog_resolution = resolution;
}

#endif /* SOC_LEDC_SUPPORTED */
