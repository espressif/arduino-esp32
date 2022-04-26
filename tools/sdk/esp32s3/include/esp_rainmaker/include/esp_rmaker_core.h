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
#include <stdbool.h>
#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ESP_RMAKER_CONFIG_VERSION    "2020-03-20"

/* Maximum length of the alert message that can be passed to esp_rmaker_raise_alert() */
#define ESP_RMAKER_MAX_ALERT_LEN    100

/** @cond **/
/** ESP RainMaker Event Base */
ESP_EVENT_DECLARE_BASE(RMAKER_EVENT);
/** @endcond **/

/** ESP RainMaker Events */
typedef enum {
    /** RainMaker Core Initialisation Done */
    RMAKER_EVENT_INIT_DONE = 1,
    /** Self Claiming Started */
    RMAKER_EVENT_CLAIM_STARTED,
    /** Self Claiming was Successful */
    RMAKER_EVENT_CLAIM_SUCCESSFUL,
    /** Self Claiming Failed */
    RMAKER_EVENT_CLAIM_FAILED,
    /** Node side communication for User-Node mapping done.
     * Actual mapping state will be managed by the ESP RainMaker cloud based on the user side communication.
     * Associated data is the NULL terminated user id.
     */
    RMAKER_EVENT_USER_NODE_MAPPING_DONE,
    /** Local control started. Associated data is the NULL terminated Service Name */
    RMAKER_EVENT_LOCAL_CTRL_STARTED,
    /* User reset request successfully sent to ESP RainMaker Cloud */
    RMAKER_EVENT_USER_NODE_MAPPING_RESET,
} esp_rmaker_event_t;

/** ESP RainMaker Node information */
typedef struct {
    /** Name of the Node */
    char *name;
    /** Type of the Node */
    char *type;
    /** Firmware Version (Optional). If not set, PROJECT_VER is used as default (recommended)*/
    char *fw_version;
    /** Model (Optional). If not set, PROJECT_NAME is used as default (recommended)*/
    char *model;
} esp_rmaker_node_info_t;

/** ESP RainMaker Configuration */
typedef struct {
    /** Enable Time Sync
     * Setting this true will enable SNTP and fetch the current time before
     * attempting to connect to the ESP RainMaker service
     */
    bool enable_time_sync;
} esp_rmaker_config_t;

/** ESP RainMaker Parameter Value type */
typedef enum {
    /** Invalid */
    RMAKER_VAL_TYPE_INVALID = 0,
    /** Boolean */
    RMAKER_VAL_TYPE_BOOLEAN,
    /** Integer. Mapped to a 32 bit signed integer */
    RMAKER_VAL_TYPE_INTEGER,
    /** Floating point number */
    RMAKER_VAL_TYPE_FLOAT,
    /** NULL terminated string */
    RMAKER_VAL_TYPE_STRING,
    /** NULL terminated JSON Object string Eg. {"name":"value"} */
    RMAKER_VAL_TYPE_OBJECT,
    /** NULL terminated JSON Array string Eg. [1,2,3] */
    RMAKER_VAL_TYPE_ARRAY,
} esp_rmaker_val_type_t;

/** ESP RainMaker Value */
typedef union {
    /** Boolean */
    bool b;
    /** Integer */
    int i;
    /** Float */
    float f;
    /** NULL terminated string */
    char *s;
} esp_rmaker_val_t;

/** ESP RainMaker Parameter Value */
typedef struct {
    /** Type of Value */
    esp_rmaker_val_type_t type;
    /** Actual value. Depends on the type */
    esp_rmaker_val_t val;
} esp_rmaker_param_val_t;

/** Param property flags */
typedef enum {
    PROP_FLAG_WRITE = (1 << 0),
    PROP_FLAG_READ = (1 << 1),
    PROP_FLAG_TIME_SERIES = (1 << 2),
    PROP_FLAG_PERSIST = (1 << 3)
} esp_param_property_flags_t;

/** System Service Reboot Flag */
#define SYSTEM_SERV_FLAG_REBOOT         (1 << 0)

/** System Service Factory Reset Flag */
#define SYSTEM_SERV_FLAG_FACTORY_RESET  (1 << 1)

/** System Service Wi-Fi Reset Flag */
#define SYSTEM_SERV_FLAG_WIFI_RESET     (1 << 2)

/** System Service All Flags */
#define SYSTEM_SERV_FLAGS_ALL   (SYSTEM_SERV_FLAG_REBOOT | SYSTEM_SERV_FLAG_FACTORY_RESET | SYSTEM_SERV_FLAG_WIFI_RESET)

/** Generic ESP RainMaker handle */
typedef size_t esp_rmaker_handle_t;

/**  ESP RainMaker Node Handle */
typedef esp_rmaker_handle_t esp_rmaker_node_t;

/**  ESP RainMaker Device Handle */
typedef esp_rmaker_handle_t esp_rmaker_device_t;

/** ESP RainMaker Parameter Handle */
typedef esp_rmaker_handle_t esp_rmaker_param_t;

/** Parameter read/write request source */
typedef enum {
    /** Request triggered in the init sequence i.e. when a value is found
     * in persistent memory for parameters with PROP_FLAG_PERSIST.
     */
    ESP_RMAKER_REQ_SRC_INIT,
    /** Request received from cloud */
    ESP_RMAKER_REQ_SRC_CLOUD,
    /** Request received when a schedule has triggered */
    ESP_RMAKER_REQ_SRC_SCHEDULE,
    /** Request received when a scene has been activated */
    ESP_RMAKER_REQ_SRC_SCENE_ACTIVATE,
    /** Request received when a scene has been deactivated */
    ESP_RMAKER_REQ_SRC_SCENE_DEACTIVATE,
    /** Request received from a local controller */
    ESP_RMAKER_REQ_SRC_LOCAL,
    /** This will always be the last value. Any value equal to or
     * greater than this should be considered invalid.
     */
    ESP_RMAKER_REQ_SRC_MAX,
} esp_rmaker_req_src_t;

/** Write request Context */
typedef struct {
    /** Source of request */
    esp_rmaker_req_src_t src;
} esp_rmaker_write_ctx_t;

/** Read request context */
typedef struct {
    /** Source of request */
    esp_rmaker_req_src_t src;
} esp_rmaker_read_ctx_t;

/** System service configuration */
typedef struct {
    /** Logical OR of system service flags (SYSTEM_SERV_FLAG_REBOOT,
     * SYSTEM_SERV_FLAG_FACTORY_RESET, SYSTEM_SERV_FLAG_WIFI_RESET) as required
     * or SYSTEM_SERV_FLAGS_ALL.
     */
    uint16_t flags;
    /** Time in seconds after which the device should reboot.
     * Value of zero would trigger an immediate reboot if a write is received for
     * the Reboot parameter.
     * Recommended value: 2
     */
    int8_t reboot_seconds;
    /** Time in seconds after which the device should reset (Wi-Fi or factory).
     * Value of zero would trigger an immediate action if a write is received for
     * the Wi-Fi reset or Factory reset parameter.
     * Recommended value: 2
     */
    int8_t reset_seconds;
    /** Time in seconds after which the device should reboot after it has been reset.
     * Value of zero would mean that there won't be any reboot after the reset.
     * Recommended value: 2
     */
    int8_t reset_reboot_seconds;
} esp_rmaker_system_serv_config_t;

/** Callback for parameter value write requests.
 *
 * The callback should call the esp_rmaker_param_update_and_report() API if the new value is to be set
 * and reported back.
 *
 * @param[in] device Device handle.
 * @param[in] param Parameter handle.
 * @param[in] param Pointer to \ref esp_rmaker_param_val_t. Use appropriate elements as per the value type.
 * @param[in] priv_data Pointer to the private data paassed while creating the device.
 * @param[in] ctx Context associated with the request.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
typedef esp_err_t (*esp_rmaker_device_write_cb_t)(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx);

/** Callback for parameter value changes
 *
 * The callback should call the esp_rmaker_param_update_and_report() API if the new value is to be set
 * and reported back.
 *
 * @note Currently, the read callback never gets invoked as the communication between clients (mobile phones, CLI, etc.)
 * and node is asynchronous. So, the read request does not reach the node. This callback will however be used in future.
 *
 * @param[in] device Device handle.
 * @param[in] param Parameter handle.
 * @param[in] priv_data Pointer to the private data passed while creating the device.
 * @param[in] ctx Context associated with the request.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
typedef esp_err_t (*esp_rmaker_device_read_cb_t)(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        void *priv_data, esp_rmaker_read_ctx_t *ctx);

/** Convert device callback source to string
 *
 * Device read/write callback can be via different sources. This is a helper API
 * to give the source in string format for printing.
 *
 * Example Usage:
 * @code{c}
 * static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
 * const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
 * @endcode
 *
 * @param[in] src The src field as received in the callback context.
 *
 * @return NULL terminated source string on success
 * @return NULL on failure
 */
const char *esp_rmaker_device_cb_src_to_str(esp_rmaker_req_src_t src);

/**
 * Initialise a Boolean value
 *
 * @param[in] bval Initialising value.
 *
 * @return Value structure.
 */
esp_rmaker_param_val_t esp_rmaker_bool(bool bval);

/**
 * Initialise an Integer value
 *
 * @param[in] ival Initialising value.
 *
 * @return Value structure.
 */
esp_rmaker_param_val_t esp_rmaker_int(int ival);

/**
 * Initialise a Float value
 *
 * @param[in] fval Initialising value.
 *
 * @return Value structure.
 */
esp_rmaker_param_val_t esp_rmaker_float(float fval);

/**
 * Initialise a String value
 *
 * @param[in] sval Initialising value.
 *
 * @return Value structure.
 */
esp_rmaker_param_val_t esp_rmaker_str(const char *sval);

/**
 * Initialise a json object value
 *
 * @note the object will not be validated internally. it is the application's
 * responsibility to ensure that the object is a valid json object.
 * eg. esp_rmaker_obj("{\"name\":\"value\"}");
 *
 * param[in] val initialising value
 *
 * return value structure
 */
esp_rmaker_param_val_t esp_rmaker_obj(const char *val);

/**
 * Initialise a json array value
 *
 * @note the array will not be validated internally. it is the application's
 * responsibility to ensure that the array is a valid json array.
 * eg. esp_rmaker_array("[1,2,3]");
 *
 * param[in] val initialising value
 *
 * return value structure
 */
esp_rmaker_param_val_t esp_rmaker_array(const char *val);


/** Initialize ESP RainMaker Node
 *
 * This initializes the ESP RainMaker agent and creates the node.
 * The model and firmware version for the node are set internally as per
 * the project name and version. These can be overridden (but not recommended) using the
 * esp_rmaker_node_add_fw_version() and esp_rmaker_node_add_model() APIs.
 *
 * @note This should be the first call before using any other ESP RainMaker API.
 *
 * @param[in] config Configuration to be used by the ESP RainMaker.
 * @param[in] name Name of the node.
 * @param[in] type Type of the node.
 *
 * @return Node handle on success.
 * @return NULL in case of failure.
 */
esp_rmaker_node_t *esp_rmaker_node_init(const esp_rmaker_config_t *config, const char *name, const char *type);

/** Start ESP RainMaker Agent
 *
 * This call starts the actual ESP RainMaker thread. This should preferably be called after a
 * successful Wi-Fi connection in order to avoid unnecessary failures.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_start(void);

/** Stop ESP RainMaker Agent
 *
 * This call stops the ESP RainMaker Agent instance started earlier by esp_rmaker_start().
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_stop(void);

/** Deinitialize ESP RainMaker Node
 *
 * This API deinitializes the ESP RainMaker agent and the node created using esp_rmaker_node_init().
 *
 * @note This should be called after rainmaker has stopped.
 *
 * @param[in] node Node Handle returned by esp_rmaker_node_init().
 *
 * @retur ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_node_deinit(const esp_rmaker_node_t *node);

/** Get a handle to the Node
 *
 * This API returns handle to a node created using esp_rmaker_node_init().
 *
 * @return Node handle on success.
 * @return NULL in case of failure.
 */
const esp_rmaker_node_t *esp_rmaker_get_node(void);

/** Get Node Id
 *
 * Returns pointer to the NULL terminated Node ID string.
 *
 * @return Pointer to a NULL terminated Node ID string.
 */
char *esp_rmaker_get_node_id(void);

/** Get Node Info
 *
 * Returns pointer to the node info as configured during initialisation.
 *
 * @param node Node handle.
 *
 * @return Pointer to the node info on success.
 * @return NULL in case of failure.
 */
esp_rmaker_node_info_t *esp_rmaker_node_get_info(const esp_rmaker_node_t *node);

/** Add Node attribute
 *
 * Adds a new attribute as the metadata for the node. For the sake of simplicity,
 * only string values are allowed.
 *
 * @param node Node handle.
 * @param[in] attr_name Name of the attribute.
 * @param[in] val Value for the attribute.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_node_add_attribute(const esp_rmaker_node_t *node, const char *attr_name, const char *val);

/** Add FW version for a node (Not recommended)
 *
 * FW version is set internally to the project version. This API can be used to
 * override that version.
 *
 * @param node Node handle.
 * @param[in] fw_version New firmware version.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_node_add_fw_version(const esp_rmaker_node_t *node, const char *fw_version);

/** Add model for a node (Not recommended)
 *
 * Model is set internally to the project name. This API can be used to
 * override that name.
 *
 * @param node Node handle.
 * @param[in] model New model string.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_node_add_model(const esp_rmaker_node_t *node, const char *model);

/**
 * Create a Device
 *
 * This API will create a virtual "Device".
 * This could be something like a Switch, Lightbulb, etc.
 *
 * @note The device created needs to be added to a node using esp_rmaker_node_add_device().
 *
 * @param[in] dev_name The unique device name.
 * @param[in] type Optional device type. Can be kept NULL.
 * @param[in] priv_data (Optional) Private data associated with the device. This will be passed to callbacks.
 * It should stay allocated throughout the lifetime of the device.
 *
 * @return Device handle on success.
 * @return NULL in case of any error.
 */
esp_rmaker_device_t *esp_rmaker_device_create(const char *dev_name, const char *type, void *priv_data);

/**
 * Create a Service
 *
 * This API will create a "Service". It is exactly same like a device in terms of structure and so, all
 * APIs for device are also valid for a service.
 * A service could be something like OTA, diagnostics, etc.
 *
 * @note Name of a service should not clash with name of a device.
 * @note The service created needs to be added to a node using esp_rmaker_node_add_device().
 *
 * @param[in] serv_name The unique service name.
 * @param[in] type Optional service type. Can be kept NULL.
 * @param[in] priv_data (Optional) Private data associated with the service. This will be passed to callbacks.
 * It should stay allocated throughout the lifetime of the device.
 *
 * @return Device handle on success.
 * @return NULL in case of any error.
 */
esp_rmaker_device_t *esp_rmaker_service_create(const char *serv_name, const char *type, void *priv_data);

/**
 * Delete a Device/Service
 *
 * This API will delete a device created using esp_rmaker_device_create().
 *
 * @note The device should first be removed from the node using esp_rmaker_node_remove_device() before deleting.
 *
 * @param[in] device Device handle.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_device_delete(const esp_rmaker_device_t *device);

/**
 * Add callbacks for a device/service
 *
 * Add read/write callbacks for a device that will be invoked as per requests received from the cloud (or other paths
 * as may be added in future).
 *
 * @param[in] device Device handle.
 * @param[in] write_cb Write callback.
 * @param[in] read_cb Read callback.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_device_add_cb(const esp_rmaker_device_t *device, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb);

/**
 * Add a device to a node
 *
 * @param[in] node Node handle.
 * @param[in] device Device handle.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_node_add_device(const esp_rmaker_node_t *node, const esp_rmaker_device_t *device);

/**
 * Remove a device from a node
 *
 * @param[in] node Node handle.
 * @param[in] device Device handle.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_node_remove_device(const esp_rmaker_node_t *node, const esp_rmaker_device_t *device);

/** Get device by name
 *
 * Get handle for a device based on the name.
 *
 * @param[in] node Node handle.
 * @param[in] device_name Device name to search.
 *
 * @return Device handle on success.
 * @return NULL in case of failure.
 */
esp_rmaker_device_t *esp_rmaker_node_get_device_by_name(const esp_rmaker_node_t *node, const char *device_name);

/** Add a Device attribute
 *
 * @note Device attributes are reported only once after a boot-up as part of the node
 * configuration.
 * Eg. Serial Number
 *
 * @param[in] device Device handle.
 * @param[in] attr_name Name of the attribute.
 * @param[in] val Value of the attribute.
 *
 * @return ESP_OK if the attribute was added successfully.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_device_add_attribute(const esp_rmaker_device_t *device, const char *attr_name, const char *val);

/** Add a Device subtype
 *
 * This can be something like esp.subtype.rgb-light for a device of type esp.device.lightbulb.
 * This would primarily be used by the phone apps to render different icons for the same device type.
 *
 * @param[in] device Device handle.
 * @param[in] subtype String describing the sub type.
 *
 * @return ESP_OK if the subtype was added successfully.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_device_add_subtype(const esp_rmaker_device_t *device, const char *subtype);

/** Get device name from handle
 *
 * @param[in] device Device handle.
 *
 * @return NULL terminated device name string on success.
 * @return NULL in case of failure.
 */
char *esp_rmaker_device_get_name(const esp_rmaker_device_t *device);

/** Get device type from handle
 *
 * @param[in] device Device handle.
 *
 * @return NULL terminated device type string on success.
 * @return NULL in case of failure, or if the type wasn't provided while creating the device.
 */
char *esp_rmaker_device_get_type(const esp_rmaker_device_t *device);

/**
 * Add a parameter to a device/service
 *
 * @param[in] device Device handle.
 * @param[in] param Parameter handle.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_device_add_param(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param);


/** Get parameter by type
 *
 * Get handle for a parameter based on the type.
 *
 * @note If there are multiple parameters with the same type, this will return the first one. The API
 * esp_rmaker_device_get_param_by_name() can be used to get a specific parameter, because the parameter
 * names in a device are unique.
 *
 * @param[in] device Device handle.
 * @param[in] param_type Parameter type to search.
 *
 * @return Parameter handle on success.
 * @return NULL in case of failure.
 */
esp_rmaker_param_t *esp_rmaker_device_get_param_by_type(const esp_rmaker_device_t *device, const char *param_type);

/** Get parameter by name
 *
 * Get handle for a parameter based on the name.
 *
 * @param[in] device Device handle.
 * @param[in] param_name Parameter name to search.
 *
 * @return Parameter handle on success.
 * @return NULL in case of failure.
 */
esp_rmaker_param_t *esp_rmaker_device_get_param_by_name(const esp_rmaker_device_t *device, const char *param_name);

/** Assign a primary parameter
 *
 * Assign a parameter (already added using esp_rmaker_device_add_param()) as a primary parameter,
 * which can be used by clients (phone apps specifically) to give prominence to it.
 *
 * @param[in] device Device handle.
 * @param[in] param Parameter handle.
 *
 * @return ESP_OK if the parameter was assigned as the primary successfully.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_device_assign_primary_param(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param);

/**
 * Create a Parameter
 *
 * Parameter can be something like Temperature, Outlet state, Lightbulb brightness, etc.
 *
 * Any changes should be reported using the esp_rmaker_param_update_and_report() API.
 * Any remote changes will be reported to the application via the device callback, if registered.
 *
 * @note The parameter created needs to be added to a device using esp_rmaker_device_add_param().
 * Parameter name should be unique in a given device.
 *
 * @param[in] param_name Name of the parameter.
 a* @param[in] type Optional parameter type. Can be kept NULL.
 * @param[in] val Value of the parameter. This also specifies the type that will be assigned
 * to this parameter. You can use esp_rmaker_bool(), esp_rmaker_int(), esp_rmaker_float()
 * or esp_rmaker_str() functions as the argument here. Eg, esp_rmaker_bool(true).
 * @param[in] properties Properties of the parameter, which will be a logical OR of flags in
 *  \ref esp_param_property_flags_t.
 *
 * @return Parameter handle on success.
 * @return NULL in case of failure.
 */
esp_rmaker_param_t *esp_rmaker_param_create(const char *param_name, const char *type,
        esp_rmaker_param_val_t val, uint8_t properties);

/**
 * Add a UI Type to a parameter
 *
 * This will be used by the Phone apps (or other clients) to render appropriate UI for the given
 * parameter. Please refer the RainMaker documetation for supported UI Types.
 *
 * @param[in] param Parameter handle.
 * @param[in] ui_type String describing the UI Type.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_param_add_ui_type(const esp_rmaker_param_t *param, const char *ui_type);

/**
 * Add bounds for an integer/float parameter
 *
 * This can be used to add bounds (min/max values) for a given integer parameter. Eg. brightness
 * will have bounds as 0 and 100 if it is a percentage.
 * Eg. esp_rmaker_param_add_bounds(brightness_param, esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(5));
 *
 * @note The RainMaker core does not check the bounds. It is upto the application to handle it.
 *
 * @param[in] param Parameter handle.
 * @param[in] min Minimum allowed value.
 * @param[in] max Maximum allowed value.
 * @param[in] step Minimum stepping (set to 0 if no specific value is desired).
 *
 * @return ESP_OK on success.
 * return error in case of failure.
 */
esp_err_t esp_rmaker_param_add_bounds(const esp_rmaker_param_t *param,
    esp_rmaker_param_val_t min, esp_rmaker_param_val_t max, esp_rmaker_param_val_t step);

/**
 * Add a list of valid strings for a string parameter
 *
 * This can be used to add a list of valid strings for a given string parameter.
 *
 * Eg.
 * static const char *valid_strs[] = {"None","Yes","No","Can't Say"};
 * esp_rmaker_param_add_valid_str_list(param, valid_strs, 4);
 *
 * @note The RainMaker core does not check the values. It is upto the application to handle it.
 *
 * @param[in] param Parameter handle.
 * @param[in] strs Pointer to an array of strings. Note that this memory should stay allocated
 * throughout the lifetime of this parameter.
 * @param[in] count Number of strings in the above array.
 *
 * @return ESP_OK on success.
 * return error in case of failure.
 */
esp_err_t esp_rmaker_param_add_valid_str_list(const esp_rmaker_param_t *param, const char *strs[], uint8_t count);

/** Add max count for an array parameter
 *
 * This can be used to put a limit on the maximum number of elements in an array.
 *
 * @note The RainMaker core does not check the values. It is upto the application to handle it.
 *
 * @param[in] param Parameter handle.
 * @param[in] count Max number of elements allowed in the array.
 *
 * @return ESP_OK on success.
 * return error in case of failure.
 */
esp_err_t esp_rmaker_param_add_array_max_count(const esp_rmaker_param_t *param, int count);


/* Update a parameter
 *
 * This will just update the value of a parameter with esp rainmaker core, without actually reporting
 * it. This can be used when multiple parameters need to be reported together.
 * Eg. If x parameters are to be reported, this API can be used for the first x -1 parameters
 * and the last one can be updated using esp_rmaker_param_update_and_report().
 * This will report all parameters which were updated prior to this call.
 *
 * Sample:
 *
 * esp_rmaker_param_update(param1, esp_rmaker_float(10.2));
 * esp_rmaker_param_update(param2, esp_rmaker_int(55));
 * esp_rmaker_param_update(param3, esp_rmaker_int(95));
 * esp_rmaker_param_update_and_report(param1, esp_rmaker_bool(true));
 *
 * @param[in] param Parameter handle.
 * @param[in] val New value of the parameter.
 *
 * @return ESP_OK if the parameter was updated successfully.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_param_update(const esp_rmaker_param_t *param, esp_rmaker_param_val_t val);

/** Update and report a parameter
 *
 * Calling this API will update the parameter and report it to ESP RainMaker cloud.
 * This should be used whenever there is any local change.
 *
 * @param[in] param Parameter handle.
 * @param[in] val New value of the parameter.
 *
 * @return ESP_OK if the parameter was updated successfully.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_param_update_and_report(const esp_rmaker_param_t *param, esp_rmaker_param_val_t val);

/** Update and notify a parameter
 *
 * Calling this API will update the parameter and report it to ESP RainMaker cloud similar to
 * esp_rmaker_param_update_and_report(). However, additionally, it will also trigger a notification
 * on the phone apps (if enabled).
 *
 * @note This should be used only when some local change requires explicit notification even when the
 * phone app is in background, not otherwise.
 * Eg. Alarm got triggered, temperature exceeded some threshold, etc.
 *
 * Alternatively, the esp_rmaker_raise_alert() API can also be used to trigger notification
 * on the phone apps with pre-formatted text.
 *
 * @param[in] param Parameter handle.
 * @param[in] val New value of the parameter.
 *
 * @return ESP_OK if the parameter was updated successfully.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_param_update_and_notify(const esp_rmaker_param_t *param, esp_rmaker_param_val_t val);

/** Trigger an alert on the phone app
 *
 * This API will trigger a notification alert on the phone apps (if enabled) using the formatted text
 * provided. Note that this does not send a notification directly to the phone, but reports the alert
 * to the ESP RainMaker cloud which then uses the Notification framework to send notifications to the
 * phone apps. The value does not get stored anywhere, nor is it linked to any node parameters.
 *
 * @note This should be used only if some event requires explicitly alerting the user even when the
 * phone app is in background, not otherwise.
 * Eg. "Motion Detected", "Fire alarm triggered"
 *
 * @param[in] alert_str NULL terminated pre-formatted alert string.
 *     Maximum length can be ESP_RMAKER_MAX_ALERT_LEN, excluding NULL character.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_raise_alert(const char *alert_str);

/** Get parameter name from handle
 *
 * @param[in] param Parameter handle.
 *
 * @return NULL terminated parameter name string on success.
 * @return NULL in case of failure.
 */
char *esp_rmaker_param_get_name(const esp_rmaker_param_t *param);

/** Get parameter type from handle
 *
 * @param[in] param Parameter handle.
 *
 * @return NULL terminated parameter type string on success.
 * @return NULL in case of failure, or if the type wasn't provided while creating the parameter.
 */
char *esp_rmaker_param_get_type(const esp_rmaker_param_t *param);

/** Get parameter value
 *
 * This gives the parameter value that is stored in the RainMaker core.
 *
 * @note This does not call any explicit functions to read value from hardware/driver.
 *
 * @param[in] param Parameter handle
 *
 * @return Pointer to parameter value on success.
 * @return NULL in case of failure.
 */
esp_rmaker_param_val_t *esp_rmaker_param_get_val(esp_rmaker_param_t *param);

/** Report the node details to the cloud
 *
 * This API reports node details i.e. the node configuration and values of all the parameters to the ESP RainMaker cloud.
 * Eg. If a new device is created (with some parameters and attributes), then this API should be called after that
 * to send the node details to the cloud again and the changes to be reflected in the clients (like phone apps).
 *
 * @note Please use this API only if you need to create or delete devices after esp_rmaker_start() has already
 * been called, for use cases like bridges or hubs.
 *
 * @return ESP_OK if the node details are successfully queued to be published.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_report_node_details(void);

/** Enable Timezone Service
 *
 * This enables the ESP RainMaker standard timezone service which can be used to set
 * timezone, either in POSIX or location string format. Please refer the specifications
 * for additional details.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_timezone_service_enable(void);

/** Enable System Service
 *
 * This enables the ESP RainMaker standard system service which can be
 * used for operations like reboot, factory reset and Wi-Fi reset.
 *
 * Please refer the specifications for additional details.
 *
 * @param[in] config Configuration for the system service.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_system_service_enable(esp_rmaker_system_serv_config_t *config);

/**
 * Check if local_ctrl service has started
 *
 * @return true if service has started
 * @return false if the service has not started
 */
bool esp_rmaker_local_ctrl_service_started(void);

#ifdef __cplusplus
}
#endif
