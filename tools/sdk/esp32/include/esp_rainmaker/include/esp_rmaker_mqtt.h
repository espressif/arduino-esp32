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

#ifdef __cplusplus
extern "C"
{
#endif

/** ESP RainMaker MQTT Configuration */
typedef struct {
    /** MQTT Host */
    char *mqtt_host;
    /** Client ID */
    char *client_id;
    /** Client Certificate in NULL terminate PEM format */
    char *client_cert;
    /** Client Key in NULL terminate PEM format */
    char *client_key;
    /** Server Certificate in NULL terminate PEM format */
    char *server_cert;
} esp_rmaker_mqtt_config_t;

/** ESP RainMaker MQTT Subscribe callback prototype
 *
 * @param[in] topic Topic on which the message was received
 * @param[in] payload Data received in the message
 * @param[in] payload_len Length of the data
 * @param[in] priv_data The private data passed during subscription
 */
typedef void (*esp_rmaker_mqtt_subscribe_cb_t) (const char *topic, void *payload, size_t payload_len, void *priv_data);
   
/** Initialize ESP RainMaker MQTT
 *
 * @param[in] config The MQTT configuration data
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_init(esp_rmaker_mqtt_config_t *config);

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
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_publish(const char *topic, void *data, size_t data_len);

/** Subscribe to MQTT topic
 *
 * @param[in] topic The topic to be subscribed to.
 * @param[in] cb The callback to be invoked when a message is received on the given topic.
 * @param[in] priv_data Optional private data to be passed to the callback
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_subscribe(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, void *priv_data);

/** Unsubscribe from MQTT topic
 *
 * @param[in] topic Topic from which to unsubscribe.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_unsubscribe(const char *topic);

#ifdef __cplusplus
}
#endif
