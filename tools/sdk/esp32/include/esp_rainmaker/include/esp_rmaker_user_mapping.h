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
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Create User Mapping Endpoint
 *
 * This will create a custom provisioning endpoint for user-node mapping.
 * This should be called after wifi_prov_mgr_init() but before
 * wifi_prov_mgr_start_provisioning()
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_user_mapping_endpoint_create(void);

/**
 * Register User Mapping Endpoint
 *
 * This will register the callback for the custom provisioning endpoint
 * for user-node mapping which was created with esp_rmaker_user_mapping_endpoint_create().
 * This should be called immediately after wifi_prov_mgr_start_provisioning().
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_user_mapping_endpoint_register(void);

/** Add User-Node mapping
 *
 * This call will start the user-node mapping workflow on the node.
 * This is automatically called if you have used esp_rmaker_user_mapping_endpoint_register().
 * Use this API only if you want to trigger the user-node mapping after the Wi-Fi provisioning
 * has already been done.
 *
 * @param[in] user_id The User identifier received from the client (Phone app/CLI)
 * @param[in] secret_key The Secret key received from the client (Phone app/CLI)
 *
 * @return ESP_OK if the workflow was successfully triggered. This does not guarantee success
 * of the actual mapping. The mapping status needs to be checked separately by the clients.
 * @return error on failure.
 */
esp_err_t esp_rmaker_start_user_node_mapping(char *user_id, char *secret_key);

#ifdef __cplusplus
}
#endif
