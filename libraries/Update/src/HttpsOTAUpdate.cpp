/* OTA task

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <functional>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "esp32-hal-log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_netif.h"

#include "HttpsOTAUpdate.h"
#define OTA_TASK_STACK_SIZE 9216

typedef void (*HttpEventCb)(HttpEvent_t*);

static esp_http_client_config_t config; 
static HttpEventCb cb;
static EventGroupHandle_t ota_status = NULL;//check for ota status
static EventBits_t set_bit;
static ifreq freq;

const int OTA_IDLE_BIT = BIT0;
const int OTA_UPDATING_BIT = BIT1;
const int OTA_SUCCESS_BIT = BIT2;
const int OTA_FAIL_BIT = BIT3;

esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    // OTA update will not be stopped if the callback wasn't set,
    // because we return ESP_OK either way.
    if (cb != nullptr) {
        cb(event);
    }
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

void HttpsOTAUpdateClass::begin(const char *url, const char *cert_pem, UpdateNetworkInterface network_interface, bool skip_cert_common_name_check)
{
    config.url = url;
    config.cert_pem = cert_pem; 
    config.skip_cert_common_name_check = skip_cert_common_name_check;
    config.event_handler = http_event_handler;

    esp_netif_inherent_config_t network_interface_config;
    get_netif_config_from_type(network_interface, network_interface_config);
    const esp_err_t error = get_bound_interface_name(network_interface_config.if_key, freq);
    config.if_name = error != ESP_OK ? nullptr : &freq;

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

esp_err_t HttpsOTAUpdateClass::get_netif_config_from_type(UpdateNetworkInterface network_interface, esp_netif_inherent_config_t& netif_inherent_config)
{
    esp_err_t error = ESP_OK;
    switch (network_interface) {
        case UpdateNetworkInterface::DEFAULT_INTERFACE:
            error = ESP_ERR_ESP_NETIF_IF_NOT_READY;
            break;
        case UpdateNetworkInterface::WIFI_STA:
            netif_inherent_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
            break;
        case UpdateNetworkInterface::WIFI_AP:
            netif_inherent_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_AP();
            break;
        case UpdateNetworkInterface::ETHERNET:
            netif_inherent_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
            break;
        case UpdateNetworkInterface::POINT_TO_POINT:
            netif_inherent_config = ESP_NETIF_INHERENT_DEFAULT_PPP();
            break;
        case UpdateNetworkInterface::SERIAL_LINE:
            netif_inherent_config = ESP_NETIF_INHERENT_DEFAULT_SLIP();
            break;
        default:
            error = ESP_ERR_ESP_NETIF_IF_NOT_READY;
            log_e("Unexpected UpdateNetworkInterface argument (%u).", static_cast<std::underlying_type<UpdateNetworkInterface>::type>(network_interface));
            break;
    }
    return error;
}

esp_err_t HttpsOTAUpdateClass::get_bound_interface_name(const char* is_key, ifreq& freq)
{
    if (is_key == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    log_d("Trying to get network interface with key (%s).", is_key);

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey(is_key);
    if (netif == nullptr) {
        log_e("Attempting to get network interface returned NULL.");
        return ESP_ERR_ESP_NETIF_IF_NOT_READY;
    }

    const esp_err_t error = esp_netif_get_netif_impl_name(netif, freq.ifr_name);
    if (error != ESP_OK) {
        log_e("Error (%s) while getting bound interface name.", esp_err_to_name(error));
        return error;
    }

    log_d("Bound interface name is (%s).", freq.ifr_name);
    return error;
}

HttpsOTAUpdateClass HttpsOTA;
