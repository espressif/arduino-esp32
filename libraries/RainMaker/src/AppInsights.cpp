/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include <inttypes.h>
#if defined(CONFIG_ESP_INSIGHTS_ENABLED) && defined(CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK)
#include "Arduino.h"
#include "AppInsights.h"
#include "Insights.h"
#include <esp_rmaker_mqtt.h>
#include <esp_insights.h>
#include <esp_diagnostics.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_common_events.h>

extern "C" {
  bool esp_rmaker_mqtt_is_budget_available();
}

#define INSIGHTS_TOPIC_SUFFIX "diagnostics/from-node"
#define INSIGHTS_TOPIC_RULE "insights_message_delivery"

static void _rmakerCommonEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base != RMAKER_COMMON_EVENT) {
    return;
  }
  esp_insights_transport_event_data_t data;
  switch (event_id) {
    case RMAKER_MQTT_EVENT_PUBLISHED:
      memset(&data, 0, sizeof(data));
      data.msg_id = *(int *)event_data;
      esp_event_post(INSIGHTS_EVENT, INSIGHTS_EVENT_TRANSPORT_SEND_SUCCESS, &data, sizeof(data), portMAX_DELAY);
      break;
    default:
      break;
  }
}

static int _appInsightsDataSend(void *data, size_t len) {
  char topic[128];
  int msg_id = -1;
  if (data == NULL) {
    return 0;
  }
  char *node_id = esp_rmaker_get_node_id();
  if (!node_id) {
    return -1;
  }
  if (esp_rmaker_mqtt_is_budget_available() == false) {
    return ESP_FAIL;
  }
  esp_rmaker_create_mqtt_topic(topic, sizeof(topic), INSIGHTS_TOPIC_SUFFIX, INSIGHTS_TOPIC_RULE);
  esp_rmaker_mqtt_publish(topic, data, len, RMAKER_MQTT_QOS1, &msg_id);
  return msg_id;
}

bool initAppInsights(uint32_t log_type, bool alloc_ext_ram) {
  char *node_id = esp_rmaker_get_node_id();
  esp_insights_transport_config_t transport;
  transport.userdata = NULL;
  transport.callbacks.data_send = _appInsightsDataSend;
  transport.callbacks.init = NULL;
  transport.callbacks.deinit = NULL;
  transport.callbacks.connect = NULL;
  transport.callbacks.disconnect = NULL;
  esp_insights_transport_register(&transport);
  esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, _rmakerCommonEventHandler, NULL);
  return Insights.begin(NULL, node_id, log_type, alloc_ext_ram, false);
}
#else
bool initAppInsights(uint32_t log_type, bool alloc_ext_ram) {
  return false;
}
#endif
