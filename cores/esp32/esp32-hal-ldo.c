/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32-hal-ldo.h"

#if SOC_GP_LDO_SUPPORTED

#include "esp32-hal-log.h"
#include "pins_arduino.h"
#include "soc/soc_caps.h"
#include <stdint.h>

esp_err_t ldoAcquireChannel(uint8_t chan_id, int voltage_mv, bool adjustable, ldo_channel_handle_t *out_handle) {
  if (out_handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  *out_handle = NULL;
  esp_ldo_channel_config_t cfg = {
    .chan_id = (int)chan_id,
    .voltage_mv = voltage_mv,
    .flags =
      {
        .adjustable = adjustable ? 1u : 0u,
        .owned_by_hw = 0u,
      },
  };
  esp_err_t err = esp_ldo_acquire_channel(&cfg, out_handle);
  if (err != ESP_OK) {
    log_e("ldoAcquireChannel: chan=%u %dmV adj=%d failed: %s", (unsigned)chan_id, voltage_mv, adjustable, esp_err_to_name(err));
  }
  return err;
}

esp_err_t ldoReleaseChannel(ldo_channel_handle_t handle) {
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = esp_ldo_release_channel(handle);
  if (err != ESP_OK) {
    log_e("ldoReleaseChannel: %s", esp_err_to_name(err));
  }
  return err;
}

static int8_t s_sdmmc_ldo_chan = -1;

void ldoSdmmcDriverAttached(uint8_t chan_id) {
  s_sdmmc_ldo_chan = (int8_t)chan_id;
}

#if BOARD_PERIMAN_IO_LDO_AUTO

#ifndef BOARD_PERIMAN_IO_LDO0_CHANNEL
#error "BOARD_PERIMAN_IO_LDO0_CHANNEL required when BOARD_PERIMAN_IO_LDO_AUTO is 1"
#endif
#ifndef BOARD_PERIMAN_IO_LDO0_GPIO_MIN
#error "BOARD_PERIMAN_IO_LDO0_GPIO_MIN required when BOARD_PERIMAN_IO_LDO_AUTO is 1"
#endif
#ifndef BOARD_PERIMAN_IO_LDO0_GPIO_MAX
#error "BOARD_PERIMAN_IO_LDO0_GPIO_MAX required when BOARD_PERIMAN_IO_LDO_AUTO is 1"
#endif
#ifndef BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV
#define BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV 3300
#endif

static const struct {
  int chan_id;
  uint8_t gpio_min;
  uint8_t gpio_max;
  int voltage_mv;
} s_io_ldo = {
  .chan_id = BOARD_PERIMAN_IO_LDO0_CHANNEL,
  .gpio_min = BOARD_PERIMAN_IO_LDO0_GPIO_MIN,
  .gpio_max = BOARD_PERIMAN_IO_LDO0_GPIO_MAX,
  .voltage_mv = BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV,
};

static struct {
  int refcount;
  ldo_channel_handle_t handle;
} s_io_ldo_slot;

static bool io_ldo_pin_in_range(uint8_t pin) {
  return (SOC_GPIO_VALID_DIGITAL_IO_PAD_MASK & (1ULL << pin)) != 0ULL && pin >= s_io_ldo.gpio_min && pin <= s_io_ldo.gpio_max;
}

static void io_ldo_release_handle(void) {
  if (s_io_ldo_slot.handle == NULL) {
    return;
  }
  if (ldoReleaseChannel(s_io_ldo_slot.handle) == ESP_OK) {
    log_i("periman IO LDO auto: released channel %d", s_io_ldo.chan_id);
  }
  s_io_ldo_slot.handle = NULL;
}

static void io_ldo_try_acquire(void) {
  if (s_io_ldo_slot.refcount <= 0 || s_io_ldo_slot.handle != NULL) {
    return;
  }
  if (s_sdmmc_ldo_chan >= 0 && s_sdmmc_ldo_chan == s_io_ldo.chan_id) {
    log_d("periman IO LDO auto: channel %d held by SDMMC; skip acquire", s_io_ldo.chan_id);
    return;
  }
  if (ldoAcquireChannel((uint8_t)s_io_ldo.chan_id, s_io_ldo.voltage_mv, false, &s_io_ldo_slot.handle) == ESP_OK) {
    log_i("periman IO LDO auto: enabled channel %d @ %d mV", s_io_ldo.chan_id, s_io_ldo.voltage_mv);
  } else {
    s_io_ldo_slot.handle = NULL;
  }
}

static void io_ldo_try_release(void) {
  if (s_io_ldo_slot.refcount != 0 || s_io_ldo_slot.handle == NULL) {
    return;
  }
  io_ldo_release_handle();
}

static bool io_ldo_channel_matches(uint8_t chan_id) {
  return (int)chan_id == s_io_ldo.chan_id;
}

#endif /* BOARD_PERIMAN_IO_LDO_AUTO */

void ldoPerimanPinBusSet(uint8_t pin, peripheral_bus_type_t old_type, peripheral_bus_type_t new_type) {
#if BOARD_PERIMAN_IO_LDO_AUTO
  if (!io_ldo_pin_in_range(pin)) {
    return;
  }
  const bool old_active = (old_type != ESP32_BUS_TYPE_INIT);
  const bool new_active = (new_type != ESP32_BUS_TYPE_INIT);

  if (!old_active && new_active) {
    s_io_ldo_slot.refcount++;
    io_ldo_try_acquire();
  } else if (old_active && !new_active) {
    if (s_io_ldo_slot.refcount > 0) {
      s_io_ldo_slot.refcount--;
    }
    io_ldo_try_release();
  }
#else
  (void)pin;
  (void)old_type;
  (void)new_type;
#endif
}

void ldoSdmmcPrepareAcquire(uint8_t chan_id) {
#if BOARD_PERIMAN_IO_LDO_AUTO
  if (io_ldo_channel_matches(chan_id)) {
    io_ldo_release_handle();
  }
#else
  (void)chan_id;
#endif
}

void ldoSdmmcDriverDetached(uint8_t chan_id) {
  if (s_sdmmc_ldo_chan == (int8_t)chan_id) {
    s_sdmmc_ldo_chan = -1;
  }
#if BOARD_PERIMAN_IO_LDO_AUTO
  if (io_ldo_channel_matches(chan_id)) {
    io_ldo_try_acquire();
  }
#endif
}

void ldoSdmmcDriverCreateFailed(uint8_t chan_id) {
#if BOARD_PERIMAN_IO_LDO_AUTO
  if (io_ldo_channel_matches(chan_id)) {
    io_ldo_try_acquire();
  }
#else
  (void)chan_id;
#endif
}

#endif /* SOC_GP_LDO_SUPPORTED */
