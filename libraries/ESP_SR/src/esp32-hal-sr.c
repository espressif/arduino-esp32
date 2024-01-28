/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S3 && (CONFIG_USE_WAKENET || CONFIG_USE_MULTINET)

#if !defined(ARDUINO_PARTITION_esp_sr_32) && !defined(ARDUINO_PARTITION_esp_sr_16) && !defined(ARDUINO_PARTITION_esp_sr_8)
#warning Compatible partition must be selected for ESP_SR to work
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mn_speech_commands.h"
#include "esp_process_sdkconfig.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_mn_iface.h"
#include "model_path.h"

#include "driver/i2s_common.h"
#include "esp32-hal-sr.h"
#include "esp32-hal-log.h"

#undef ESP_GOTO_ON_FALSE
#define ESP_GOTO_ON_FALSE(a, err_code, goto_tag, format, ...) \
  do { \
    if (unlikely(!(a))) { \
      log_e(format, ##__VA_ARGS__); \
      ret = err_code; \
      goto goto_tag; \
    } \
  } while (0)

#undef ESP_RETURN_ON_FALSE
#define ESP_RETURN_ON_FALSE(a, err_code, format, ...) \
  do { \
    if (unlikely(!(a))) { \
      log_e(format, ##__VA_ARGS__); \
      return err_code; \
    } \
  } while (0)

#define NEED_DELETE BIT0
#define FEED_DELETED BIT1
#define DETECT_DELETED BIT2
#define PAUSE_FEED BIT3
#define PAUSE_DETECT BIT4
#define RESUME_FEED BIT5
#define RESUME_DETECT BIT6

typedef struct {
  wakenet_state_t wakenet_mode;
  esp_mn_state_t state;
  int command_id;
  int phrase_id;
} sr_result_t;

typedef struct {
  model_iface_data_t *model_data;
  const esp_mn_iface_t *multinet;
  const esp_afe_sr_iface_t *afe_handle;
  esp_afe_sr_data_t *afe_data;
  int16_t *afe_in_buffer;
  sr_mode_t mode;
  uint8_t i2s_rx_chan_num;
  sr_event_cb user_cb;
  void *user_cb_arg;
  sr_fill_cb fill_cb;
  void *fill_cb_arg;
  TaskHandle_t feed_task;
  TaskHandle_t detect_task;
  TaskHandle_t handle_task;
  QueueHandle_t result_que;
  EventGroupHandle_t event_group;
} sr_data_t;

static int SR_CHANNEL_NUM = 3;

static srmodel_list_t *models = NULL;
static sr_data_t *g_sr_data = NULL;

esp_err_t sr_set_mode(sr_mode_t mode);

void sr_handler_task(void *pvParam) {
  while (true) {
    sr_result_t result;
    if (xQueueReceive(g_sr_data->result_que, &result, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    if (WAKENET_DETECTED == result.wakenet_mode) {
      if (g_sr_data->user_cb) {
        g_sr_data->user_cb(g_sr_data->user_cb_arg, SR_EVENT_WAKEWORD, -1, -1);
      }
      continue;
    }

    if (WAKENET_CHANNEL_VERIFIED == result.wakenet_mode) {
      if (g_sr_data->user_cb) {
        g_sr_data->user_cb(g_sr_data->user_cb_arg, SR_EVENT_WAKEWORD_CHANNEL, result.command_id, -1);
      }
      continue;
    }

    if (ESP_MN_STATE_DETECTED == result.state) {
      if (g_sr_data->user_cb) {
        g_sr_data->user_cb(g_sr_data->user_cb_arg, SR_EVENT_COMMAND, result.command_id, result.phrase_id);
      }
      continue;
    }

    if (ESP_MN_STATE_TIMEOUT == result.state) {
      if (g_sr_data->user_cb) {
        g_sr_data->user_cb(g_sr_data->user_cb_arg, SR_EVENT_TIMEOUT, -1, -1);
      }
      continue;
    }
  }
  vTaskDelete(NULL);
}

static void audio_feed_task(void *arg) {
  size_t bytes_read = 0;
  int audio_chunksize = g_sr_data->afe_handle->get_feed_chunksize(g_sr_data->afe_data);
  log_i("audio_chunksize=%d, feed_channel=%d", audio_chunksize, SR_CHANNEL_NUM);

  /* Allocate audio buffer and check for result */
  int16_t *audio_buffer = heap_caps_malloc(audio_chunksize * sizeof(int16_t) * SR_CHANNEL_NUM, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (NULL == audio_buffer) {
    esp_system_abort("No mem for audio buffer");
  }
  g_sr_data->afe_in_buffer = audio_buffer;

  while (true) {
    EventBits_t bits = xEventGroupGetBits(g_sr_data->event_group);
    if (NEED_DELETE & bits) {
      xEventGroupSetBits(g_sr_data->event_group, FEED_DELETED);
      break;
    }
    if (PAUSE_FEED & bits) {
      xEventGroupWaitBits(g_sr_data->event_group, PAUSE_FEED | RESUME_FEED, 1, 1, portMAX_DELAY);
    }

    /* Read audio data from I2S bus */
    //ToDo: handle error
    if (g_sr_data->fill_cb == NULL) {
      vTaskDelay(100);
      continue;
    }
    esp_err_t err = g_sr_data->fill_cb(g_sr_data->fill_cb_arg, (char *)audio_buffer, audio_chunksize * g_sr_data->i2s_rx_chan_num * sizeof(int16_t), &bytes_read, portMAX_DELAY);
    if (err != ESP_OK) {
      vTaskDelay(100);
      continue;
    }

    /* Channel Adjust */
    if (g_sr_data->i2s_rx_chan_num == 1) {
      for (int i = audio_chunksize - 1; i >= 0; i--) {
        audio_buffer[i * SR_CHANNEL_NUM + 2] = 0;
        audio_buffer[i * SR_CHANNEL_NUM + 1] = 0;
        audio_buffer[i * SR_CHANNEL_NUM + 0] = audio_buffer[i];
      }
    } else if (g_sr_data->i2s_rx_chan_num == 2) {
      for (int i = audio_chunksize - 1; i >= 0; i--) {
        audio_buffer[i * SR_CHANNEL_NUM + 2] = 0;
        audio_buffer[i * SR_CHANNEL_NUM + 1] = audio_buffer[i * 2 + 1];
        audio_buffer[i * SR_CHANNEL_NUM + 0] = audio_buffer[i * 2 + 0];
      }
    } else {
      vTaskDelay(100);
      continue;
    }

    /* Feed samples of an audio stream to the AFE_SR */
    g_sr_data->afe_handle->feed(g_sr_data->afe_data, audio_buffer);
  }
  vTaskDelete(NULL);
}

static void audio_detect_task(void *arg) {
  int afe_chunksize = g_sr_data->afe_handle->get_fetch_chunksize(g_sr_data->afe_data);
  int mu_chunksize = g_sr_data->multinet->get_samp_chunksize(g_sr_data->model_data);
  assert(mu_chunksize == afe_chunksize);
  log_i("------------detect start------------");

  while (true) {
    EventBits_t bits = xEventGroupGetBits(g_sr_data->event_group);
    if (NEED_DELETE & bits) {
      xEventGroupSetBits(g_sr_data->event_group, DETECT_DELETED);
      break;
    }
    if (PAUSE_DETECT & bits) {
      xEventGroupWaitBits(g_sr_data->event_group, PAUSE_DETECT | RESUME_DETECT, 1, 1, portMAX_DELAY);
    }

    afe_fetch_result_t *res = g_sr_data->afe_handle->fetch(g_sr_data->afe_data);
    if (!res || res->ret_value == ESP_FAIL) {
      continue;
    }

    if (g_sr_data->mode == SR_MODE_WAKEWORD) {
      if (res->wakeup_state == WAKENET_DETECTED) {
        log_d("wakeword detected");
        sr_result_t result = {
          .wakenet_mode = WAKENET_DETECTED,
          .state = ESP_MN_STATE_DETECTING,
          .command_id = 0,
          .phrase_id = 0,
        };
        xQueueSend(g_sr_data->result_que, &result, 0);
      } else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
        sr_set_mode(SR_MODE_OFF);
        log_d("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d", res->trigger_channel_id);
        sr_result_t result = {
          .wakenet_mode = WAKENET_CHANNEL_VERIFIED,
          .state = ESP_MN_STATE_DETECTING,
          .command_id = res->trigger_channel_id,
          .phrase_id = 0,
        };
        xQueueSend(g_sr_data->result_que, &result, 0);
      }
    }

    if (g_sr_data->mode == SR_MODE_COMMAND) {

      esp_mn_state_t mn_state = ESP_MN_STATE_DETECTING;
      mn_state = g_sr_data->multinet->detect(g_sr_data->model_data, res->data);

      if (ESP_MN_STATE_DETECTING == mn_state) {
        continue;
      }

      if (ESP_MN_STATE_TIMEOUT == mn_state) {
        sr_set_mode(SR_MODE_OFF);
        log_d("Time out");
        sr_result_t result = {
          .wakenet_mode = WAKENET_NO_DETECT,
          .state = mn_state,
          .command_id = 0,
          .phrase_id = 0,
        };
        xQueueSend(g_sr_data->result_que, &result, 0);
        continue;
      }

      if (ESP_MN_STATE_DETECTED == mn_state) {
        sr_set_mode(SR_MODE_OFF);
        esp_mn_results_t *mn_result = g_sr_data->multinet->get_results(g_sr_data->model_data);
        for (int i = 0; i < mn_result->num; i++) {
          log_d("TOP %d, command_id: %d, phrase_id: %d, prob: %f",
                i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->prob[i]);
        }

        int sr_command_id = mn_result->command_id[0];
        int sr_phrase_id = mn_result->phrase_id[0];
        log_d("Detected command : %d, phrase: %d", sr_command_id, sr_phrase_id);
        sr_result_t result = {
          .wakenet_mode = WAKENET_NO_DETECT,
          .state = mn_state,
          .command_id = sr_command_id,
          .phrase_id = sr_phrase_id,
        };
        xQueueSend(g_sr_data->result_que, &result, 0);
        continue;
      }
      log_e("Exception unhandled");
    }
  }
  vTaskDelete(NULL);
}

esp_err_t sr_set_mode(sr_mode_t mode) {
  ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, "SR is not running");
  switch (mode) {
    case SR_MODE_OFF:
      if (g_sr_data->mode == SR_MODE_WAKEWORD) {
        g_sr_data->afe_handle->disable_wakenet(g_sr_data->afe_data);
      }
      break;
    case SR_MODE_WAKEWORD:
      if (g_sr_data->mode != SR_MODE_WAKEWORD) {
        g_sr_data->afe_handle->enable_wakenet(g_sr_data->afe_data);
      }
      break;
    case SR_MODE_COMMAND:
      if (g_sr_data->mode == SR_MODE_WAKEWORD) {
        g_sr_data->afe_handle->disable_wakenet(g_sr_data->afe_data);
      }
      break;
    default:
      return ESP_FAIL;
  }
  g_sr_data->mode = mode;
  return ESP_OK;
}

esp_err_t sr_start(sr_fill_cb fill_cb, void *fill_cb_arg, sr_channels_t rx_chan, sr_mode_t mode, const sr_cmd_t sr_commands[], size_t cmd_number, sr_event_cb cb, void *cb_arg) {
  esp_err_t ret = ESP_OK;
  ESP_RETURN_ON_FALSE(NULL == g_sr_data, ESP_ERR_INVALID_STATE, "SR already running");

  g_sr_data = heap_caps_calloc(1, sizeof(sr_data_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_NO_MEM, "Failed create sr data");

  g_sr_data->result_que = xQueueCreate(3, sizeof(sr_result_t));
  ESP_GOTO_ON_FALSE(NULL != g_sr_data->result_que, ESP_ERR_NO_MEM, err, "Failed create result queue");

  g_sr_data->event_group = xEventGroupCreate();
  ESP_GOTO_ON_FALSE(NULL != g_sr_data->event_group, ESP_ERR_NO_MEM, err, "Failed create event_group");

  BaseType_t ret_val;
  g_sr_data->user_cb = cb;
  g_sr_data->user_cb_arg = cb_arg;
  g_sr_data->fill_cb = fill_cb;
  g_sr_data->fill_cb_arg = fill_cb_arg;
  g_sr_data->i2s_rx_chan_num = rx_chan + 1;
  g_sr_data->mode = mode;

  // Init Model
  log_d("init model");
  models = esp_srmodel_init("model");

  // Load WakeWord Detection
  g_sr_data->afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
  afe_config_t afe_config = AFE_CONFIG_DEFAULT();
  afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, "hiesp");
  afe_config.aec_init = false;
  log_d("load wakenet '%s'", afe_config.wakenet_model_name);
  g_sr_data->afe_data = g_sr_data->afe_handle->create_from_config(&afe_config);

  // Load Custom Command Detection
  char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  log_d("load multinet '%s'", mn_name);
  g_sr_data->multinet = esp_mn_handle_from_name(mn_name);
  log_d("load model_data '%s'", mn_name);
  g_sr_data->model_data = g_sr_data->multinet->create(mn_name, 5760);


  // Add commands
  esp_mn_commands_alloc((esp_mn_iface_t *)g_sr_data->multinet, (model_iface_data_t *)g_sr_data->model_data);
  log_i("add %d commands", cmd_number);
  for (size_t i = 0; i < cmd_number; i++) {
    esp_mn_commands_add(sr_commands[i].command_id, (char *)(sr_commands[i].phoneme));
    log_i("  cmd[%d] phrase[%d]:'%s'", sr_commands[i].command_id, i, sr_commands[i].str);
  }

  // Load commands
  esp_mn_error_t *err_id = esp_mn_commands_update();
  if (err_id) {
    for (int i = 0; i < err_id->num; i++) {
      log_e("err cmd id:%d", err_id->phrases[i]->command_id);
    }
  }

  //Start tasks
  log_d("start tasks");
  ret_val = xTaskCreatePinnedToCore(&audio_feed_task, "SR Feed Task", 4 * 1024, NULL, 5, &g_sr_data->feed_task, 0);
  ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, "Failed create audio feed task");
  vTaskDelay(10);
  ret_val = xTaskCreatePinnedToCore(&audio_detect_task, "SR Detect Task", 8 * 1024, NULL, 5, &g_sr_data->detect_task, 1);
  ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, "Failed create audio detect task");
  ret_val = xTaskCreatePinnedToCore(&sr_handler_task, "SR Handler Task", 6 * 1024, NULL, configMAX_PRIORITIES - 1, &g_sr_data->handle_task, 1);
  //ret_val = xTaskCreatePinnedToCore(&sr_handler_task, "SR Handler Task", 6 * 1024, NULL, configMAX_PRIORITIES - 1, &g_sr_data->handle_task, 0);
  ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, "Failed create audio handler task");

  return ESP_OK;
err:
  sr_stop();
  return ret;
}

esp_err_t sr_stop(void) {
  ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, "SR is not running");

  /**
     * Waiting for all task stopped
     * TODO: A task creation failure cannot be handled correctly now
     * */
  vTaskDelete(g_sr_data->handle_task);
  xEventGroupSetBits(g_sr_data->event_group, NEED_DELETE);
  xEventGroupWaitBits(g_sr_data->event_group, NEED_DELETE | FEED_DELETED | DETECT_DELETED, 1, 1, portMAX_DELAY);

  if (g_sr_data->result_que) {
    vQueueDelete(g_sr_data->result_que);
    g_sr_data->result_que = NULL;
  }

  if (g_sr_data->event_group) {
    vEventGroupDelete(g_sr_data->event_group);
    g_sr_data->event_group = NULL;
  }

  if (g_sr_data->model_data) {
    g_sr_data->multinet->destroy(g_sr_data->model_data);
  }

  if (g_sr_data->afe_data) {
    g_sr_data->afe_handle->destroy(g_sr_data->afe_data);
  }

  if (g_sr_data->afe_in_buffer) {
    heap_caps_free(g_sr_data->afe_in_buffer);
  }

  heap_caps_free(g_sr_data);
  g_sr_data = NULL;
  return ESP_OK;
}

esp_err_t sr_pause(void) {
  ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, "SR is not running");
  xEventGroupSetBits(g_sr_data->event_group, PAUSE_FEED | PAUSE_DETECT);
  return ESP_OK;
}

esp_err_t sr_resume(void) {
  ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, "SR is not running");
  xEventGroupSetBits(g_sr_data->event_group, RESUME_FEED | RESUME_DETECT);
  return ESP_OK;
}

#endif  // CONFIG_IDF_TARGET_ESP32S3
