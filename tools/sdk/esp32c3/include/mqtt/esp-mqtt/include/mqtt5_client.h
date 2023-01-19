/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MQTT5_CLIENT_H_
#define _MQTT5_CLIENT_H_

#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_mqtt_client *esp_mqtt5_client_handle_t;

/**
 *  MQTT5 protocol error reason code, more details refer to MQTT5 protocol document section 2.4
 */
enum mqtt5_error_reason_code {
    MQTT5_UNSPECIFIED_ERROR                      = 0x80,
    MQTT5_MALFORMED_PACKET                       = 0x81,
    MQTT5_PROTOCOL_ERROR                         = 0x82,
    MQTT5_IMPLEMENT_SPECIFIC_ERROR               = 0x83,
    MQTT5_UNSUPPORTED_PROTOCOL_VER               = 0x84,
    MQTT5_INVAILD_CLIENT_ID                      = 0x85,
    MQTT5_BAD_USERNAME_OR_PWD                    = 0x86,
    MQTT5_NOT_AUTHORIZED                         = 0x87,
    MQTT5_SERVER_UNAVAILABLE                     = 0x88,
    MQTT5_SERVER_BUSY                            = 0x89,
    MQTT5_BANNED                                 = 0x8A,
    MQTT5_SERVER_SHUTTING_DOWN                   = 0x8B,
    MQTT5_BAD_AUTH_METHOD                        = 0x8C,
    MQTT5_KEEP_ALIVE_TIMEOUT                     = 0x8D,
    MQTT5_SESSION_TAKEN_OVER                     = 0x8E,
    MQTT5_TOPIC_FILTER_INVAILD                   = 0x8F,
    MQTT5_TOPIC_NAME_INVAILD                     = 0x90,
    MQTT5_PACKET_IDENTIFIER_IN_USE               = 0x91,
    MQTT5_PACKET_IDENTIFIER_NOT_FOUND            = 0x92,
    MQTT5_RECEIVE_MAXIMUM_EXCEEDED               = 0x93,
    MQTT5_TOPIC_ALIAS_INVAILD                    = 0x94,
    MQTT5_PACKET_TOO_LARGE                       = 0x95,
    MQTT5_MESSAGE_RATE_TOO_HIGH                  = 0x96,
    MQTT5_QUOTA_EXCEEDED                         = 0x97,
    MQTT5_ADMINISTRATIVE_ACTION                  = 0x98,
    MQTT5_PAYLOAD_FORMAT_INVAILD                 = 0x99,
    MQTT5_RETAIN_NOT_SUPPORT                     = 0x9A,
    MQTT5_QOS_NOT_SUPPORT                        = 0x9B,
    MQTT5_USE_ANOTHER_SERVER                     = 0x9C,
    MQTT5_SERVER_MOVED                           = 0x9D,
    MQTT5_SHARED_SUBSCR_NOT_SUPPORTED            = 0x9E,
    MQTT5_CONNECTION_RATE_EXCEEDED               = 0x9F,
    MQTT5_MAXIMUM_CONNECT_TIME                   = 0xA0,
    MQTT5_SUBSCRIBE_IDENTIFIER_NOT_SUPPORT       = 0xA1,
    MQTT5_WILDCARD_SUBSCRIBE_NOT_SUPPORT         = 0xA2,
};

/**
 *  MQTT5 user property handle
 */
typedef struct mqtt5_user_property_list_t *mqtt5_user_property_handle_t;

/**
 *  MQTT5 protocol connect properties and will properties configuration, more details refer to MQTT5 protocol document section 3.1.2.11 and 3.3.2.3
 */
typedef struct {
    uint32_t session_expiry_interval;            /*!< The interval time of session expiry */ 
    uint32_t maximum_packet_size;                /*!< The maximum packet size that we can receive */
    uint16_t receive_maximum;                    /*!< The maximum pakcket count that we process concurrently */
    uint16_t topic_alias_maximum;                /*!< The maximum topic alias that we support */
    bool request_resp_info;                      /*!< This value to request Server to return Response information */
    bool request_problem_info;                   /*!< This value to indicate whether the reason string or user properties are sent in case of failures */
    mqtt5_user_property_handle_t user_property;  /*!< The handle for user property, call function esp_mqtt5_client_set_user_property to set it */
    uint32_t will_delay_interval;                /*!< The time interval that server delays publishing will message  */
    uint32_t message_expiry_interval;            /*!< The time interval that message expiry */
    bool payload_format_indicator;               /*!< This value is to indicator will message payload format */
    const char *content_type;                    /*!< This value is to indicator will message content type, use a MIME content type string */
    const char *response_topic;                  /*!< Topic name for a response message */
    const char *correlation_data;                /*!< Binary data for receiver to match the response message */
    uint16_t correlation_data_len;               /*!< The length of correlation data */
    mqtt5_user_property_handle_t will_user_property;  /*!< The handle for will message user property, call function esp_mqtt5_client_set_user_property to set it */
} esp_mqtt5_connection_property_config_t;

/**
 *  MQTT5 protocol publish properties configuration, more details refer to MQTT5 protocol document section 3.3.2.3
 */
typedef struct {
    bool payload_format_indicator;               /*!< This value is to indicator publish message payload format */
    uint32_t message_expiry_interval;            /*!< The time interval that message expiry */
    uint16_t topic_alias;                        /*!< An interger value to identify the topic instead of using topic name string */
    const char *response_topic;                  /*!< Topic name for a response message */
    const char *correlation_data;                /*!< Binary data for receiver to match the response message */
    uint16_t correlation_data_len;               /*!< The length of correlation data */
    const char *content_type;                    /*!< This value is to indicator publish message content type, use a MIME content type string */
    mqtt5_user_property_handle_t user_property;  /*!< The handle for user property, call function esp_mqtt5_client_set_user_property to set it */
} esp_mqtt5_publish_property_config_t;

/**
 *  MQTT5 protocol subscribe properties configuration, more details refer to MQTT5 protocol document section 3.8.2.1
 */
typedef struct {
    uint16_t subscribe_id;                       /*!< A variable byte represents the identifier of the subscription */
    bool no_local_flag;                          /*!< Subscription Option to allow that server publish message that client sent */
    bool retain_as_published_flag;               /*!< Subscription Option to keep the retain flag as published option */
    uint8_t retain_handle;                       /*!< Subscription Option to handle retain option */
    bool is_share_subscribe;                     /*!< Whether subscribe is a shared subscription */
    const char *share_name;                      /*!< The name of shared subscription which is a part of $share/{share_name}/{topic} */
    mqtt5_user_property_handle_t user_property;  /*!< The handle for user property, call function esp_mqtt5_client_set_user_property to set it */
} esp_mqtt5_subscribe_property_config_t;

/**
 *  MQTT5 protocol unsubscribe properties configuration, more details refer to MQTT5 protocol document section 3.10.2.1
 */
typedef struct {
    bool is_share_subscribe;                     /*!< Whether subscribe is a shared subscription */
    const char *share_name;                      /*!< The name of shared subscription which is a part of $share/{share_name}/{topic} */
    mqtt5_user_property_handle_t user_property;  /*!< The handle for user property, call function esp_mqtt5_client_set_user_property to set it */
} esp_mqtt5_unsubscribe_property_config_t;

/**
 *  MQTT5 protocol disconnect properties configuration, more details refer to MQTT5 protocol document section 3.14.2.2
 */
typedef struct {
    uint32_t session_expiry_interval;            /*!< The interval time of session expiry */
    uint8_t disconnect_reason;                   /*!< The reason that connection disconnet, refer to mqtt5_error_reason_code */
    mqtt5_user_property_handle_t user_property;  /*!< The handle for user property, call function esp_mqtt5_client_set_user_property to set it */
} esp_mqtt5_disconnect_property_config_t;

/**
 *  MQTT5 protocol for event properties
 */
typedef struct {
    bool payload_format_indicator;      /*!< Payload format of the message */
    char *response_topic;               /*!< Response topic of the message */
    int response_topic_len;             /*!< Response topic length of the message */
    char *correlation_data;             /*!< Correlation data of the message */
    uint16_t correlation_data_len;      /*!< Correlation data length of the message */
    char *content_type;                 /*!< Content type of the message */
    int content_type_len;               /*!< Content type length of the message */
    mqtt5_user_property_handle_t user_property;  /*!< The handle for user property, call function esp_mqtt5_client_delete_user_property to free the memory */
} esp_mqtt5_event_property_t;

/**
 *  MQTT5 protocol for user property
 */
typedef struct {
    const char *key;                       /*!< Item key name */
    const char *value;                     /*!< Item value string */
} esp_mqtt5_user_property_item_t;

/**
 * @brief Set MQTT5 client connect property configuration
 *
 * @param client            mqtt client handle
 * @param connect_property  connect property
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL on fail
 *         ESP_OK on success
 */
esp_err_t esp_mqtt5_client_set_connect_property(esp_mqtt5_client_handle_t client, const esp_mqtt5_connection_property_config_t *connect_property);

/**
 * @brief Set MQTT5 client publish property configuration
 *
 * This API will not store the publish property, it is one-time configuration.
 * Before call `esp_mqtt_client_publish` to publish data, call this API to set publish property if have
 *
 * @param client            mqtt client handle
 * @param property          publish property
 *
 * @return ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL on fail
 *         ESP_OK on success
 */
esp_err_t esp_mqtt5_client_set_publish_property(esp_mqtt5_client_handle_t client, const esp_mqtt5_publish_property_config_t *property);

/**
 * @brief Set MQTT5 client subscribe property configuration
 *
 * This API will not store the subscribe property, it is one-time configuration.
 * Before call `esp_mqtt_client_subscribe` to subscribe topic, call this API to set subscribe property if have
 *
 * @param client            mqtt client handle
 * @param property          subscribe property
 *
 * @return ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL on fail
 *         ESP_OK on success
 */
esp_err_t esp_mqtt5_client_set_subscribe_property(esp_mqtt5_client_handle_t client, const esp_mqtt5_subscribe_property_config_t *property);

/**
 * @brief Set MQTT5 client unsubscribe property configuration
 *
 * This API will not store the unsubscribe property, it is one-time configuration.
 * Before call `esp_mqtt_client_unsubscribe` to unsubscribe topic, call this API to set unsubscribe property if have
 *
 * @param client            mqtt client handle
 * @param property          unsubscribe property
 *
 * @return ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL on fail
 *         ESP_OK on success
 */
esp_err_t esp_mqtt5_client_set_unsubscribe_property(esp_mqtt5_client_handle_t client, const esp_mqtt5_unsubscribe_property_config_t *property);

/**
 * @brief Set MQTT5 client disconnect property configuration
 *
 * This API will not store the disconnect property, it is one-time configuration.
 * Before call `esp_mqtt_client_disconnect` to disconnect connection, call this API to set disconnect property if have
 *
 * @param client            mqtt client handle
 * @param property          disconnect property
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL on fail
 *         ESP_OK on success
 */
esp_err_t esp_mqtt5_client_set_disconnect_property(esp_mqtt5_client_handle_t client, const esp_mqtt5_disconnect_property_config_t *property);

/**
 * @brief Set MQTT5 client user property configuration
 *
 * This API will allocate memory for user_property, please DO NOT forget `call esp_mqtt5_client_delete_user_property`
 * after you use it.
 * Before publish data, subscribe topic, unsubscribe, etc, call this API to set user property if have
 *
 * @param user_property            user_property handle
 * @param item                     array of user property data (eg. {{"var","val"},{"other","2"}})
 * @param item_num                 number of items in user property data
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_FAIL on fail
 *         ESP_OK on success
 */
esp_err_t esp_mqtt5_client_set_user_property(mqtt5_user_property_handle_t *user_property, esp_mqtt5_user_property_item_t item[], uint8_t item_num);

/**
 * @brief Get MQTT5 client user property
 *
 * @param user_property            user_property handle
 * @param item                     point that store user property data
 * @param item_num                 number of items in user property data
 *
 * This API can use with `esp_mqtt5_client_get_user_property_count` to get list count of user property.
 * And malloc number of count item array memory to store the user property data.
 * Please DO NOT forget the item memory, key and value point in item memory when get user property data successfully.
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_FAIL on fail
 *         ESP_OK on success
 */
esp_err_t esp_mqtt5_client_get_user_property(mqtt5_user_property_handle_t user_property, esp_mqtt5_user_property_item_t *item, uint8_t *item_num);

/**
 * @brief Get MQTT5 client user property list count
 *
 * @param user_property            user_property handle
 * @return user property list count
 */
uint8_t esp_mqtt5_client_get_user_property_count(mqtt5_user_property_handle_t user_property);

/**
 * @brief Free the user property list
 *
 * @param user_property            user_property handle
 * 
 * This API will free the memory in user property list and free user_property itself
 */
void esp_mqtt5_client_delete_user_property(mqtt5_user_property_handle_t user_property);
#ifdef __cplusplus
}
#endif //__cplusplus

#endif
