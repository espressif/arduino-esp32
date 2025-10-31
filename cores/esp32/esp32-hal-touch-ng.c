// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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
#include "esp_idf_version.h"

#if SOC_TOUCH_SENSOR_SUPPORTED
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) || SOC_TOUCH_SENSOR_VERSION == 3

#include "esp32-hal-touch-ng.h"
#include "esp32-hal-periman.h"

/*
    Internal Private Touch Data Structure and Functions
*/

typedef void (*voidFuncPtr)(void);
typedef void (*voidArgFuncPtr)(void *);

typedef struct {
  voidFuncPtr fn;
  bool callWithArgs;
  void *arg;
  bool lastStatusIsPressed;
} TouchInterruptHandle_t;

static TouchInterruptHandle_t __touchInterruptHandlers[SOC_TOUCH_SENSOR_NUM] = {
  0,
};
#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
static uint8_t _sample_num = 1;    // only one sample configuration supported
static float _duration_ms = 5.0f;
static touch_volt_lim_l_t _volt_low = TOUCH_VOLT_LIM_L_0V5;
static touch_volt_lim_h_t _volt_high = TOUCH_VOLT_LIM_H_1V7;
static touch_intr_trig_mode_t _intr_trig_mode = TOUCH_INTR_TRIG_ON_BELOW_THRESH;
#elif SOC_TOUCH_SENSOR_VERSION == 2  // ESP32S2, ESP32S3
static uint8_t _sample_num = 1;  // only one sample configuration supported
static uint32_t _chg_times = 500;
static touch_volt_lim_l_t _volt_low = TOUCH_VOLT_LIM_L_0V5;
static touch_volt_lim_h_t _volt_high = TOUCH_VOLT_LIM_H_2V2;
#elif SOC_TOUCH_SENSOR_VERSION == 3  // ESP32P4
static uint8_t _sample_num = 1;  // TODO: can be extended to multiple samples
static uint32_t _div_num = 1;
static uint8_t _coarse_freq_tune = 1;
static uint8_t _fine_freq_tune = 1;
#endif

static uint8_t used_pads = 0;

static uint32_t __touchSleepTime = 256;
static float __touchMeasureTime = 32.0f;

static touch_sensor_config_t sensor_config;

static bool initialized = false;
static bool enabled = false;
static bool running = false;
static bool channels_initialized[SOC_TOUCH_SENSOR_NUM] = {false};

static touch_sensor_handle_t touch_sensor_handle = NULL;
static touch_channel_handle_t touch_channel_handle[SOC_TOUCH_SENSOR_NUM] = {};

// Active threshold to benchmark ratio. (i.e., touch will be activated when data >= benchmark * (1 + ratio))
static float s_thresh2bm_ratio = 0.015f;  // 1.5% for all channels

static bool ARDUINO_ISR_ATTR __touchOnActiveISR(touch_sensor_handle_t sens_handle, const touch_active_event_data_t *event, void *user_ctx) {
  uint8_t pad_num = (uint8_t)event->chan_id;
  __touchInterruptHandlers[pad_num].lastStatusIsPressed = true;
  if (__touchInterruptHandlers[pad_num].fn) {
    // keeping backward compatibility with "void cb(void)" and with new "void cb(void *)"
    if (__touchInterruptHandlers[pad_num].callWithArgs) {
      ((voidArgFuncPtr)__touchInterruptHandlers[pad_num].fn)(__touchInterruptHandlers[pad_num].arg);
    } else {
      __touchInterruptHandlers[pad_num].fn();
    }
  }
  return false;
}

static bool ARDUINO_ISR_ATTR __touchOnInactiveISR(touch_sensor_handle_t sens_handle, const touch_inactive_event_data_t *event, void *user_ctx) {
  uint8_t pad_num = (uint8_t)event->chan_id;
  __touchInterruptHandlers[pad_num].lastStatusIsPressed = false;
  if (__touchInterruptHandlers[pad_num].fn) {
    // keeping backward compatibility with "void cb(void)" and with new "void cb(void *)"
    if (__touchInterruptHandlers[pad_num].callWithArgs) {
      ((voidArgFuncPtr)__touchInterruptHandlers[pad_num].fn)(__touchInterruptHandlers[pad_num].arg);
    } else {
      __touchInterruptHandlers[pad_num].fn();
    }
  }
  return false;
}

bool touchStop() {
  if (!running) {  // Already stopped
    return true;
  }
  if (touch_sensor_stop_continuous_scanning(touch_sensor_handle) != ESP_OK) {
    log_e("Touch sensor stop scanning failed!");
    return false;
  }
  running = false;
  return true;
}

bool touchDisable() {
  if (!enabled) {  // Already disabled
    return true;
  }
  if (running) {
    log_e("Touch sensor still running!");
    return false;
  }
  if (touch_sensor_disable(touch_sensor_handle) != ESP_OK) {
    log_e("Touch sensor disable failed!");
    return false;
  }
  enabled = false;
  return true;
}

bool touchStart() {
  if (running) {  // Already running
    return true;
  }
  if (!enabled) {
    log_e("Touch sensor not enabled!");
    return false;
  }
  if (touch_sensor_start_continuous_scanning(touch_sensor_handle) != ESP_OK) {
    log_e("Touch sensor failed to start continuous scanning!");
    return false;
  }
  running = true;
  return true;
}

bool touchEnable() {
  if (enabled) {  // Already enabled
    return true;
  }
  if (touch_sensor_enable(touch_sensor_handle) != ESP_OK) {
    log_e("Touch sensor enable failed!");
    return false;
  }
  enabled = true;
  return true;
}

bool touchBenchmarkThreshold(uint8_t pad) {
  if (!touchEnable()) {
    return false;
  }

  /* Scan the enabled touch channels for several times, to make sure the initial channel data is stable */
  for (int i = 0; i < 3; i++) {
    if (touch_sensor_trigger_oneshot_scanning(touch_sensor_handle, 2000) != ESP_OK) {
      log_e("Touch sensor trigger oneshot scanning failed!");
      return false;
    }
  }

  /* Disable the touch channel to rollback the state */
  if (!touchDisable()) {
    return false;
  }

  // Reconfigure passed pad with new threshold
  uint32_t benchmark[_sample_num] = {};
#if SOC_TOUCH_SUPPORT_BENCHMARK  // ESP32S2, ESP32S3,ESP32P4
  if (touch_channel_read_data(touch_channel_handle[pad], TOUCH_CHAN_DATA_TYPE_BENCHMARK, benchmark) != ESP_OK) {
    log_e("Touch channel read data failed!");
    return false;
  }
#else
  if (touch_channel_read_data(touch_channel_handle[pad], TOUCH_CHAN_DATA_TYPE_SMOOTH, benchmark) != ESP_OK) {
    log_e("Touch channel read data failed!");
    return false;
  }
#endif

  /* Calculate the proper active thresholds regarding the initial benchmark */
  touch_channel_config_t chan_cfg = TOUCH_CHANNEL_DEFAULT_CONFIG();
  for (int i = 0; i < _sample_num; i++) {
#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
    chan_cfg.abs_active_thresh[i] = (uint32_t)(benchmark[i] * (1 - s_thresh2bm_ratio));
    log_v("Configured [CH %d] sample %d: benchmark = %" PRIu32 ", threshold = %" PRIu32 "\t", pad, i, benchmark[i], chan_cfg.abs_active_thresh[i]);
#else
    chan_cfg.active_thresh[i] = (uint32_t)(benchmark[i] * s_thresh2bm_ratio);
    log_v("Configured [CH %d] sample %d: benchmark = %" PRIu32 ", threshold = %" PRIu32 "\t", pad, i, benchmark[i], chan_cfg.active_thresh[i]);
#endif
  }
  /* Update the channel configuration */
  if (touch_sensor_reconfig_channel(touch_channel_handle[pad], &chan_cfg) != ESP_OK) {
    log_e("Touch sensor threshold reconfig channel failed!");
    return false;
  }
  return true;
}

static bool touchDetachBus(void *pin) {
  int8_t pad = digitalPinToTouchChannel((int)(pin - 1));
  channels_initialized[pad] = false;
  //disable touch pad and delete the channel
  if (!touchStop()) {
    log_e("touchStop() failed!");
    return false;
  }
  if (!touchDisable()) {
    log_e("touchDisable() failed!");
    return false;
  }
  touch_sensor_del_channel(touch_channel_handle[pad]);
  used_pads--;
  if (used_pads == 0) {
    if (touch_sensor_del_controller(touch_sensor_handle) != ESP_OK)  //deinit touch module, as no pads are used
    {
      log_e("Touch module deinit failed!");
      return false;
    }
    initialized = false;
  } else {
    touchEnable();
    touchStart();
  }
  return true;
}

static void __touchInit() {
  if (initialized) {
    return;
  }
  // Support only one sample configuration for now
#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
  touch_sensor_sample_config_t single_sample_cfg = TOUCH_SENSOR_V1_DEFAULT_SAMPLE_CONFIG(_duration_ms, _volt_low, _volt_high);
#elif SOC_TOUCH_SENSOR_VERSION == 2  // ESP32S2, ESP32S3
  touch_sensor_sample_config_t single_sample_cfg = TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(_chg_times, _volt_low, _volt_high);
#elif SOC_TOUCH_SENSOR_VERSION == 3  // ESP32P4
  touch_sensor_sample_config_t single_sample_cfg = TOUCH_SENSOR_V3_DEFAULT_SAMPLE_CONFIG(_div_num, _coarse_freq_tune, _fine_freq_tune);
#endif
  touch_sensor_sample_config_t sample_cfg[_sample_num] = {};
  sample_cfg[0] = single_sample_cfg;

  touch_sensor_config_t sens_cfg = {
#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
    .power_on_wait_us = __touchSleepTime,
    .meas_interval_us = __touchMeasureTime,
    .intr_trig_mode = _intr_trig_mode,
    .intr_trig_group = TOUCH_INTR_TRIG_GROUP_BOTH,
    .sample_cfg_num = _sample_num,
    .sample_cfg = sample_cfg,
#elif SOC_TOUCH_SENSOR_VERSION == 2  // ESP32S2, ESP32S3
    .power_on_wait_us = __touchSleepTime,
    .meas_interval_us = __touchMeasureTime,
    .max_meas_time_us = 0,
    .sample_cfg_num = _sample_num,
    .sample_cfg = sample_cfg,
#elif SOC_TOUCH_SENSOR_VERSION == 3  // ESP32P4
    .power_on_wait_us = __touchSleepTime,
    .meas_interval_us = __touchMeasureTime,
    .max_meas_time_us = 0,
    .output_mode = TOUCH_PAD_OUT_AS_CLOCK,
    .sample_cfg_num = _sample_num,
    .sample_cfg = sample_cfg,
#endif
  };

  if (touch_sensor_new_controller(&sens_cfg, &touch_sensor_handle) != ESP_OK) {
    goto err;
  }

  sensor_config = sens_cfg;
  /* Configure the touch sensor filter */
  touch_sensor_filter_config_t filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
  if (touch_sensor_config_filter(touch_sensor_handle, &filter_cfg) != ESP_OK) {
    goto err;
  }

  /* Register the touch sensor on_active and on_inactive callbacks */
  touch_event_callbacks_t callbacks = {0};
  callbacks.on_active = __touchOnActiveISR;
  callbacks.on_inactive = __touchOnInactiveISR;

  if (touch_sensor_register_callbacks(touch_sensor_handle, &callbacks, NULL) != ESP_OK) {
    goto err;
  }

  initialized = true;
  return;
err:
  log_e(" Touch sensor initialization error.");
  initialized = false;
  return;
}

static void __touchChannelInit(int pad) {
  if (channels_initialized[pad]) {
    return;
  }

  // Initial setup with default Threshold
  __touchInterruptHandlers[pad].fn = NULL;

  touch_channel_config_t chan_cfg = TOUCH_CHANNEL_DEFAULT_CONFIG();

  if (!touchStop() || !touchDisable()) {
    log_e("Touch sensor stop and disable failed!");
    return;
  }

  if (touch_sensor_new_channel(touch_sensor_handle, pad, &chan_cfg, &touch_channel_handle[pad]) != ESP_OK) {
    log_e("Touch sensor new channel failed!");
    return;
  }

  // Benchmark active threshold and reconfigure pad
  if (!touchBenchmarkThreshold(pad)) {
    log_e("Touch sensor benchmark threshold failed!");
    return;
  }

  channels_initialized[pad] = true;
  used_pads++;

  if (!touchEnable() || !touchStart()) {
    log_e("Touch sensor enable and start failed!");
  }
}

static touch_value_t __touchRead(uint8_t pin) {
  int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0) {
    return 0;
  }

  if (perimanGetPinBus(pin, ESP32_BUS_TYPE_TOUCH) == NULL) {
    perimanSetBusDeinit(ESP32_BUS_TYPE_TOUCH, touchDetachBus);
    if (!perimanClearPinBus(pin)) {
      return 0;
    }
    __touchInit();
    __touchChannelInit(pad);

    if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_TOUCH, (void *)(pin + 1), -1, pad)) {
      touchDetachBus((void *)(pin + 1));
      return 0;
    }
  }

  uint32_t touch_read[_sample_num] = {};
  touch_channel_read_data(touch_channel_handle[pad], TOUCH_CHAN_DATA_TYPE_SMOOTH, touch_read);
  touch_value_t touch_value = touch_read[0];  // only one sample configuration for now

  return touch_value;
}

static void __touchConfigInterrupt(uint8_t pin, void (*userFunc)(void), void *Args, bool callWithArgs, touch_value_t threshold) {
  int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0) {
    return;
  }

  if (userFunc == NULL) {
    // detach ISR User Call
    __touchInterruptHandlers[pad].fn = NULL;
    __touchInterruptHandlers[pad].callWithArgs = false;
    __touchInterruptHandlers[pad].arg = NULL;
  } else {
    // attach ISR User Call
    if (perimanGetPinBus(pin, ESP32_BUS_TYPE_TOUCH) == NULL) {
      perimanSetBusDeinit(ESP32_BUS_TYPE_TOUCH, touchDetachBus);
      if (!perimanClearPinBus(pin)) {
        log_e("Failed to clear pin bus");
        return;
      }
      __touchInit();
      __touchChannelInit(pad);

      if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_TOUCH, (void *)(pin + 1), -1, pad)) {
        touchDetachBus((void *)(pin + 1));
        log_e("Failed to set bus to Peripheral manager");
        return;
      }
    }
    __touchInterruptHandlers[pad].fn = userFunc;
    __touchInterruptHandlers[pad].callWithArgs = callWithArgs;
    __touchInterruptHandlers[pad].arg = Args;
  }

  if (threshold != 0) {
    if (!touchStop() || !touchDisable()) {
      log_e("Touch sensor stop and disable failed!");
      return;
    }

    touch_channel_config_t chan_cfg = TOUCH_CHANNEL_DEFAULT_CONFIG();
    for (int i = 0; i < _sample_num; i++) {
#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
      chan_cfg.abs_active_thresh[i] = threshold;
#else
      chan_cfg.active_thresh[i] = threshold;
#endif
    }

    if (touch_sensor_reconfig_channel(touch_channel_handle[pad], &chan_cfg) != ESP_OK) {
      log_e("Touch sensor threshold reconfig channel failed!");
    }

    if (!touchEnable() || !touchStart()) {
      log_e("Touch sensor enable and start failed!");
    }
  }
}

// it keeps backwards compatibility
static void __touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), touch_value_t threshold) {
  __touchConfigInterrupt(pin, userFunc, NULL, false, threshold);
}

// new additional version of the API with User Args
static void __touchAttachArgsInterrupt(uint8_t pin, void (*userFunc)(void), void *args, touch_value_t threshold) {
  __touchConfigInterrupt(pin, userFunc, args, true, threshold);
}

// new additional API to detach touch ISR
static void __touchDettachInterrupt(uint8_t pin) {
  __touchConfigInterrupt(pin, NULL, NULL, false, 0);  // userFunc as NULL acts as detaching
}

// /*
//     External Public Touch API Functions
// */

bool touchInterruptGetLastStatus(uint8_t pin) {
  int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0) {
    return false;
  }
  return __touchInterruptHandlers[pad].lastStatusIsPressed;
}

void touchSleepWakeUpEnable(uint8_t pin, touch_value_t threshold) {
  int8_t pad = digitalPinToTouchChannel(pin);
  if (pad < 0) {
    return;
  }

  if (perimanGetPinBus(pin, ESP32_BUS_TYPE_TOUCH) == NULL) {
    perimanSetBusDeinit(ESP32_BUS_TYPE_TOUCH, touchDetachBus);
    __touchInit();
    __touchChannelInit(pad);
    if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_TOUCH, (void *)(pin + 1), -1, pad)) {
      log_e("Failed to set bus to Peripheral manager");
      touchDetachBus((void *)(pin + 1));
      return;
    }
  }

  log_v("Touch sensor deep sleep wake-up configuration for pad %d with threshold %d", pad, threshold);
  if (!touchStop() || !touchDisable()) {
    log_e("Touch sensor stop and disable failed!");
    return;
  }

  touch_sleep_config_t deep_slp_cfg = {
    .slp_wakeup_lvl = TOUCH_DEEP_SLEEP_WAKEUP,
#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
    .deep_slp_sens_cfg = NULL,     // Use the original touch sensor configuration
#else                              // SOC_TOUCH_SENSOR_VERSION 2 and 3// ESP32S2, ESP32S3, ESP32P4
    .deep_slp_chan = touch_channel_handle[pad],
    .deep_slp_thresh = {threshold},
    .deep_slp_sens_cfg = NULL,  // Use the original touch sensor configuration
#endif
  };

  // Register the deep sleep wake-up
  if (touch_sensor_config_sleep_wakeup(touch_sensor_handle, &deep_slp_cfg) != ESP_OK) {
    log_e("Touch sensor deep sleep wake-up failed!");
    return;
  }

  if (!touchEnable() || !touchStart()) {
    log_e("Touch sensor enable and start failed!");
  }
}

void touchSetDefaultThreshold(float percentage) {
  s_thresh2bm_ratio = (float)percentage / 100.0f;
}

void touchSetTiming(float measure, uint32_t sleep) {
  if (initialized) {
    log_e("Touch sensor already initialized. Cannot set cycles.");
    return;
  }
  __touchSleepTime = sleep;
  __touchMeasureTime = measure;
}

#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
void touchSetConfig(float duration_ms, touch_volt_lim_l_t volt_low, touch_volt_lim_h_t volt_high) {
  if (initialized) {
    log_e("Touch sensor already initialized. Cannot set configuration.");
    return;
  }
  _duration_ms = duration_ms;
  _volt_low = volt_low;
  _volt_high = volt_high;
}

#elif SOC_TOUCH_SENSOR_VERSION == 2  // ESP32S2, ESP32S3
void touchSetConfig(uint32_t chg_times, touch_volt_lim_l_t volt_low, touch_volt_lim_h_t volt_high) {
  if (initialized) {
    log_e("Touch sensor already initialized. Cannot set configuration.");
    return;
  }
  _chg_times = chg_times;
  _volt_low = volt_low;
  _volt_high = volt_high;
}

#elif SOC_TOUCH_SENSOR_VERSION == 3  // ESP32P4
void touchSetConfig(uint32_t div_num, uint8_t coarse_freq_tune, uint8_t fine_freq_tune) {
  if (initialized) {
    log_e("Touch sensor already initialized. Cannot set configuration.");
    return;
  }
  _div_num = div_num;
  _coarse_freq_tune = coarse_freq_tune;
  _fine_freq_tune = fine_freq_tune;
}
#endif

#if SOC_TOUCH_SENSOR_VERSION == 1  // ESP32
void touchInterruptSetThresholdDirection(bool mustbeLower) {
  if (mustbeLower) {
    _intr_trig_mode = TOUCH_INTR_TRIG_ON_BELOW_THRESH;
  } else {
    _intr_trig_mode = TOUCH_INTR_TRIG_ON_ABOVE_THRESH;
  }
}
#endif

extern touch_value_t touchRead(uint8_t) __attribute__((weak, alias("__touchRead")));
extern void touchAttachInterrupt(uint8_t, voidFuncPtr, touch_value_t) __attribute__((weak, alias("__touchAttachInterrupt")));
extern void touchAttachInterruptArg(uint8_t, voidArgFuncPtr, void *, touch_value_t) __attribute__((weak, alias("__touchAttachArgsInterrupt")));
extern void touchDetachInterrupt(uint8_t) __attribute__((weak, alias("__touchDettachInterrupt")));

#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0) || SOC_TOUCH_SENSOR_VERSION == 3 */
#endif /* SOC_TOUCH_SENSOR_SUPPORTED */
