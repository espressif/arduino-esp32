/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Super Admin User Flag*/
#define ESP_RMAKER_USER_ROLE_SUPER_ADMIN    (1 << 0)

/** Primary User Flag */
#define ESP_RMAKER_USER_ROLE_PRIMARY_USER   (1 << 1)

/** Secondary User Flag */
#define ESP_RMAKER_USER_ROLE_SECONDARY_USER (1 << 2)


/** RainMaker Command Response TLV8 Types */
typedef enum {
    /** Request Id : Variable length string, max 32 characters*/
    ESP_RMAKER_TLV_TYPE_REQ_ID = 1,
    /** User Role : 1 byte */
    ESP_RMAKER_TLV_TYPE_USER_ROLE,
    /** Status : 1 byte */
    ESP_RMAKER_TLV_TYPE_STATUS,
    /** Timestamp : TBD */
    ESP_RMAKER_TLV_TYPE_TIMESTAMP,
    /** Command : 2 bytes*/
    ESP_RMAKER_TLV_TYPE_CMD,
    /** Data : Variable length */
    ESP_RMAKER_TLV_TYPE_DATA
} esp_rmaker_tlv_type_t;

/* RainMaker Command Response Status */
typedef enum {
    /** Success */
    ESP_RMAKER_CMD_STATUS_SUCCESS = 0,
    /** Generic Failure */
    ESP_RMAKER_CMD_STATUS_FAILED,
    /** Invalid Command */
    ESP_RMAKER_CMD_STATUS_CMD_INVALID,
    /** Authentication Failed */
    ESP_RMAKER_CMD_STATUS_AUTH_FAIL,
    /** Command not found */
    ESP_RMAKER_CMD_STATUS_NOT_FOUND,
    /** Last status value */
    ESP_RMAKER_CMD_STATUS_MAX,
} esp_rmaker_cmd_status_t;

#define REQ_ID_LEN  32
typedef struct {
    /** Command id */
    uint16_t cmd;
    /** Request id */
    char req_id[REQ_ID_LEN];
    /** User Role */
    uint8_t user_role;
} esp_rmaker_cmd_ctx_t;

typedef enum {
    /** Standard command: Set Parameters */
    ESP_RMAKER_CMD_TYPE_SET_PARAMS = 1,
    /** Last Standard command */
    ESP_RMAKER_CMD_STANDARD_LAST = 0xfff,
    /** Custom commands can start from here */
    ESP_RMAKER_CMD_CUSTOM_START = 0x1000
} esp_rmaker_cmd_t;

/** Command Response Handler
 *
 * If any command data is received from any of the supported transports (which are outside the scope of this core framework),
 * this function should be called to handle it and fill in the response.
 *
 * @param[in] input Pointer to input data.
 * @param[in] input_len data len.
 * @param[in] output Pointer to output data which should be set by the handler.
 * @param[out] output_len Length of output generated.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_response_handler(const void *input, size_t input_len, void **output, size_t *output_len);

/** Prototype for Command Handler
 *
 * The handler to be invoked when a given command is received.
 *
 * @param[in] in_data Pointer to input data.
 * @param[in] in_len data len.
 * @param[in] out_data Pointer to output data which should be set by the handler.
 * @param[out] out_len Length of output generated.
 * @param[in] ctx Command Context.
 * @param[in] priv Private data, if specified while registering command.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
typedef esp_err_t (*esp_rmaker_cmd_handler_t)(const void *in_data, size_t in_len, void **out_data, size_t *out_len, esp_rmaker_cmd_ctx_t *ctx, void *priv);

/** Register a new command
 *
 * @param[in] cmd Command Identifier. Custom commands should start beyond ESP_RMAKER_CMD_STANDARD_LAST
 * @param[in] access User Access for the command. Can be an OR of the various user role flags like ESP_RMAKER_USER_ROLE_SUPER_ADMIN,
 * ESP_RMAKER_USER_ROLE_PRIMARY_USER and ESP_RMAKER_USER_ROLE_SECONDARY_USER
 * @param[in] handler The handler to be invoked when the given command is received.
 * @param[in] free_on_return Flag to indicate of the framework should free the output after it has been sent as response.
 * @paramp[in] priv Optional private data to be passed to the handler.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_register(uint16_t cmd, uint8_t access, esp_rmaker_cmd_handler_t handler, bool free_on_return, void *priv);

/** De-register a command
 *
 * @param[in] cmd Command Identifier. Custom commands should start beyond ESP_RMAKER_CMD_STANDARD_LAST
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_deregister(uint16_t cmd);

/* Prepare an empty command response
 *
 * This can be used to populate the request to be sent to get all pending commands
 *
 * @param[in] out_data Pointer to output data. This function will allocate memory and set this pointer
 * accordingly.
 * @param[out] out_len Length of output generated.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
 esp_err_t esp_rmaker_cmd_prepare_empty_response(void **output, size_t *output_len);

/** Prototype for Command sending function (TESTING only)
 *
 * @param[in] data Pointer to the data to be sent.
 * @param[in[ data_len Size of data to be sent.
 * @param[in] priv Private data, if applicable.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
typedef esp_err_t (*esp_rmaker_cmd_send_t)(const void *data, size_t data_len, void *priv);

/** Send Test command (TESTING only)
 *
 * @param[in] req_id NULL terminated request id of max 32 characters.
 * @param[in] role User Role flag.
 * @param[in] cmd Command Identifier.
 * @param[in] data Pointer to data for the command.
 * @param[in] data_size Size of the data.
 * @param[in] cmd_send Transport specific function to send the command data.
 * @param[in] priv Private data (if any) to be sent to cmd_send.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_resp_test_send(const char *req_id, uint8_t role, uint16_t cmd, const void *data, size_t data_size, esp_rmaker_cmd_send_t cmd_send, void *priv);

/** Parse response (TESTING only)
 *
 * @param[in] response Pointer to the response received
 * @param[in] response_len Length of the response
 * @param[in] priv Private data, if any. Can be NULL.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_resp_parse_response(const void *response, size_t response_len, void *priv);
#ifdef __cplusplus
}
#endif
