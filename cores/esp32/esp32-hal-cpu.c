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
#include <inttypes.h>
#include "hal/timer_ll.h"
#include "esp_private/systimer.h"
#if __has_include("hal/lact_ll.h")
#include "hal/lact_ll.h"
#endif

#include "esp_system.h"
#include "esp_chip_info.h"
#ifdef ESP_IDF_VERSION_MAJOR  // IDF 4+
// The tables are indexed by soc_cpu_clk_src_t, whose values are only a dense range starting at
// zero on some targets: the ESP32-C5 aliases them to soc_module_clk_t, where XTAL is 16. Naming
// the index keeps every table correct regardless, at the cost of unused holes in a few of them.
#if CONFIG_IDF_TARGET_ESP32  // ESP32/PICO-D4
#include "xtensa_timer.h"
#include "esp32/rom/rtc.h"
static const char *clock_source_names[] = {
  [SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_PLL] = "PLL", [SOC_CPU_CLK_SRC_RC_FAST] = "8.5M", [SOC_CPU_CLK_SRC_APLL] = "APLL"
};
#elif CONFIG_IDF_TARGET_ESP32S2
#include "xtensa_timer.h"
#include "esp32s2/rom/rtc.h"
static const char *clock_source_names[] = {
  [SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_PLL] = "PLL", [SOC_CPU_CLK_SRC_RC_FAST] = "8.5M", [SOC_CPU_CLK_SRC_APLL] = "APLL"
};
#elif CONFIG_IDF_TARGET_ESP32S3
#include "xtensa_timer.h"
#include "esp32s3/rom/rtc.h"
static const char *clock_source_names[] = {[SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_PLL] = "PLL", [SOC_CPU_CLK_SRC_RC_FAST] = "17.5M"};
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/rtc.h"
static const char *clock_source_names[] = {[SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_PLL] = "PLL", [SOC_CPU_CLK_SRC_RC_FAST] = "17.5M"};
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
static const char *clock_source_names[] = {[SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_PLL] = "PLL", [SOC_CPU_CLK_SRC_RC_FAST] = "17.5M"};
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/rtc.h"
static const char *clock_source_names[] = {[SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_PLL] = "PLL", [SOC_CPU_CLK_SRC_RC_FAST] = "17.5M"};
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/rtc.h"
static const char *clock_source_names[] = {
  [SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_PLL] = "PLL", [SOC_CPU_CLK_SRC_RC_FAST] = "8.5M", [SOC_CPU_CLK_SRC_FLASH_PLL] = "FLASH_PLL"
};
#elif CONFIG_IDF_TARGET_ESP32P4
#include "esp32p4/rom/rtc.h"
static const char *clock_source_names[] = {[SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_CPLL] = "CPLL", [SOC_CPU_CLK_SRC_RC_FAST] = "17.5M"};
#elif CONFIG_IDF_TARGET_ESP32C5
#include "esp32c5/rom/rtc.h"
static const char *clock_source_names[] = {
  [SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_RC_FAST] = "17.5M", [SOC_CPU_CLK_SRC_PLL_F160M] = "PLL_F160M", [SOC_CPU_CLK_SRC_PLL_F240M] = "PLL_F240M"
};
#elif CONFIG_IDF_TARGET_ESP32C61
#include "esp32c61/rom/rtc.h"
static const char *clock_source_names[] = {[SOC_CPU_CLK_SRC_XTAL] = "XTAL", [SOC_CPU_CLK_SRC_RC_FAST] = "17.5M", [SOC_CPU_CLK_SRC_PLL_F160M] = "PLL_F160M"};
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
  // Switching the CPU to the XTAL takes the bus clocks down with it: the IDF programs the
  // AHB divider to the CPU divider ("let f_cpu = f_ahb"), so APB_CLK ends up at the CPU
  // frequency. Only a PLL-derived CPU frequency leaves APB_CLK at its nominal frequency.
  if (conf->source == SOC_CPU_CLK_SRC_XTAL) {
    return conf->freq_mhz * MHZ;
  }
  return APB_CLK_FREQ;
#endif
}

#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(LACT_MODULE) && !defined(LACT_TICKS_PER_US)
void esp_timer_impl_update_apb_freq(uint32_t apb_ticks_per_us);  //private in IDF
#endif

const char *getClockSourceName(uint8_t source) {
  if (source < sizeof(clock_source_names) / sizeof(clock_source_names[0]) && clock_source_names[source] != NULL) {
    return clock_source_names[source];
  }

  return "Invalid";
}

#if CONFIG_IDF_TARGET_ESP32
#define REF_TICK_DIV_MIN 10
#elif CONFIG_IDF_TARGET_ESP32S2
#define REF_TICK_DIV_MIN 2
#endif

// The IDF checks the frequency against what the target can generate, but not against what this
// particular chip was rated for, which is lower on some parts
static bool cpuFreqIsRated(uint32_t freq_mhz) {
#if CONFIG_IDF_TARGET_ESP32
  // Both eFuse bits set means the part is rated for 160 MHz instead of the usual 240 MHz
  if (freq_mhz > 160) {
    return !(REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_CPU_FREQ_RATED) && REG_GET_BIT(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_CPU_FREQ_LOW));
  }
#elif CONFIG_IDF_TARGET_ESP32P4
  // Only revision 3 and newer are rated for 400 MHz
  if (freq_mhz > 360) {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    return chip_info.revision >= 300;
  }
#endif

  return true;
}

// rtc_clk_cpu_freq_mhz_to_config() accepts a few frequencies the chip cannot really run, so
// tell those apart from the rest. Both XTAL cases below only concern the XTAL-derived
// frequencies: the PLL is always divided exactly and keeps APB_CLK at its own frequency.
static bool cpuFreqIsUsable(const rtc_cpu_freq_config_t *conf) {
  if (!cpuFreqIsRated(conf->freq_mhz)) {
    return false;
  }

  if (conf->source != SOC_CPU_CLK_SRC_XTAL) {
    return true;
  }

  // The IDF rounds the XTAL divider, so it accepts frequencies it cannot generate exactly
  if ((conf->source_freq_mhz % conf->freq_mhz) != 0) {
    return false;
  }

#ifdef REF_TICK_DIV_MIN
  // On these targets APB_CLK follows the CPU, and the 1 MHz REF_TICK divided from APB_CLK
  // clocks the UART at up to 250000 baud among others. Its divider has a minimum, below which
  // REF_TICK can no longer be 1 MHz, and that is why esp_pm_configure() rejects such a
  // minimum frequency as well. The IDF keeps that limit in a private esp_pm header.
  if (conf->freq_mhz < REF_TICK_DIV_MIN * REF_CLK_FREQ / MHZ) {
    return false;
  }
#endif

  return true;
}

const char *getSupportedCpuFrequencyMhz(void) {
  // The IDF has no API to enumerate the supported CPU frequencies, so ask
  // rtc_clk_cpu_freq_mhz_to_config() about every whole MHz value instead. That is the
  // same check setCpuFrequencyMhz() and esp_pm_configure() use to accept a frequency,
  // and it only evaluates the XTAL frequency against per-target constants, so it can
  // be called freely. Keeping the IDF as the only source of truth means this list stays
  // correct for every target, chip revision and IDF version.
  // The technical reference manuals document a maximum CPU_CLK per target (400 MHz on
  // the ESP32-P4, lower on every other target), so the search starts above every
  // documented maximum. Only the ESP32 and the ESP32-S2 have a documented minimum, which
  // cpuFreqIsUsable() enforces, hence the search ends at 1 MHz on every other target.
  const uint32_t highest_freq_mhz = 1000;
  static char buf[128];
  rtc_cpu_freq_config_t conf;
  size_t pos = 0;

  // The result only depends on the chip itself, so it never changes after the first call
  if (buf[0] != '\0') {
    return buf;
  }

  for (uint32_t freq_mhz = highest_freq_mhz; freq_mhz > 0; freq_mhz--) {
    if (!rtc_clk_cpu_freq_mhz_to_config(freq_mhz, &conf) || conf.freq_mhz != freq_mhz || !cpuFreqIsUsable(&conf)) {
      continue;
    }

    int len = snprintf(buf + pos, sizeof(buf) - pos, (pos == 0) ? "%" PRIu32 : ", %" PRIu32, freq_mhz);
    if (len < 0 || (size_t)len >= sizeof(buf) - pos) {
      break;
    }
    pos += (size_t)len;
  }

  if (pos == 0) {
    return "Unknown";
  }

  snprintf(buf + pos, sizeof(buf) - pos, " MHz");
  return buf;
}

bool setCpuFrequencyMhz(uint32_t cpu_freq_mhz) {
  rtc_cpu_freq_config_t conf, cconf;
  uint32_t capb, apb;

  // ===== Get current configuration and check if change is needed =====
  rtc_clk_cpu_freq_get_config(&cconf);
  if (cconf.freq_mhz == cpu_freq_mhz) {
    return true;  // Frequency already set
  }

  // ===== Get configuration for new frequency =====
  if (!rtc_clk_cpu_freq_mhz_to_config(cpu_freq_mhz, &conf) || !cpuFreqIsUsable(&conf)) {
    log_e("CPU clock could not be set to %" PRIu32 " MHz. Supported frequencies: %s", cpu_freq_mhz, getSupportedCpuFrequencyMhz());
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
#if __has_include("hal/lact_ll.h")
    lact_ll_set_clock_prescale(LACT_LL_GET_HW(LACT_MODULE), apb / MHZ / LACT_TICKS_PER_US);
#else
    timer_ll_set_lact_clock_prescale(TIMER_LL_GET_HW(LACT_MODULE), apb / MHZ / LACT_TICKS_PER_US);
#endif
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
  log_d(
    "%s: %" PRIu32 " / %" PRIu32 " = %" PRIu32 " Mhz, APB: %" PRIu32 " Hz", getClockSourceName(conf.source), conf.source_freq_mhz, conf.div, conf.freq_mhz, apb
  );

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
