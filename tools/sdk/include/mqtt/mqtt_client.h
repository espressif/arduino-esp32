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
#include "esp_event.h"
#if CONFIG_ESP_TLS_USE_DS_PERIPHERAL
#include "rsa_sign_alt.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ESP_EVENT_DECLARE_BASE
// Define event loop types if macros not available
typedef void * esp_event_loop_handle_t;
typedef void * esp_event_handler_t;
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
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,          /*!< on error event, additional context: connection return code, error handle from esp_tls (if supported) */
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

/**
 * MQTT connection error codes propagated via ERROR event
 */
typedef enum {
    MQTT_CONNECTION_ACCEPTED = 0,                   /*!< Connection accepted  */
    MQTT_CONNECTION_REFUSE_PROTOCOL,                /*!< MQTT connection refused reason: Wrong protocol */
    MQTT_CONNECTION_REFUSE_ID_REJECTED,             /*!< MQTT connection refused reason: ID rejected */
    MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE,      /*!< MQTT connection refused reason: Server unavailable */
    MQTT_CONNECTION_REFUSE_BAD_USERNAME,            /*!< MQTT connection refused reason: Wrong user */
    MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED           /*!< MQTT connection refused reason: Wrong username or password */
} esp_mqtt_connect_return_code_t;

/**
 * MQTT connection error codes propagated via ERROR event
 */
typedef enum {
    MQTT_ERROR_TYPE_NONE = 0,
    MQTT_ERROR_TYPE_ESP_TLS,
    MQTT_ERROR_TYPE_CONNECTION_REFUSED,
} esp_mqtt_error_type_t;

typedef enum {
    MQTT_TRANSPORT_UNKNOWN = 0x0,
    MQTT_TRANSPORT_OVER_TCP,      /*!< MQTT over TCP, using scheme: ``mqtt`` */
    MQTT_TRANSPORT_OVER_SSL,      /*!< MQTT over SSL, using scheme: ``mqtts`` */
    MQTT_TRANSPORT_OVER_WS,       /*!< MQTT over Websocket, using scheme:: ``ws`` */
    MQTT_TRANSPORT_OVER_WSS       /*!< MQTT over Websocket Secure, using scheme: ``wss`` */
} esp_mqtt_transport_t;

/**
 *  MQTT protocol version used for connection
 */
typedef enum {
    MQTT_PROTOCOL_UNDEFINED = 0,
    MQTT_PROTOCOL_V_3_1,
    MQTT_PROTOCOL_V_3_1_1
} esp_mqtt_protocol_ver_t;

/**
 * @brief MQTT error code structure to be passed as a contextual information into ERROR event
 *
 * Important: This structure extends `esp_tls_last_error` error structure and is backward compatible with it
 * (so might be down-casted and treated as `esp_tls_last_error` error, but recommended to update applications if used this way previously)
 *
 * Use this structure directly checking error_type first and then appropriate error code depending on the source of the error:
 *
 * | error_type | related member variables | note |
 * | MQTT_ERROR_TYPE_ESP_TLS | esp_tls_last_esp_err, esp_tls_stack_err, esp_tls_cert_verify_flags | Error reported from esp-tls |
 * | MQTT_ERROR_TYPE_CONNECTION_REFUSED | connect_return_code | Internal error reported from MQTT broker on connection |
 */
typedef struct esp_mqtt_error_codes {
    /* compatible portion of the struct corresponding to struct esp_tls_last_error */
    esp_err_t esp_tls_last_esp_err;              /*!< last esp_err code reported from esp-tls component */
    int       esp_tls_stack_err;                 /*!< tls specific error code reported from underlying tls stack */
    int       esp_tls_cert_verify_flags;         /*!< tls flags reported from underlying tls stack during certificate verification */
    /* esp-mqtt specific structure extension */
    esp_mqtt_error_type_t error_type;            /*!< error type referring to the source of the error */
    esp_mqtt_connect_return_code_t connect_return_code; /*!< connection refused error code reported from MQTT broker on connection */
} esp_mqtt_error_codes_t;

/**
 * MQTT event configuration structure
 */
typedef struct {
    esp_mqtt_event_id_t event_id;       /*!< MQTT event type */
    esp_mqtt_client_handle_t client;    /*!< MQTT client handle for this event */
    void *user_context;                 /*!< User context passed from MQTT client config */
    char *data;                         /*!< Data associated with this event */
    int data_len;                       /*!< Length of the data for this event */
    int total_data_len;                 /*!< Total length of the data (longer data are supplied with multiple events) */
    int current_data_offset;            /*!< Actual offset for the data associated with this event */
    char *topic;                        /*!< Topic associated with this event */
    int topic_len;                      /*!< Length of the topic for this event associated with this event */
    int msg_id;                         /*!< MQTT messaged id of message */
    int session_present;                /*!< MQTT session_present flag for connection event */
    esp_mqtt_error_codes_t *error_handle; /*!< esp-mqtt error handle including esp-tls errors as well as internal mqtt errors */
} esp_mqtt_event_t;

typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;

typedef esp_err_t (* mqtt_event_callback_t)(esp_mqtt_event_handle_t event);

/**
 * MQTT client configuration structure
 */
typedef struct {
    mqtt_event_callback_t event_handle;     /*!< handle for MQTT events as a callback in legacy mode */
    esp_event_loop_handle_t event_loop_handle; /*!< handle for MQTT event loop library */
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
    int buffer_size;                        /*!< size of MQTT send/receive buffer, default is 1024 (only receive buffer size if ``out_buffer_size`` defined) */
    const char *cert_pem;                   /*!< Pointer to certificate data in PEM or DER format for server verify (with SSL), default is NULL, not required to verify the server. PEM-format must have a terminating NULL-character. DER-format requires the length to be passed in cert_len. */
    size_t cert_len;                        /*!< Length of the buffer pointed to by cert_pem. May be 0 for null-terminated pem */
    const char *client_cert_pem;            /*!< Pointer to certificate data in PEM or DER format for SSL mutual authentication, default is NULL, not required if mutual authentication is not needed. If it is not NULL, also `client_key_pem` has to be provided. PEM-format must have a terminating NULL-character. DER-format requires the length to be passed in client_cert_len. */
    size_t client_cert_len;                 /*!< Length of the buffer pointed to by client_cert_pem. May be 0 for null-terminated pem */
    const char *client_key_pem;             /*!< Pointer to private key data in PEM or DER format for SSL mutual authentication, default is NULL, not required if mutual authentication is not needed. If it is not NULL, also `client_cert_pem` has to be provided. PEM-format must have a terminating NULL-character. DER-format requires the length to be passed in client_key_len */
    size_t client_key_len;                  /*!< Length of the buffer pointed to by client_key_pem. May be 0 for null-terminated pem */
    esp_mqtt_transport_t transport;         /*!< overrides URI transport */
    int refresh_connection_after_ms;        /*!< Refresh connection after this value (in milliseconds) */
    const struct psk_key_hint* psk_hint_key;     /*!< Pointer to PSK struct defined in esp_tls.h to enable PSK authentication (as alternative to certificate verification). If not NULL and server/client certificates are NULL, PSK is enabled */
    bool          use_global_ca_store;      /*!< Use a global ca_store for all the connections in which this bool is set. */
    int reconnect_timeout_ms;               /*!< Reconnect to the broker after this value in miliseconds if auto reconnect is not disabled */
    const char **alpn_protos;               /*!< NULL-terminated list of supported application protocols to be used for ALPN */
    const char *clientkey_password;         /*!< Client key decryption password string */
    int clientkey_password_len;             /*!< String length of the password pointed to by clientkey_password */
    esp_mqtt_protocol_ver_t protocol_ver;   /*!< MQTT protocol version used for connection, defaults to value from menuconfig*/
    int out_buffer_size;                    /*!< size of MQTT output buffer. If not defined, both output and input buffers have the same size defined as ``buffer_size`` */
    bool skip_cert_common_name_check;       /*!< Skip any validation of server certificate CN field, this reduces the security of TLS and makes the mqtt client susceptible to MITM attacks  */
    bool use_secure_element;                /*!< enable secure element for enabling SSL connection */
    void *ds_data;                          /*!< carrier of handle for digital signature parameters */
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
 * @param client    mqtt client handle
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
 * @brief This api is typically used to force disconnection from the broker
 *
 * @param client    mqtt client handle
 *
 * @return ESP_OK on success
 */
esp_err_t esp_mqtt_client_disconnect(esp_mqtt_client_handle_t client);

/**
 * @brief Stops mqtt client tasks
 *
 *  * Notes:
 *  - Cannot be called from the mqtt event handler
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
 * - This API might block for several seconds, either due to network timeout (10s)
 *   or if publishing payloads longer than internal buffer (due to message
 *   fragmentation)
 * - Client doesn't have to be connected to send publish message
 *   (although it would drop all qos=0 messages, qos>1 messages would be enqueued)
 * - It is thread safe, please refer to `esp_mqtt_client_subscribe` for details
 *
 * @param client    mqtt client handle
 * @param topic     topic string
 * @param data      payload string (set to NULL, sending empty payload message)
 * @param len       data length, if set to 0, length is calculated from payload string
 * @param qos       qos of publish message
 * @param retain    retain flag
 *
 * @return message_id of the publish message (for QoS 0 message_id will always be zero) on success.
 *         -1 on failure.
 */
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);

/**
 * @brief Destroys the client handle
 *
 * Notes:
 *  - Cannot be called from the mqtt event handler
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

/**
 * @brief Registers mqtt event
 *
 * @param client            mqtt client handle
 * @param event             event type
 * @param event_handler     handler callback
 * @param event_handler_arg handlers context
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_OK on success
 */
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void* event_handler_arg);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif
