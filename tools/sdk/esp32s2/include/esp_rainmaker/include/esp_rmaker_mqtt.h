// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include <stdint.h>
#include <esp_err.h>
#include <esp_rmaker_mqtt_glue.h>

#ifdef __cplusplus
extern "C"
{
#endif

esp_rmaker_mqtt_conn_params_t *esp_rmaker_mqtt_get_conn_params(void);

/** Initialize ESP RainMaker MQTT
 *
 * @param[in] config The MQTT configuration data
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_init(esp_rmaker_mqtt_conn_params_t *conn_params);

/* Deinitialize ESP RainMaker MQTT
 *
 * Call this function after MQTT has disconnected.
 */
void esp_rmaker_mqtt_deinit(void);

/** MQTT Connect
 *
 * Starts the connection attempts to the MQTT broker as per the configuration
 * provided during initializing.
 * This should ideally be called after successful network connection.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_connect(void);

/** MQTT Disconnect
 *
 * Disconnects from the MQTT broker.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_disconnect(void);

/** Publish MQTT Message
 *
 * @param[in] topic The MQTT topic on which the message should be published.
 * @param[in] data Data to be published
 * @param[in] data_len Length of the data
 * @param[in] qos Quality of Service for the Publish. Can be 0, 1 or 2. Also depends on what the MQTT broker supports.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_publish(const char *topic, void *data, size_t data_len, uint8_t qos, int *msg_id);

/** Subscribe to MQTT topic
 *
 * @param[in] topic The topic to be subscribed to.
 * @param[in] cb The callback to be invoked when a message is received on the given topic.
 * @param[in] priv_data Optional private data to be passed to the callback
 * @param[in] qos Quality of Service for the Subscription. Can be 0, 1 or 2. Also depends on what the MQTT broker supports.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_subscribe(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, uint8_t qos, void *priv_data);

/** Unsubscribe from MQTT topic
 *
 * @param[in] topic Topic from which to unsubscribe.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_unsubscribe(const char *topic);
esp_err_t esp_rmaker_mqtt_setup(esp_rmaker_mqtt_config_t mqtt_config);

/** Creates appropriate MQTT Topic String based on CONFIG_ESP_RMAKER_MQTT_USE_BASIC_INGEST_TOPICS
 * @param[out] buf Buffer to hold topic string
 * @param[in] buf_size Size of buffer
 * @param[in] topic_suffix MQTT Topic suffix
 * @param[in] rule Basic Ingests Rule Name
*/
void esp_rmaker_create_mqtt_topic(char *buf, size_t buf_size, const char *topic_suffix, const char *rule);

#ifdef __cplusplus
}
#endif
