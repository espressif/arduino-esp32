/* OTA task

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

#include "HttpsOTAUpdate.h"
#include "Esp.h"
#define OTA_TASK_STACK_SIZE 9216

typedef void (*HttpEventCb)(HttpEvent_t*);

static esp_http_client_config_t config; 
static HttpEventCb cb;
static EventGroupHandle_t ota_status = NULL;//check for ota status
static EventBits_t set_bit;

const int OTA_IDLE_BIT = BIT0;
const int OTA_UPDATING_BIT = BIT1;
const int OTA_SUCCESS_BIT = BIT2;
const int OTA_FAIL_BIT = BIT3;

esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    cb(event); 
    return ESP_OK;
}

void https_ota_task(void *param)
{
    if(ota_status) {
        xEventGroupSetBits(ota_status, OTA_UPDATING_BIT);
        xEventGroupClearBits(ota_status, OTA_IDLE_BIT);
    }
    esp_err_t ret = esp_https_ota((const esp_http_client_config_t *)param);
    if(ret == ESP_OK) {
        if(ota_status) {
            xEventGroupClearBits(ota_status, OTA_UPDATING_BIT);
            xEventGroupSetBits(ota_status, OTA_SUCCESS_BIT);
        }
    } else {
        if(ota_status) {  
            xEventGroupClearBits(ota_status, OTA_UPDATING_BIT);
            xEventGroupSetBits(ota_status, OTA_FAIL_BIT);
        }
    }
    vTaskDelete(NULL);
}

HttpsOTAStatus_t HttpsOTAUpdateClass::status()
{
    if(ota_status) {     
        set_bit =  xEventGroupGetBits(ota_status);
        if(set_bit == OTA_IDLE_BIT) {
            return HTTPS_OTA_IDLE;
        }
        if(set_bit == OTA_UPDATING_BIT) {
            return HTTPS_OTA_UPDATING;
        }
        if(set_bit == OTA_SUCCESS_BIT) {
            return HTTPS_OTA_SUCCESS;
        }
        if(set_bit == OTA_FAIL_BIT) {
            return HTTPS_OTA_FAIL;
        }
    }
    return HTTPS_OTA_ERR;
}

void HttpsOTAUpdateClass::onHttpEvent(HttpEventCb cbEvent)
{
    cb = cbEvent;
}

void HttpsOTAUpdateClass::begin(const char *url, const char *cert_pem, bool skip_cert_common_name_check)
{
    config.url = url;
    config.cert_pem = cert_pem; 
    config.skip_cert_common_name_check = skip_cert_common_name_check;
    config.event_handler = http_event_handler;

    if(!ota_status) {
        ota_status = xEventGroupCreate();
        if(!ota_status) {
            log_e("OTA Event Group Create Failed");
        }
        xEventGroupSetBits(ota_status, OTA_IDLE_BIT);
    }
 
    if (xTaskCreate(&https_ota_task, "https_ota_task", OTA_TASK_STACK_SIZE, &config, 5, NULL) != pdPASS) {
        log_e("Couldn't create ota task\n"); 
    }
}

HttpsOTAUpdateClass HttpsOTA;
