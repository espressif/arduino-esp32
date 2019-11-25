/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 * Tuan PM <tuanpm at live dot com>
 */

#ifndef _MQTT_CLIENT_H_
#define _MQTT_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"

#include "mqtt_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_mqtt_client *esp_mqtt_client_handle_t;

/**
 * @brief MQTT event types.
 *
 * User event handler receives context data in `esp_mqtt_event_t` structure with
 *  - `user_context` - user data from `esp_mqtt_client_config_t`
 *  - `client` - mqtt client handle
 *  - various other data depending on event type
 *
 */
typedef enum {
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED,          /*!< connected event, additional context: session_present flag */
    MQTT_EVENT_DISCONNECTED,       /*!< disconnected event */
    MQTT_EVENT_SUBSCRIBED,         /*!< subscribed event, additional context: msg_id */
    MQTT_EVENT_UNSUBSCRIBED,       /*!< unsubscribed event */
    MQTT_EVENT_PUBLISHED,          /*!< published event, additional context:  msg_id */
    MQTT_EVENT_DATA,               /*!< data event, additional context:
                                        - msg_id               message id
                                        - topic                pointer to the received topic
                                        - topic_len            length of the topic
                                        - data                 pointer to the received data
                                        - data_len             length of the data for this event
                                        - current_data_offset  offset of the current data for this event
                                        - total_data_len       total length of the data received
                                        Note: Multiple MQTT_EVENT_DATA could be fired for one message, if it is
                                        longer than internal buffer. In that case only first event contains topic
                                        pointer and length, other contain data only with current data length
                                        and current data offset updating.
                                         */
    MQTT_EVENT_BEFORE_CONNECT,     /*!< The event occurs before connecting */
} esp_mqtt_event_id_t;

typedef enum {
    MQTT_TRANSPORT_UNKNOWN = 0x0,
    MQTT_TRANSPORT_OVER_TCP,      /*!< MQTT over TCP, using scheme: ``mqtt`` */
    MQTT_TRANSPORT_OVER_SSL,      /*!< MQTT over SSL, using scheme: ``mqtts`` */
    MQTT_TRANSPORT_OVER_WS,       /*!< MQTT over Websocket, using scheme:: ``ws`` */
    MQTT_TRANSPORT_OVER_WSS       /*!< MQTT over Websocket Secure, using scheme: ``wss`` */
} esp_mqtt_transport_t;

/**
 * MQTT event configuration structure
 */
typedef struct {
    esp_mqtt_event_id_t event_id;       /*!< MQTT event type */
    esp_mqtt_client_handle_t client;    /*!< MQTT client handle for this event */
    void *user_context;                 /*!< User context passed from MQTT client config */
    char *data;                         /*!< Data asociated with this event */
    int data_len;                       /*!< Lenght of the data for this event */
    int total_data_len;                 /*!< Total length of the data (longer data are supplied with multiple events) */
    int current_data_offset;            /*!< Actual offset for the data asociated with this event */
    char *topic;                        /*!< Topic asociated with this event */
    int topic_len;                      /*!< Length of the topic for this event asociated with this event */
    int msg_id;                         /*!< MQTT messaged id of message */
    int session_present;                /*!< MQTT session_present flag for connection event */
} esp_mqtt_event_t;

typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;

typedef esp_err_t (* mqtt_event_callback_t)(esp_mqtt_event_handle_t event);

/**
 * MQTT client configuration structure
 */
typedef struct {
    mqtt_event_callback_t event_handle;     /*!< handle for MQTT events */
    const char *host;                       /*!< MQTT server domain (ipv4 as string) */
    const char *uri;                        /*!< Complete MQTT broker URI */
    uint32_t port;                          /*!< MQTT server port */
    const char *client_id;                  /*!< default client id is ``ESP32_%CHIPID%`` where %CHIPID% are last 3 bytes of MAC address in hex format */
    const char *username;                   /*!< MQTT username */
    const char *password;                   /*!< MQTT password */
    const char *lwt_topic;                  /*!< LWT (Last Will and Testament) message topic (NULL by default) */
    const char *lwt_msg;                    /*!< LWT message (NULL by default) */
    int lwt_qos;                            /*!< LWT message qos */
    int lwt_retain;                         /*!< LWT retained message flag */
    int lwt_msg_len;                        /*!< LWT message length */
    int disable_clean_session;              /*!< mqtt clean session, default clean_session is true */
    int keepalive;                          /*!< mqtt keepalive, default is 120 seconds */
    bool disable_auto_reconnect;            /*!< this mqtt client will reconnect to server (when errors/disconnect). Set disable_auto_reconnect=true to disable */
    void *user_context;                     /*!< pass user context to this option, then can receive that context in ``event->user_context`` */
    int task_prio;                          /*!< MQTT task priority, default is 5, can be changed in ``make menuconfig`` */
    int task_stack;                         /*!< MQTT task stack size, default is 6144 bytes, can be changed in ``make menuconfig`` */
    int buffer_size;                        /*!< size of MQTT send/receive buffer, default is 1024 */
    const char *cert_pem;                   /*!< Pointer to certificate data in PEM format for server verify (with SSL), default is NULL, not required to verify the server */
    const char *client_cert_pem;            /*!< Pointer to certificate data in PEM format for SSL mutual authentication, default is NULL, not required if mutual authentication is not needed. If it is not NULL, also `client_key_pem` has to be provided. */
    const char *client_key_pem;             /*!< Pointer to private key data in PEM format for SSL mutual authentication, default is NULL, not required if mutual authentication is not needed. If it is not NULL, also `client_cert_pem` has to be provided. */
    esp_mqtt_transport_t transport;         /*!< overrides URI transport */
    int refresh_connection_after_ms;        /*!< Refresh connection after this value (in milliseconds) */
} esp_mqtt_client_config_t;

/**
 * @brief Creates mqtt client handle based on the configuration
 *
 * @param config    mqtt configuration structure
 *
 * @return mqtt_client_handle if successfully created, NULL on error
 */
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);

/**
 * @brief Sets mqtt connection URI. This API is usually used to overrides the URI
 * configured in esp_mqtt_client_init
 *
 * @param client    mqtt client hanlde
 * @param uri
 *
 * @return ESP_FAIL if URI parse error, ESP_OK on success
 */
esp_err_t esp_mqtt_client_set_uri(esp_mqtt_client_handle_t client, const char *uri);

/**
 * @brief Starts mqtt client with already created client handle
 *
 * @param client    mqtt client handle
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL on other error
 */
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);

/**
 * @brief This api is typically used to force reconnection upon a specific event
 *
 * @param client    mqtt client handle
 *
 * @return ESP_OK on success
 *         ESP_FAIL if client is in invalid state
 */
esp_err_t esp_mqtt_client_reconnect(esp_mqtt_client_handle_t client);

/**
 * @brief Stops mqtt client tasks
 *
 * @param client    mqtt client handle
 *
 * @return ESP_OK on success
 *         ESP_FAIL if client is in invalid state
 */
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client);

/**
 * @brief Subscribe the client to defined topic with defined qos
 *
 * Notes:
 * - Client must be connected to send subscribe message
 * - This API is could be executed from a user task or
 * from a mqtt event callback i.e. internal mqtt task
 * (API is protected by internal mutex, so it might block
 * if a longer data receive operation is in progress.
 *
 * @param client    mqtt client handle
 * @param topic
 * @param qos
 *
 * @return message_id of the subscribe message on success
 *         -1 on failure
 */
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos);

/**
 * @brief Unsubscribe the client from defined topic
 *
 * Notes:
 * - Client must be connected to send unsubscribe message
 * - It is thread safe, please refer to `esp_mqtt_client_subscribe` for details
 *
 * @param client    mqtt client handle
 * @param topic
 *
 * @return message_id of the subscribe message on success
 *         -1 on failure
 */
int esp_mqtt_client_unsubscribe(esp_mqtt_client_handle_t client, const char *topic);

/**
 * @brief Client to send a publish message to the broker
 *
 * Notes:
 * - Client doesn't have to be connected to send publish message
 *   (although it would drop all qos=0 messages, qos>1 messages would be enqueued)
 * - It is thread safe, please refer to `esp_mqtt_client_subscribe` for details
 *
 * @param client    mqtt client handle
 * @param topic     topic string
 * @param data      payload string (set to NULL, sending empty payload message)
 * @param len       data length, if set to 0, length is calculated from payload string
 * @param qos       qos of publish message
 * @param retain    ratain flag
 *
 * @return message_id of the subscribe message on success
 *         0 if cannot publish
 */
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);

/**
 * @brief Destroys the client handle
 *
 * @param client    mqtt client handle
 *
 * @return ESP_OK
 */
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client);

/**
 * @brief Set configuration structure, typically used when updating the config (i.e. on "before_connect" event
 *
 * @param client    mqtt client handle
 *
 * @param config    mqtt configuration structure
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_OK on success
 */
esp_err_t esp_mqtt_set_config(esp_mqtt_client_handle_t client, const esp_mqtt_client_config_t *config);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif
