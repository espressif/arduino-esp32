#include "esp_system.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_IDF_TARGET_ESP32

#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_err.h>
#include <esp32-hal.h>

typedef esp_rmaker_node_t* node_t;
typedef esp_rmaker_node_info_t node_info_t;
typedef esp_rmaker_param_val_t param_val_t;
typedef esp_rmaker_write_ctx_t write_ctx_t;
typedef esp_rmaker_read_ctx_t read_ctx_t;
typedef esp_rmaker_device_t device_handle_t;
typedef esp_rmaker_param_t param_handle_t;
typedef esp_rmaker_ota_type_t ota_type_t;

param_val_t value(int);
param_val_t value(bool);
param_val_t value(char *);
param_val_t value(float);

#endif