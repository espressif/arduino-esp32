/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize console
 *
 * Initializes serial console and adds basic commands.
 *
 * @return ESP_OK on success.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_common_console_init(void);

/* Reference for adding custom console commands:
#include <esp_console.h>

static int command_console_handler(int argc, char *argv[])
{
    // Command code here
}

static void register_console_command()
{
    const esp_console_cmd_t cmd = {
        .command = "<command_name>",
        .help = "<help_details>",
        .func = &command_console_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
*/

#ifdef __cplusplus
}
#endif
