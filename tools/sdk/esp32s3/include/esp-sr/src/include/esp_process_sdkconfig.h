#pragma once
#include "esp_err.h"
#include "esp_mn_iface.h"

/**
 * @brief Check chip config to ensure optimum performance
 */
void check_chip_config(void);

/**
 * @brief Update the speech commands of MultiNet by menuconfig
 *
 * @param multinet            The multinet handle
 *
 * @param model_data          The model object to query
 *
 * @param langugae            The language of MultiNet
 *
 * @return
 *     - ESP_OK                  Success
 *     - ESP_ERR_INVALID_STATE   Fail
 */
esp_mn_error_t* esp_mn_commands_update_from_sdkconfig(const esp_mn_iface_t *multinet, model_iface_data_t *model_data);
