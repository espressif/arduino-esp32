// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#include "esp32-hal-timer.h"

#if SOC_GPTIMER_SUPPORTED
#include "driver/gptimer.h"
#if defined __has_include && __has_include("clk_tree.h")
#include "clk_tree.h"
#else
#include "esp_clk_tree.h"
#endif

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);

typedef struct {
  voidFuncPtr fn;
  void *arg;
} interrupt_config_t;

struct timer_struct_t {
  gptimer_handle_t timer_handle;
  interrupt_config_t interrupt_handle;
  bool timer_started;
};

inline uint64_t timerRead(hw_timer_t *timer) {

  uint64_t value;
  gptimer_get_raw_count(timer->timer_handle, &value);
  return value;
}

void timerWrite(hw_timer_t *timer, uint64_t val) {
  gptimer_set_raw_count(timer->timer_handle, val);
}

void timerAlarm(hw_timer_t *timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count) {
  esp_err_t err = ESP_OK;
  gptimer_alarm_config_t alarm_cfg = {
    .alarm_count = alarm_value,
    .reload_count = reload_count,
    .flags.auto_reload_on_alarm = autoreload,
  };
  err = gptimer_set_alarm_action(timer->timer_handle, &alarm_cfg);
  if (err != ESP_OK) {
    log_e("Timer Alarm Write failed, error num=%d", err);
  }
}

uint32_t timerGetFrequency(hw_timer_t *timer) {
  uint32_t frequency;
  gptimer_get_resolution(timer->timer_handle, &frequency);
  return frequency;
}

void timerStart(hw_timer_t *timer) {
  gptimer_start(timer->timer_handle);
  timer->timer_started = true;
}

void timerStop(hw_timer_t *timer) {
  gptimer_stop(timer->timer_handle);
  timer->timer_started = false;
}

void timerRestart(hw_timer_t *timer) {
  gptimer_set_raw_count(timer->timer_handle, 0);
}

hw_timer_t *timerBegin(uint32_t frequency) {
  esp_err_t err = ESP_OK;
  uint32_t counter_src_hz = 0;
  uint32_t divider = 0;
  soc_periph_gptimer_clk_src_t clk;

  soc_periph_gptimer_clk_src_t gptimer_clks[] = SOC_GPTIMER_CLKS;
  for (size_t i = 0; i < sizeof(gptimer_clks) / sizeof(gptimer_clks[0]); i++) {
    clk = gptimer_clks[i];
#if defined __has_include && __has_include("clk_tree.h")
    clk_tree_src_get_freq_hz(clk, CLK_TREE_SRC_FREQ_PRECISION_CACHED, &counter_src_hz);
#else
    esp_clk_tree_src_get_freq_hz(clk, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &counter_src_hz);
#endif
    divider = counter_src_hz / frequency;
    if ((divider >= 2) && (divider <= 65536)) {
      break;
    } else divider = 0;
  }

  if (divider == 0) {
    log_e("Resolution cannot be reached with any clock source, aborting!");
    return NULL;
  }

  gptimer_config_t config = {
    .clk_src = clk,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = frequency,
    .flags.intr_shared = true,
  };

  hw_timer_t *timer = malloc(sizeof(hw_timer_t));

  err = gptimer_new_timer(&config, &timer->timer_handle);
  if (err != ESP_OK) {
    log_e("Failed to create a new GPTimer, error num=%d", err);
    free(timer);
    return NULL;
  }
  gptimer_enable(timer->timer_handle);
  gptimer_start(timer->timer_handle);
  timer->timer_started = true;
  return timer;
}

void timerEnd(hw_timer_t *timer) {
  esp_err_t err = ESP_OK;
  if (timer->timer_started == true) {
    gptimer_stop(timer->timer_handle);
  }
  gptimer_disable(timer->timer_handle);
  err = gptimer_del_timer(timer->timer_handle);
  if (err != ESP_OK) {
    log_e("Failed to destroy GPTimer, error num=%d", err);
    return;
  }
  free(timer);
}

bool IRAM_ATTR timerFnWrapper(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *args) {
  interrupt_config_t *isr = (interrupt_config_t *)args;
  if (isr->fn) {
    if (isr->arg) {
      ((voidFuncPtrArg)isr->fn)(isr->arg);
    } else {
      isr->fn();
    }
  }
  // some additional logic or handling may be required here to appropriately yield or not
  return false;
}

void timerAttachInterruptFunctionalArg(hw_timer_t *timer, void (*userFunc)(void *), void *arg) {
  esp_err_t err = ESP_OK;
  gptimer_event_callbacks_t cbs = {
    .on_alarm = timerFnWrapper,
  };

  timer->interrupt_handle.fn = (voidFuncPtr)userFunc;
  timer->interrupt_handle.arg = arg;

  if (timer->timer_started == true) {
    gptimer_stop(timer->timer_handle);
  }
  gptimer_disable(timer->timer_handle);
  err = gptimer_register_event_callbacks(timer->timer_handle, &cbs, &timer->interrupt_handle);
  if (err != ESP_OK) {
    log_e("Timer Attach Interrupt failed, error num=%d", err);
  }
  gptimer_enable(timer->timer_handle);
  if (timer->timer_started == true) {
    gptimer_start(timer->timer_handle);
  }
}

void timerAttachInterruptArg(hw_timer_t *timer, void (*userFunc)(void *), void *arg) {
  timerAttachInterruptFunctionalArg(timer, userFunc, arg);
}

void timerAttachInterrupt(hw_timer_t *timer, voidFuncPtr userFunc) {
  timerAttachInterruptFunctionalArg(timer, (voidFuncPtrArg)userFunc, NULL);
}

void timerDetachInterrupt(hw_timer_t *timer) {
  esp_err_t err = ESP_OK;
  err = gptimer_set_alarm_action(timer->timer_handle, NULL);
  timer->interrupt_handle.fn = NULL;
  timer->interrupt_handle.arg = NULL;
  if (err != ESP_OK) {
    log_e("Timer Detach Interrupt failed, error num=%d", err);
  }
}

uint64_t timerReadMicros(hw_timer_t *timer) {
  uint64_t timer_val = timerRead(timer);
  uint32_t frequency = timerGetFrequency(timer);
  return timer_val * 1000000 / frequency;
}

uint64_t timerReadMilis(hw_timer_t *timer) {
  uint64_t timer_val = timerRead(timer);
  uint32_t frequency = timerGetFrequency(timer);
  return timer_val * 1000 / frequency;
}

double timerReadSeconds(hw_timer_t *timer) {
  uint64_t timer_val = timerRead(timer);
  uint32_t frequency = timerGetFrequency(timer);
  return (double)timer_val / frequency;
}

#endif /* SOC_GPTIMER_SUPPORTED */
