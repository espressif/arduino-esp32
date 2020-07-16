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

#define MAX_VERSION_STRING_LEN  16

/** ESP RainMaker Event Base */
ESP_EVENT_DECLARE_BASE(RMAKER_EVENT);

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
    /** Node information \ref esp_rmaker_node_info_t */
    esp_rmaker_node_info_t info;
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

/**
 * Initialise a Boolean value
 *
 * @param[in] bval Initialising value
 *
 * @return Value structure
 */
esp_rmaker_param_val_t esp_rmaker_bool(bool bval);

/**
 * Initialise an Integer value
 *
 * @param[in] ival Initialising value
 *
 * @return Value structure
 */
esp_rmaker_param_val_t esp_rmaker_int(int ival);

/**
 * Initialise a Float value
 *
 * @param[in] fval Initialising value
 *
 * @return Value structure
 */
esp_rmaker_param_val_t esp_rmaker_float(float fval);

/**
 * Initialise a String value
 *
 * @param[in] sval Initialising value
 *
 * @return Value structure
 */
esp_rmaker_param_val_t esp_rmaker_str(const char *sval);

/** Initialize ESP RainMaker Agent
 *
 * This initializes the internal data required by ESP RainMaker agent and allocates memory as required.
 * This should be the first call before using any other ESP RainMaker API
 *
 * @param[in] config Configuration to be used by the ESP RainMaker.
 *
 * @return ESP_OK on success.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_init(esp_rmaker_config_t *config);

/** Start ESP RainMaker Agent
 *
 * This call starts the actual ESP RainMaker thread. This should preferably be called after a
 * successful Wi-Fi connection in order to avoid unnecessary failures.
 *
 * @return ESP_OK on success.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_start(void);

/** Stop ESP RainMaker Agent
 *
 * This call stops the ESP RainMaker Agent instance started earlier by esp_rmaker_start().
 *
 * @return ESP_OK on success.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_stop(void);

/** Get Node Id
 * 
 * Returns pointer to the NULL terminated Node ID string
 *
 * @return Pointer to a NULL terminated Node ID string.
 */
char *esp_rmaker_get_node_id(void);

/** Get Node Info
 *
 * Returns pointer to the node info as configured during initialisation
 *
 * @return Pointer to the node info on success.
 * @return NULL on failure.
 */
esp_rmaker_node_info_t *esp_rmaker_get_node_info(void);

/** Add Node attribute
 *
 * Adds a new attribute as the metadata for the node. For the sake of simplicity,
 * only string values are allowed.
 *
 * @param[in] attr_name Name of the attribute
 * @param[in] val Value for the attribute
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_node_add_attribute(const char *attr_name, const char *val);

/** Callback for parameter value changes
 *
 * @param[in] name Name of the parameters used while creating it
 * @param[in] param Pointer to \ref esp_rmaker_param_val_t. Use appropriate elements as per the value type
 * @param[in] priv_data Pointer to the private data passed while creating the parameter.
 *
 * @return ESP_OK on success. The agent will report the changed value to ESP RainMaker in this case
 * @return error in case of any error. No value will be reported back to the ESP RainMaker in this case
 */
typedef esp_err_t (*esp_rmaker_param_callback_t)(const char *name, const char *dev_name, esp_rmaker_param_val_t val, void *priv_data);

/**
 * Create a Device
 *
 * This API will create a virtual "Device" as part of the Node configuration.
 * This could be something like a Switch, Lightbulb, etc.
 * The mandatory parameter "Name" will also be added internally.
 *
 * @param[in] dev_name The unique device name
 * @param[in] type Optional device type. Can be kept NULL
 * @param[in] cb Callback to be invoked for "write" request for device parameters.
 * Can be kept NULL if no write requests are expected
 * @param[in] priv_data (Optional) Private data that will be passed to the callback. This should stay
 * allocated throughout the lifetime of the device
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_create_device(const char *dev_name, const char *type, esp_rmaker_param_callback_t cb, void *priv_data);

/** Add a Device attribute
 *
 * @note Device attributes are reported only once after a boot-up as part of the node
 * configuration.
 * Eg. Serial Number
 * 
 * @param[in] dev_name Name of the device
 * @param[in] attr_name Name of the attribute
 * @param[in] val Value of the attribute
 *
 * @return ESP_OK if the attribute was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_attribute(const char *dev_name, const char *attr_name, const char *val);

/**
 * Add a new parameter to a device
 *
 * Parameter can be something like Temperature, Outlet state, Lightbulb brightness, etc.
 *
 * Any changes should be reported to using the esp_rmaker_update_param() API.
 * Any remote changes will be reported to the application via the device callback, if registered.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Value of the parameter. This also specifies the type that will be assigned
 * to this parameter. You can use esp_rmaker_bool(), esp_rmaker_int(), esp_rmaker_float()
 * or esp_rmaker_str() functions as the argument here. Eg, esp_rmaker_bool(true).
 * @param[in] properties Properties of the parameter, which will be a logical OR of flags in
 *  \ref esp_param_property_flags_t
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_param(const char *dev_name, const char *param_name,
        esp_rmaker_param_val_t val, uint8_t properties);

/**
 * Create a Service
 *
 * This API will create a "Service" as part of the Node configuration.
 * A service could be something like OTA, diagnostics, etc.
 *
 * @note Name of a service should not clash with name of a device.
 *
 * @param[in] serv_name The unique service name
 * @param[in] type Optional service type. Can be kept NULL
 * @param[in] cb Callback to be invoked for "write" request for parameters.
 * Can be kept NULL if no write requests are expected
 * @param[in] priv_data (Optional) Private data that will be passed to the callback. This should stay
 * allocated throughout the lifetime of the service
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_create_service(const char *serv_name, const char *type, esp_rmaker_param_callback_t cb, void *priv_data);

/**
 * Add a new parameter to a service
 *
 * Parameter can be something like OTA URL, RSSI, etc.
 *
 * Any changes should be reported to using the esp_rmaker_update_param() API.
 * Any remote changes will be reported to the application via the device callback, if registered.
 *
 * @param[in] serv_name Name of the service
 * @param[in] param_name Name of the parameter
 * @param[in] val Value of the parameter. This also specifies the type that will be assigned
 * to this parameter. You can use esp_rmaker_bool(), esp_rmaker_int(), esp_rmaker_float()
 * or esp_rmaker_str() functions as the argument here. Eg, esp_rmaker_bool(true).
 * @param[in] properties Properties of the parameter, which will be a logical OR of flags in
 *  \ref esp_param_property_flags_t
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_service_add_param(const char *serv_name, const char *param_name,
        esp_rmaker_param_val_t val, uint8_t properties);

/**
 * Add a UI Type to a parameter
 *
 * This will be used by the Phone apps (or other clients) to render appropriate UI for the given
 * parameter. Please refer the RainMaker documetation for supported UI Types.
 *
 * @param[in] dev_name Name of the device, in which the parameter is present.
 * @param[in] name Name of the parameter
 * @param[in] ui_type String describing the UI Type
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_param_add_ui_type(const char *dev_name, const char *name, const char *ui_type);

/**
 * Add bounds for an integer/float parameter
 *
 * This can be used to add bounds (min/max values) for a given integer parameter. Eg. brightness
 * will have bounds as 0 and 100 if it is a percentage.
 * Eg. esp_rmaker_param_add_bounds("Light", "brightness", esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(5));
 *
 * @param[in] dev_name Name of the device, in which the parameter is present.
 * @param[in] param_name Name of the parameter
 * @param[in] min Minimum allowed value
 * @param[in] max Maximum allowed value
 * @param[in] step Minimum stepping (set to 0 if no specific value is desired)
 *
 * @return ESP_OK on success.
 * return error in case of any error.
 */
esp_err_t esp_rmaker_param_add_bounds(const char *dev_name, const char *param_name,
    esp_rmaker_param_val_t min, esp_rmaker_param_val_t max, esp_rmaker_param_val_t step);

/**
 * Add an optional type to a parameter
 *
 * Useful if you want the clients (like phone apps) to treat a parameter in a different way.
 * Eg. esp.type.brightness as a type can automatically tell a phone app that this is an integer
 * type with a percentage value and can also show a custom UI for brightness.
 *
 * @param[in] dev_name Name of the device, in which the parameter is present.
 * @param[in] param_name Name of the parameter
 * @param[in] type Desired type string
 *
 * @return ESP_OK on success.
 * return error in case of any error.
 */
esp_err_t esp_rmaker_param_add_type(const char *dev_name, const char *param_name, const char* type);

/** Update a parameter
 *
 * Calling this API will update the parameter and report it to ESP RainMaker cloud.
 * This should be used whenever there is any local change.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val New value of the parameter
 *
 * @return ESP_OK if the parameter was updated successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_update_param(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val);

/** Assign a primary parameter
 *
 * Assign a parameter (already added using esp_rmaker_device_add_param()) as a primary parameter,
 * which can be used by clients (phone apps specifically) to give prominence to it.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 *
 * @return ESP_OK if the parameter was assigned as the primary successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_assign_primary_param(const char *dev_name, const char *param_name);

/** Prototype for ESP RainMaker Work Queue Function
 *
 * @param[in] priv_data The private data associated with the work function
 */
typedef void (*esp_rmaker_work_fn_t)(void *priv_data);

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
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_report_node_details();

/** Queue execution of a function in ESP RainMaker's context
 *
 * This API queues a work function for execution in the ESP RainMaker Task's context.
 *
 * @param[in] work_fn The Work function to be queued
 * @param[in] priv_data Private data to be passed to the work function
 *
 * @return ESP_OK on success.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_queue_work(esp_rmaker_work_fn_t work_fn, void *priv_data);

#ifdef __cplusplus
}
#endif
