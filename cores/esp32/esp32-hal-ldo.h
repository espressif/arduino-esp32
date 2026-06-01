/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

#if SOC_GP_LDO_SUPPORTED

#include "esp_ldo_regulator.h"
#include "esp32-hal-periman.h"

typedef esp_ldo_channel_handle_t ldo_channel_handle_t;

/**
 * Manual LDO control (any board with SOC_GP_LDO_SUPPORTED).
 *
 * @param chan_id    Datasheet channel ID (e.g. ESP32-P4 LDO_VO4 -> 4).
 * @param voltage_mv Requested output voltage in millivolts.
 * @param adjustable false = fixed voltage, shareable; true = exclusive (example: SDMMC variable voltage: 1.8V / 3.3V).
 * @param out_handle Output pointer that receives the acquired LDO channel handle.
 */
esp_err_t ldoAcquireChannel(uint8_t chan_id, int voltage_mv, bool adjustable, ldo_channel_handle_t *out_handle);
esp_err_t ldoReleaseChannel(ldo_channel_handle_t handle);

/*
 * Optional periman auto-LDO (board opt-in via pins_arduino.h):
 *   BOARD_PERIMAN_IO_LDO_AUTO = 1
 *   BOARD_PERIMAN_IO_LDO0_CHANNEL, BOARD_PERIMAN_IO_LDO0_GPIO_MIN/MAX
 *   BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV (default 3300 in .c if omitted)
 */

void ldoPerimanPinBusSet(uint8_t pin, peripheral_bus_type_t old_type, peripheral_bus_type_t new_type);
void ldoSdmmcPrepareAcquire(uint8_t chan_id);
void ldoSdmmcDriverAttached(uint8_t chan_id);
void ldoSdmmcDriverDetached(uint8_t chan_id);
void ldoSdmmcDriverCreateFailed(uint8_t chan_id);

#endif /* SOC_GP_LDO_SUPPORTED */

#ifdef __cplusplus
}
#endif
