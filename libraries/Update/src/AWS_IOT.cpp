/* Cloud task

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp32-hal-log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include"AWS_IOT.h"
#include"Update.h"

const char * server_cert = NULL;
const char * ota_url = NULL;

void aws_iot_task(void *param)
{
    log_i("Starting OTA example");
    log_i("Connected to WiFi network! Attempting to connect to server...");

    esp_http_client_config_t config;
    config.url = ota_url;
    config.cert_pem = server_cert;

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        ESP.restart();
    } else {
        log_e("Firmware upgrade failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

int AWS_IOT::cloud_start(const char *cert, const char *url)
{
    server_cert = cert;
    ota_url = url;
    if (xTaskCreate(&aws_iot_task, "aws_iot_task", 9216, NULL, 5, NULL) != pdPASS) {
        log_e("Couldn't create cloud task\n");
    }
    return ESP_OK;
}

AWS_IOT aws;
