/* Wi-Fi FTM Responder Arduino Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_wifi.h"

// Change the SSID and PASSWORD here if needed
#define WIFI_FTM_SSID           "WiFi_FTM_Responder"
#define WIFI_FTM_PASS           "ftm_responder"

static bool s_reconnect = true;

void initialise_wifi(void)
{
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    static bool initialized = false;

    if (initialized) {
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    initialized = true;
}

static bool start_ftm_responder(const char* ssid, const char* pass)
{

    wifi_config_t wifi_config_resp;
    
    wifi_config_resp.ap.ssid_len = 0;
    wifi_config_resp.ap.max_connection = 4;
    wifi_config_resp.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config_resp.ap.ftm_responder = true;

    s_reconnect = false;
    strlcpy((char*) wifi_config_resp.ap.ssid, ssid, sizeof(wifi_config_resp.ap.ssid));
    if (pass) {
        if (strlen(pass) != 0 && strlen(pass) < 8) {
            s_reconnect = true;
            Serial.println("Password less than 8 chars!");
            return false;
        }
        strlcpy((char*) wifi_config_resp.ap.password, pass, sizeof(wifi_config_resp.ap.password));
    }

    if (strlen(pass) == 0) {
        wifi_config_resp.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_AP, &wifi_config_resp));
    Serial.println("Starting SoftAP with FTM Responder support.");
    return true;
}

void setup() {

    Serial.begin(115200);
    Serial.println("Starting the FTM Responder");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    // WiFi initialization with default configuration
    initialise_wifi();
    // Start the AP with FTM Responder capability
    start_ftm_responder(WIFI_FTM_SSID, WIFI_FTM_PASS);
}

void loop() {
  // put your main code here, to run repeatedly:
}
