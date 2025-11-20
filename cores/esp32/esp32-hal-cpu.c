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

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "soc/rtc.h"
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
#include "soc/rtc_cntl_reg.h"
#include "soc/syscon_reg.h"
#endif
#include "soc/efuse_reg.h"
#include "esp32-hal.h"
#include "esp32-hal-cpu.h"
#include "hal/timer_ll.h"
#include "esp_private/systimer.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR  // IDF 4+
#if CONFIG_IDF_TARGET_ESP32   // ESP32/PICO-D4
#include "xtensa_timer.h"
#include "esp32/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "PLL", "8.5M", "APLL" };
#elif CONFIG_IDF_TARGET_ESP32S2
#include "xtensa_timer.h"
#include "esp32s2/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "PLL", "8.5M", "APLL" };
#elif CONFIG_IDF_TARGET_ESP32S3
#include "xtensa_timer.h"
#include "esp32s3/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "PLL", "17.5M" };
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "PLL", "17.5M" };
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "PLL", "17.5M" };
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "PLL", "17.5M" };
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "PLL", "8.5M", "FLASH_PLL" };
#elif CONFIG_IDF_TARGET_ESP32P4
#include "esp32p4/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "CPLL", "17.5M" };
#elif CONFIG_IDF_TARGET_ESP32C5
#include "esp32c5/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "17.5M", "PLL_F160M", "PLL_F240M" };
#elif CONFIG_IDF_TARGET_ESP32C61
#include "esp32c61/rom/rtc.h"
static const char *clock_source_names[] = { "XTAL", "17.5M", "PLL_F160M" };
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else  // ESP32 Before IDF 4.0
#include "rom/rtc.h"
#endif

typedef struct apb_change_cb_s {
  struct apb_change_cb_s *prev;
  struct apb_change_cb_s *next;
  void *arg;
  apb_change_cb_t cb;
} apb_change_t;

static apb_change_t *apb_change_callbacks = NULL;
static SemaphoreHandle_t apb_change_lock = NULL;

static void initApbChangeCallback() {
  static volatile bool initialized = false;
  if (!initialized) {
    initialized = true;
    apb_change_lock = xSemaphoreCreateMutex();
    if (!apb_change_lock) {
      initialized = false;
    }
  }
}

static void triggerApbChangeCallback(apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb) {
  initApbChangeCallback();
  xSemaphoreTake(apb_change_lock, portMAX_DELAY);
  apb_change_t *r = apb_change_callbacks;
  if (r != NULL) {
    if (ev_type == APB_BEFORE_CHANGE) {
      while (r != NULL) {
        r->cb(r->arg, ev_type, old_apb, new_apb);
        r = r->next;
      }
    } else {  // run backwards through chain
      while (r->next != NULL) {
        r = r->next;  // find first added
      }
      while (r != NULL) {
        r->cb(r->arg, ev_type, old_apb, new_apb);
        r = r->prev;
      }
    }
  }
  xSemaphoreGive(apb_change_lock);
}

bool addApbChangeCallback(void *arg, apb_change_cb_t cb) {
  initApbChangeCallback();
  apb_change_t *c = (apb_change_t *)malloc(sizeof(apb_change_t));
  if (!c) {
    log_e("Callback Object Malloc Failed");
    return false;
  }
  c->next = NULL;
  c->prev = NULL;
  c->arg = arg;
  c->cb = cb;
  xSemaphoreTake(apb_change_lock, portMAX_DELAY);
  if (apb_change_callbacks == NULL) {
    apb_change_callbacks = c;
  } else {
    apb_change_t *r = apb_change_callbacks;
    // look for duplicate callbacks
    while ((r != NULL) && !((r->cb == cb) && (r->arg == arg))) {
      r = r->next;
    }
    if (r) {
      log_e("duplicate func=%8p arg=%8p", c->cb, c->arg);
      free(c);
      xSemaphoreGive(apb_change_lock);
      return false;
    } else {
      c->next = apb_change_callbacks;
      apb_change_callbacks->prev = c;
      apb_change_callbacks = c;
    }
  }
  xSemaphoreGive(apb_change_lock);
  return true;
}

bool removeApbChangeCallback(void *arg, apb_change_cb_t cb) {
  initApbChangeCallback();
  xSemaphoreTake(apb_change_lock, portMAX_DELAY);
  apb_change_t *r = apb_change_callbacks;
  // look for matching callback
  while ((r != NULL) && !((r->cb == cb) && (r->arg == arg))) {
    r = r->next;
  }
  if (r == NULL) {
    log_e("not found func=%8p arg=%8p", cb, arg);
    xSemaphoreGive(apb_change_lock);
    return false;
  } else {
    // patch links
    if (r->prev) {
      r->prev->next = r->next;
    } else {  // this is first link
      apb_change_callbacks = r->next;
    }
    if (r->next) {
      r->next->prev = r->prev;
    }
    free(r);
  }
  xSemaphoreGive(apb_change_lock);
  return true;
}

static uint32_t calculateApb(rtc_cpu_freq_config_t *conf) {
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
  if (conf->freq_mhz >= 80) {
    return 80 * MHZ;
  }
  return (conf->source_freq_mhz * MHZ) / conf->div;
#else
  return APB_CLK_FREQ;
#endif
}

#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(LACT_MODULE) && !defined(LACT_TICKS_PER_US)
void esp_timer_impl_update_apb_freq(uint32_t apb_ticks_per_us);  //private in IDF
#endif

const char *getClockSourceName(uint8_t source) {
  if (source < SOC_CPU_CLK_SRC_INVALID) {
    return clock_source_names[source];
  }

  return "Invalid";
}

const char *getSupportedCpuFrequencyMhz(uint8_t xtal) {
  char *supported_frequencies = (char *)calloc(256, sizeof(char));
  int pos = 0;

#if TARGET_CPU_FREQ_MAX_400
#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_ESP32P4_REV_MIN_FULL < 300
  pos += snprintf(supported_frequencies + pos, 256 - pos, "360");
#else
  pos += snprintf(supported_frequencies + pos, 256 - pos, "400");
#endif
#elif TARGET_CPU_FREQ_MAX_240
#if CONFIG_IDF_TARGET_ESP32
  if (!REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_CPU_FREQ_RATED) || !REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_CPU_FREQ_LOW)) {
    pos += snprintf(supported_frequencies + pos, 256 - pos, "160, 80");
  } else
#endif
  {
    pos += snprintf(supported_frequencies + pos, 256 - pos, "240, 160, 80");
  }
#elif TARGET_CPU_FREQ_MAX_160
  pos += snprintf(supported_frequencies + pos, 256 - pos, "160, 120, 80");
#elif TARGET_CPU_FREQ_MAX_120
  pos += snprintf(supported_frequencies + pos, 256 - pos, "120, 80");
#elif TARGET_CPU_FREQ_MAX_96
  pos += snprintf(supported_frequencies + pos, 256 - pos, "96, 64, 48");
#else
  free(supported_frequencies);
  return "Unknown";
#endif

  // Append xtal and its dividers only if xtal is nonzero
  if (xtal != 0) {
    // We'll show as: , <xtal>, <xtal/2>[, <xtal/4>] MHz
    pos += snprintf(supported_frequencies + pos, 256 - pos, ", %u, %u", xtal, xtal / 2);

#if CONFIG_IDF_TARGET_ESP32
    // Only append xtal/4 if it's > 0 and meaningful for higher-frequency chips (e.g., ESP32 40MHz/4=10)
    if (xtal >= RTC_XTAL_FREQ_40M) {
      pos += snprintf(supported_frequencies + pos, 256 - pos, ", %u", xtal / 4);
    }
#endif
  }

  pos += snprintf(supported_frequencies + pos, 256 - pos, " MHz");
  return supported_frequencies;
}

bool setCpuFrequencyMhz(uint32_t cpu_freq_mhz) {
  rtc_cpu_freq_config_t conf, cconf;
  uint32_t capb, apb;
  [[maybe_unused]] uint8_t xtal = 0;

  // ===== Get XTAL Frequency and validate input =====
#if TARGET_HAS_XTAL_FREQ
  xtal = (uint8_t)rtc_clk_xtal_freq_get();
#endif

  // ===== Get current configuration and check if change is needed =====
  rtc_clk_cpu_freq_get_config(&cconf);
  if (cconf.freq_mhz == cpu_freq_mhz) {
    return true;  // Frequency already set
  }

  // ===== Get configuration for new frequency =====
  if (!rtc_clk_cpu_freq_mhz_to_config(cpu_freq_mhz, &conf)) {
    log_e("CPU clock could not be set to %u MHz. Supported frequencies: %s", cpu_freq_mhz, getSupportedCpuFrequencyMhz(xtal));
    return false;
  }

  // ===== Calculate APB frequencies =====
  capb = calculateApb(&cconf);
  apb = calculateApb(&conf);

  // ===== Apply frequency change =====
  if (apb_change_callbacks) {
    triggerApbChangeCallback(APB_BEFORE_CHANGE, capb, apb);
  }

  rtc_clk_cpu_freq_set_config_fast(&conf);

  // Update APB frequency for targets with dynamic APB
#if TARGET_HAS_DYNAMIC_APB
  if (capb != apb) {
    // Update REF_TICK (uncomment if REF_TICK is different than 1MHz)
    // if (conf.freq_mhz < 80) {
    //   ESP_REG(APB_CTRL_XTAL_TICK_CONF_REG) = conf.freq_mhz / (REF_CLK_FREQ / MHZ) - 1;
    // }
    rtc_clk_apb_freq_update(apb);

    // ESP32-specific: Update esp_timer divisor
#if CONFIG_IDF_TARGET_ESP32
#if defined(LACT_MODULE) && defined(LACT_TICKS_PER_US)
    timer_ll_set_lact_clock_prescale(TIMER_LL_GET_HW(LACT_MODULE), apb / MHZ / LACT_TICKS_PER_US);
#else
    esp_timer_impl_update_apb_freq(apb / MHZ);
#endif
#endif
  }
#endif

  // Update FreeRTOS Tick Divisor for Xtensa targets
#if TARGET_HAS_XTENSA_TICK
  uint32_t fcpu = (conf.freq_mhz >= 80) ? (conf.freq_mhz * MHZ) : (apb);
  _xt_tick_divisor = fcpu / XT_TICK_PER_SEC;
#endif

  if (apb_change_callbacks) {
    triggerApbChangeCallback(APB_AFTER_CHANGE, capb, apb);
  }

  // ===== Debug logging =====
  log_d("%s: %u / %u = %u Mhz, APB: %u Hz", getClockSourceName(conf.source), conf.source_freq_mhz, conf.div, conf.freq_mhz, apb);

  return true;
}

uint32_t getCpuFrequencyMhz() {
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);
  return conf.freq_mhz;
}

uint32_t getXtalFrequencyMhz() {
  return rtc_clk_xtal_freq_get();
}

uint32_t getApbFrequency() {
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);
  return calculateApb(&conf);
}
