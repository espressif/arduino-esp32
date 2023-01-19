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
#include "esp_event.h"
#ifdef CONFIG_MQTT_PROTOCOL_5
#include "mqtt5_client.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ESP_EVENT_DECLARE_BASE
// Define event loop types if macros not available
typedef void *esp_event_loop_handle_t;
typedef void *esp_event_handler_t;
#endif

typedef struct esp_mqtt_client *esp_mqtt_client_handle_t;

/**
 * @brief *MQTT* event types.
 *
 * User event handler receives context data in `esp_mqtt_event_t` structure with
 *  - `client` - *MQTT* client handle
 *  - various other data depending on event type
 *
 */
typedef enum esp_mqtt_event_id_t {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR =
        0, /*!< on error event, additional context: connection return code, error
            handle from esp_tls (if supported) */
    MQTT_EVENT_CONNECTED,    /*!< connected event, additional context:
                              session_present flag */
    MQTT_EVENT_DISCONNECTED, /*!< disconnected event */
    MQTT_EVENT_SUBSCRIBED,   /*!< subscribed event, additional context:
                                - msg_id               message id
                                - error_handle         `error_type` in case subscribing failed
                                - data                 pointer to broker response, check for errors.
                                - data_len             length of the data for this
                              event
                                */
    MQTT_EVENT_UNSUBSCRIBED, /*!< unsubscribed event, additional context:  msg_id */
    MQTT_EVENT_PUBLISHED,    /*!< published event, additional context:  msg_id */
    MQTT_EVENT_DATA,         /*!< data event, additional context:
                                - msg_id               message id
                                - topic                pointer to the received topic
                                - topic_len            length of the topic
                                - data                 pointer to the received data
                                - data_len             length of the data for this event
                                - current_data_offset  offset of the current data for
                              this event
                                - total_data_len       total length of the data received
                                - retain               retain flag of the message
                                - qos                  QoS level of the message
                                - dup                  dup flag of the message
                                Note: Multiple MQTT_EVENT_DATA could be fired for one
                              message, if it is         longer than internal buffer. In that
                              case only first event contains topic         pointer and length,
                              other contain data only with current data length         and
                              current data offset updating.
                                 */
    MQTT_EVENT_BEFORE_CONNECT, /*!< The event occurs before connecting */
    MQTT_EVENT_DELETED,        /*!< Notification on delete of one message from the
                                internal outbox,        if the message couldn't have been sent
                                and acknowledged before expiring        defined in
                                OUTBOX_EXPIRED_TIMEOUT_MS.        (events are not posted upon
                                deletion of successfully acknowledged messages)
                                  - This event id is posted only if
                                MQTT_REPORT_DELETED_MESSAGES==1
                                  - Additional context: msg_id (id of the deleted
                                message).
                                  */
    MQTT_USER_EVENT,            /*!< Custom event used to queue tasks into mqtt event handler
                                 All fields from the esp_mqtt_event_t type could be used to pass
                                 an additional context data to the handler.
                                 */
} esp_mqtt_event_id_t;

/**
 * *MQTT* connection error codes propagated via ERROR event
 */
typedef enum esp_mqtt_connect_return_code_t {
    MQTT_CONNECTION_ACCEPTED = 0,    /*!< Connection accepted  */
    MQTT_CONNECTION_REFUSE_PROTOCOL, /*!< *MQTT* connection refused reason: Wrong
                                      protocol */
    MQTT_CONNECTION_REFUSE_ID_REJECTED, /*!< *MQTT* connection refused reason: ID
                                         rejected */
    MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE, /*!< *MQTT* connection refused
                                                reason: Server unavailable */
    MQTT_CONNECTION_REFUSE_BAD_USERNAME,  /*!< *MQTT* connection refused reason:
                                           Wrong user */
    MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED /*!< *MQTT* connection refused reason:
                                           Wrong username or password */
} esp_mqtt_connect_return_code_t;

/**
 * *MQTT* connection error codes propagated via ERROR event
 */
typedef enum esp_mqtt_error_type_t {
    MQTT_ERROR_TYPE_NONE = 0,
    MQTT_ERROR_TYPE_TCP_TRANSPORT,
    MQTT_ERROR_TYPE_CONNECTION_REFUSED,
    MQTT_ERROR_TYPE_SUBSCRIBE_FAILED
} esp_mqtt_error_type_t;

/**
 * MQTT_ERROR_TYPE_TCP_TRANSPORT error type hold all sorts of transport layer
 * errors, including ESP-TLS error, but in the past only the errors from
 * MQTT_ERROR_TYPE_ESP_TLS layer were reported, so the ESP-TLS error type is
 * re-defined here for backward compatibility
 */
#define MQTT_ERROR_TYPE_ESP_TLS MQTT_ERROR_TYPE_TCP_TRANSPORT

typedef enum esp_mqtt_transport_t {
    MQTT_TRANSPORT_UNKNOWN = 0x0,
    MQTT_TRANSPORT_OVER_TCP, /*!< *MQTT* over TCP, using scheme: ``MQTT`` */
    MQTT_TRANSPORT_OVER_SSL, /*!< *MQTT* over SSL, using scheme: ``MQTTS`` */
    MQTT_TRANSPORT_OVER_WS,  /*!< *MQTT* over Websocket, using scheme:: ``ws`` */
    MQTT_TRANSPORT_OVER_WSS  /*!< *MQTT* over Websocket Secure, using scheme:
                              ``wss`` */
} esp_mqtt_transport_t;

/**
 *  *MQTT* protocol version used for connection
 */
typedef enum esp_mqtt_protocol_ver_t {
    MQTT_PROTOCOL_UNDEFINED = 0,
    MQTT_PROTOCOL_V_3_1,
    MQTT_PROTOCOL_V_3_1_1,
    MQTT_PROTOCOL_V_5,
} esp_mqtt_protocol_ver_t;

/**
 * @brief *MQTT* error code structure to be passed as a contextual information
 * into ERROR event
 *
 * Important: This structure extends `esp_tls_last_error` error structure and is
 * backward compatible with it (so might be down-casted and treated as
 * `esp_tls_last_error` error, but recommended to update applications if used
 * this way previously)
 *
 * Use this structure directly checking error_type first and then appropriate
 * error code depending on the source of the error:
 *
 * | error_type | related member variables | note |
 * | MQTT_ERROR_TYPE_TCP_TRANSPORT | esp_tls_last_esp_err, esp_tls_stack_err,
 * esp_tls_cert_verify_flags, sock_errno | Error reported from
 * tcp_transport/esp-tls | | MQTT_ERROR_TYPE_CONNECTION_REFUSED |
 * connect_return_code | Internal error reported from *MQTT* broker on
 * connection |
 */
typedef struct esp_mqtt_error_codes {
    /* compatible portion of the struct corresponding to struct esp_tls_last_error
     */
    esp_err_t esp_tls_last_esp_err; /*!< last esp_err code reported from esp-tls
                                     component */
    int esp_tls_stack_err; /*!< tls specific error code reported from underlying
                            tls stack */
    int esp_tls_cert_verify_flags; /*!< tls flags reported from underlying tls
                                    stack during certificate verification */
    /* esp-mqtt specific structure extension */
    esp_mqtt_error_type_t
    error_type; /*!< error type referring to the source of the error */
    esp_mqtt_connect_return_code_t
    connect_return_code; /*!< connection refused error code reported from
                              *MQTT* broker on connection */
    /* tcp_transport extension */
    int esp_transport_sock_errno; /*!< errno from the underlying socket */

} esp_mqtt_error_codes_t;

/**
 * *MQTT* event configuration structure
 */
typedef struct esp_mqtt_event_t {
    esp_mqtt_event_id_t event_id;    /*!< *MQTT* event type */
    esp_mqtt_client_handle_t client; /*!< *MQTT* client handle for this event */
    char *data;                      /*!< Data associated with this event */
    int data_len;                    /*!< Length of the data for this event */
    int total_data_len; /*!< Total length of the data (longer data are supplied
                         with multiple events) */
    int current_data_offset; /*!< Actual offset for the data associated with this
                              event */
    char *topic;             /*!< Topic associated with this event */
    int topic_len; /*!< Length of the topic for this event associated with this
                    event */
    int msg_id;    /*!< *MQTT* messaged id of message */
    int session_present; /*!< *MQTT* session_present flag for connection event */
    esp_mqtt_error_codes_t
    *error_handle; /*!< esp-mqtt error handle including esp-tls errors as well
                        as internal *MQTT* errors */
    bool retain; /*!< Retained flag of the message associated with this event */
    int qos;     /*!< QoS of the messages associated with this event */
    bool dup;    /*!< dup flag of the message associated with this event */
    esp_mqtt_protocol_ver_t protocol_ver;   /*!< MQTT protocol version used for connection, defaults to value from menuconfig*/
#ifdef CONFIG_MQTT_PROTOCOL_5
    esp_mqtt5_event_property_t *property; /*!< MQTT 5 property associated with this event */
#endif

} esp_mqtt_event_t;

typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;

typedef esp_err_t (*mqtt_event_callback_t)(esp_mqtt_event_handle_t event);

/**
 * *MQTT* client configuration structure
 *
 *  - Default values can be set via menuconfig
 *  - All certificates and key data could be passed in PEM or DER format. PEM format must have a terminating NULL
 *    character and the related len field set to 0. DER format requires a related len field set to the correct length.
 */
typedef struct esp_mqtt_client_config_t {
  /**
   *   Broker related configuration
   */
  struct broker_t {
      /**
       * Broker address
       *
       *  - uri have precedence over other fields
       *  - If uri isn't set at least hostname, transport and port should.
       */
      struct address_t {
          const char *uri; /*!< Complete *MQTT* broker URI */
          const char *hostname; /*!< Hostname, to set ipv4 pass it as string) */
          esp_mqtt_transport_t transport; /*!< Selects transport*/
          const char *path;               /*!< Path in the URI*/
          uint32_t port;                  /*!< *MQTT* server port */
      } address; /*!< Broker address configuration */
      /**
       * Broker identity verification
       *
       * If fields are not set broker's identity isn't verified. it's recommended
       * to set the options in this struct for security reasons.
       */
      struct verification_t {
          bool use_global_ca_store; /*!< Use a global ca_store, look esp-tls
                         documentation for details. */
          esp_err_t (*crt_bundle_attach)(void *conf); /*!< Pointer to ESP x509 Certificate Bundle attach function for
                                                the usage of certificate bundles. */
          const char *certificate; /*!< Certificate data, default is NULL, not required to verify the server. */
          size_t certificate_len; /*!< Length of the buffer pointed to by certificate. */
          const struct psk_key_hint *psk_hint_key; /*!< Pointer to PSK struct defined in esp_tls.h to enable PSK
                                             authentication (as alternative to certificate verification).
                                             PSK is enabled only if there are no other ways to
                                             verify broker.*/
          bool skip_cert_common_name_check; /*!< Skip any validation of server certificate CN field, this reduces the
                                      security of TLS and makes the *MQTT* client susceptible to MITM attacks  */
          const char **alpn_protos;        /*!< NULL-terminated list of supported application protocols to be used for ALPN */
      } verification; /*!< Security verification of the broker */
  } broker; /*!< Broker address and security verification */
  /**
   * Client related credentials for authentication.
   */
  struct credentials_t {
      const char *username;    /*!< *MQTT* username */
      const char *client_id;   /*!< Set *MQTT* client identifier. Ignored if set_null_client_id == true If NULL set
                         the default client id. Default client id is ``ESP32_%CHIPID%`` where `%CHIPID%` are
                         last 3 bytes of MAC address in hex format */
      bool set_null_client_id; /*!< Selects a NULL client id */
      /**
       * Client authentication
       *
       * Fields related to client authentication by broker
       *
       * For mutual authentication using TLS, user could select certificate and key,
       * secure element or digital signature peripheral if available.
       *
       */
      struct authentication_t {
          const char *password;    /*!< *MQTT* password */
          const char *certificate; /*!< Certificate for ssl mutual authentication, not required if mutual
                             authentication is not needed. Must be provided with `key`.*/
          size_t certificate_len;  /*!< Length of the buffer pointed to by certificate.*/
          const char *key;       /*!< Private key for SSL mutual authentication, not required if mutual authentication
                           is not needed. If it is not NULL, also `certificate` has to be provided.*/
          size_t key_len; /*!< Length of the buffer pointed to by key.*/
          const char *key_password; /*!< Client key decryption password, not PEM nor DER, if provided
                               `key_password_len` must be correctly set. */
          int key_password_len;    /*!< Length of the password pointed to by `key_password` */
          bool use_secure_element; /*!< Enable secure element, available in ESP32-ROOM-32SE, for SSL connection */
          void *ds_data; /*!< Carrier of handle for digital signature parameters, digital signature peripheral is
                   available in some Espressif devices. */
      } authentication; /*!< Client authentication */
  } credentials; /*!< User credentials for broker */
  /**
   * *MQTT* Session related configuration
   */
  struct session_t {
      /**
       * Last Will and Testament message configuration.
       */
      struct last_will_t {
          const char *topic; /*!< LWT (Last Will and Testament) message topic */
          const char *msg; /*!< LWT message, may be NULL terminated*/
          int msg_len; /*!< LWT message length, if msg isn't NULL terminated must have the correct length */
          int qos;     /*!< LWT message QoS */
          int retain;  /*!< LWT retained message flag */
      } last_will; /*!< Last will configuration */
      bool disable_clean_session; /*!< *MQTT* clean session, default clean_session is true */
      int keepalive;              /*!< *MQTT* keepalive, default is 120 seconds */
      bool disable_keepalive; /*!< Set `disable_keepalive=true` to turn off keep-alive mechanism, keepalive is active
                        by default. Note: setting the config value `keepalive` to `0` doesn't disable
                        keepalive feature, but uses a default keepalive period */
      esp_mqtt_protocol_ver_t protocol_ver; /*!< *MQTT* protocol version used for connection.*/
      int message_retransmit_timeout; /*!< timeout for retransmitting of failed packet */
  } session; /*!< *MQTT* session configuration. */
  /**
   * Network related configuration
   */
  struct network_t {
      int reconnect_timeout_ms; /*!< Reconnect to the broker after this value in miliseconds if auto reconnect is not
                          disabled (defaults to 10s) */
      int timeout_ms; /*!< Abort network operation if it is not completed after this value, in milliseconds
                (defaults to 10s). */
      int refresh_connection_after_ms; /*!< Refresh connection after this value (in milliseconds) */
      bool disable_auto_reconnect;     /*!< Client will reconnect to server (when errors/disconnect). Set
                                 `disable_auto_reconnect=true` to disable */
  } network; /*!< Network configuration */
  /**
   * Client task configuration
   */
  struct task_t {
      int priority;   /*!< *MQTT* task priority*/
      int stack_size; /*!< *MQTT* task stack size*/
  } task; /*!< FreeRTOS task configuration.*/
  /**
   * Client buffer size configuration
   *
   * Client have two buffers for input and output respectivelly.
   */
  struct buffer_t {
      int size;     /*!< size of *MQTT* send/receive buffer*/
      int out_size; /*!< size of *MQTT* output buffer. If not defined, defaults to the size defined by
              ``buffer_size`` */
  } buffer; /*!< Buffer size configuration.*/
} esp_mqtt_client_config_t;

/**
 * @brief Creates *MQTT* client handle based on the configuration
 *
 * @param config    *MQTT* configuration structure
 *
 * @return mqtt_client_handle if successfully created, NULL on error
 */
esp_mqtt_client_handle_t
esp_mqtt_client_init(const esp_mqtt_client_config_t *config);

/**
 * @brief Sets *MQTT* connection URI. This API is usually used to overrides the
 * URI configured in esp_mqtt_client_init
 *
 * @param client    *MQTT* client handle
 * @param uri
 *
 * @return ESP_FAIL if URI parse error, ESP_OK on success
 */
esp_err_t esp_mqtt_client_set_uri(esp_mqtt_client_handle_t client,
                                  const char *uri);

/**
 * @brief Starts *MQTT* client with already created client handle
 *
 * @param client    *MQTT* client handle
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL on other error
 */
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);

/**
 * @brief This api is typically used to force reconnection upon a specific event
 *
 * @param client    *MQTT* client handle
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL if client is in invalid state
 */
esp_err_t esp_mqtt_client_reconnect(esp_mqtt_client_handle_t client);

/**
 * @brief This api is typically used to force disconnection from the broker
 *
 * @param client    *MQTT* client handle
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_mqtt_client_disconnect(esp_mqtt_client_handle_t client);

/**
 * @brief Stops *MQTT* client tasks
 *
 *  * Notes:
 *  - Cannot be called from the *MQTT* event handler
 *
 * @param client    *MQTT* client handle
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_FAIL if client is in invalid state
 */
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client);

/**
 * @brief Subscribe the client to defined topic with defined qos
 *
 * Notes:
 * - Client must be connected to send subscribe message
 * - This API is could be executed from a user task or
 * from a *MQTT* event callback i.e. internal *MQTT* task
 * (API is protected by internal mutex, so it might block
 * if a longer data receive operation is in progress.
 *
 * @param client    *MQTT* client handle
 * @param topic
 * @param qos  // TODO describe parameters
 *
 * @return message_id of the subscribe message on success
 *         -1 on failure
 */
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client,
                              const char *topic, int qos);

/**
 * @brief Unsubscribe the client from defined topic
 *
 * Notes:
 * - Client must be connected to send unsubscribe message
 * - It is thread safe, please refer to `esp_mqtt_client_subscribe` for details
 *
 * @param client    *MQTT* client handle
 * @param topic
 *
 * @return message_id of the subscribe message on success
 *         -1 on failure
 */
int esp_mqtt_client_unsubscribe(esp_mqtt_client_handle_t client,
                                const char *topic);

/**
 * @brief Client to send a publish message to the broker
 *
 * Notes:
 * - This API might block for several seconds, either due to network timeout
 * (10s) or if publishing payloads longer than internal buffer (due to message
 *   fragmentation)
 * - Client doesn't have to be connected for this API to work, enqueueing the
 * messages with qos>1 (returning -1 for all the qos=0 messages if
 * disconnected). If MQTT_SKIP_PUBLISH_IF_DISCONNECTED is enabled, this API will
 * not attempt to publish when the client is not connected and will always
 * return -1.
 * - It is thread safe, please refer to `esp_mqtt_client_subscribe` for details
 *
 * @param client    *MQTT* client handle
 * @param topic     topic string
 * @param data      payload string (set to NULL, sending empty payload message)
 * @param len       data length, if set to 0, length is calculated from payload
 * string
 * @param qos       QoS of publish message
 * @param retain    retain flag
 *
 * @return message_id of the publish message (for QoS 0 message_id will always
 * be zero) on success. -1 on failure.
 */
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic,
                            const char *data, int len, int qos, int retain);

/**
 * @brief Enqueue a message to the outbox, to be sent later. Typically used for
 * messages with qos>0, but could be also used for qos=0 messages if store=true.
 *
 * This API generates and stores the publish message into the internal outbox
 * and the actual sending to the network is performed in the mqtt-task context
 * (in contrast to the esp_mqtt_client_publish() which sends the publish message
 * immediately in the user task's context). Thus, it could be used as a non
 * blocking version of esp_mqtt_client_publish().
 *
 * @param client    *MQTT* client handle
 * @param topic     topic string
 * @param data      payload string (set to NULL, sending empty payload message)
 * @param len       data length, if set to 0, length is calculated from payload
 * string
 * @param qos       QoS of publish message
 * @param retain    retain flag
 * @param store     if true, all messages are enqueued; otherwise only QoS 1 and
 * QoS 2 are enqueued
 *
 * @return message_id if queued successfully, -1 otherwise
 */
int esp_mqtt_client_enqueue(esp_mqtt_client_handle_t client, const char *topic,
                            const char *data, int len, int qos, int retain,
                            bool store);

/**
 * @brief Destroys the client handle
 *
 * Notes:
 *  - Cannot be called from the *MQTT* event handler
 *
 * @param client    *MQTT* client handle
 *
 * @return ESP_OK
 *         ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client);

/**
 * @brief Set configuration structure, typically used when updating the config
 * (i.e. on "before_connect" event
 *
 * @param client    *MQTT* client handle
 *
 * @param config    *MQTT* configuration structure
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_ERR_INVALID_ARG if conflicts on transport configuration.
 *         ESP_OK on success
 */
esp_err_t esp_mqtt_set_config(esp_mqtt_client_handle_t client,
                              const esp_mqtt_client_config_t *config);

/**
 * @brief Registers *MQTT* event
 *
 * @param client            *MQTT* client handle
 * @param event             event type
 * @param event_handler     handler callback
 * @param event_handler_arg handlers context
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_ERR_INVALID_ARG on wrong initialization
 *         ESP_OK on success
 */
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
        esp_mqtt_event_id_t event,
        esp_event_handler_t event_handler,
        void *event_handler_arg);

/**
 * @brief Unregisters mqtt event
 *
 * @param client            mqtt client handle
 * @param event             event ID
 * @param event_handler     handler to unregister
 *
 * @return ESP_ERR_NO_MEM if failed to allocate
 *         ESP_ERR_INVALID_ARG on invalid event ID
 *         ESP_OK on success
 */
esp_err_t esp_mqtt_client_unregister_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler);

/**
 * @brief Get outbox size
 *
 * @param client            *MQTT* client handle
 * @return outbox size
 *         0 on wrong initialization
 */
int esp_mqtt_client_get_outbox_size(esp_mqtt_client_handle_t client);

/**
 * @brief Dispatch user event to the mqtt internal event loop
 *
 * @param client            *MQTT* client handle
 * @param event             *MQTT* event handle structure
 * @return ESP_OK on success
 *         ESP_ERR_TIMEOUT if the event couldn't be queued (ref also CONFIG_MQTT_EVENT_QUEUE_SIZE)
 */
esp_err_t esp_mqtt_dispatch_custom_event(esp_mqtt_client_handle_t client, esp_mqtt_event_t *event);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif
