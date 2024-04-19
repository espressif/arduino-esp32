/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S3 && (CONFIG_USE_WAKENET || CONFIG_USE_MULTINET)

#include "driver/i2s_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SR_CMD_STR_LEN_MAX     64
#define SR_CMD_PHONEME_LEN_MAX 64

typedef struct sr_cmd_t {
  int command_id;
  char str[SR_CMD_STR_LEN_MAX];
  char phoneme[SR_CMD_PHONEME_LEN_MAX];
} sr_cmd_t;

typedef enum {
  SR_EVENT_WAKEWORD,          //WakeWord Detected
  SR_EVENT_WAKEWORD_CHANNEL,  //WakeWord Channel Verified
  SR_EVENT_COMMAND,           //Command Detected
  SR_EVENT_TIMEOUT,           //Command Timeout
  SR_EVENT_MAX
} sr_event_t;

typedef enum {
  SR_MODE_OFF,       //Detection Off
  SR_MODE_WAKEWORD,  //WakeWord Detection
  SR_MODE_COMMAND,   //Command Detection
  SR_MODE_MAX
} sr_mode_t;

typedef enum {
  SR_CHANNELS_MONO,
  SR_CHANNELS_STEREO,
  SR_CHANNELS_MAX
} sr_channels_t;

typedef void (*sr_event_cb)(void *arg, sr_event_t event, int command_id, int phrase_id);
typedef esp_err_t (*sr_fill_cb)(void *arg, void *out, size_t len, size_t *bytes_read, uint32_t timeout_ms);

esp_err_t sr_start(
  sr_fill_cb fill_cb, void *fill_cb_arg, sr_channels_t rx_chan, sr_mode_t mode, const sr_cmd_t *sr_commands, size_t cmd_number, sr_event_cb cb, void *cb_arg
);
esp_err_t sr_stop(void);
esp_err_t sr_pause(void);
esp_err_t sr_resume(void);
esp_err_t sr_set_mode(sr_mode_t mode);

// static const sr_cmd_t sr_commands[] = {
//     {0, "Turn On the Light", "TkN nN jc LiT"},
//     {0, "Switch On the Light", "SWgp nN jc LiT"},
//     {1, "Switch Off the Light", "SWgp eF jc LiT"},
//     {1, "Turn Off the Light", "TkN eF jc LiT"},
//     {2, "Turn Red", "TkN RfD"},
//     {3, "Turn Green", "TkN GRmN"},
//     {4, "Turn Blue", "TkN BLo"},
//     {5, "Customize Color", "KcSTcMiZ KcLk"},
//     {6, "Sing a song", "Sgl c Sel"},
//     {7, "Play Music", "PLd MYoZgK"},
//     {8, "Next Song", "NfKST Sel"},
//     {9, "Pause Playing", "PeZ PLdgl"},
// };

#ifdef __cplusplus
}
#endif

#endif  // CONFIG_IDF_TARGET_ESP32S3
