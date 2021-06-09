/* Wi-Fi FTM Initiator Arduino Example

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
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_wifi.h"

/*
 * THIS FEATURE IS SUPPORTED ONLY BY ESP32-S2 AND ESP32-C3
 */

// Change the SSID and PASSWORD here if needed
#define WIFI_FTM_SSID           "WiFi_FTM_Responder" // SSID
#define WIFI_FTM_PASS           "ftm_responder" // Password
// FTM settings (minimal)
//Num of FTM frames requested in terms of 4 or 8 bursts (allowed values - 0 (No pref), 16, 24, 32, 64)
#define FTM_FRAME_COUNT         32
// Requested time period between consecutive FTM bursts in 100’s of milliseconds
#define FTM_BURST_PERIOD        2

static bool s_reconnect = true;
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;

static EventGroupHandle_t ftm_event_group;
const int FTM_REPORT_BIT = BIT0;
const int FTM_FAILURE_BIT = BIT1;
static uint32_t g_rtt_est, g_dist_est;

/* Handler for the Wi-Fi connection to the Responder */ 
static void wifi_connected_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;

    Serial.print("Connected to ");
    Serial.println(WIFI_FTM_SSID);
    
    xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
}

/* Wi-Fi disconnect handler */
static void wifi_disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (s_reconnect) {
        Serial.println("STA disconnect, s_reconnect...");
        esp_wifi_connect();
    } else {
        Serial.println("STA disconnect");
    }

    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
}

/* FTM report handler with the calculated data from the round trip */
static void ftm_report_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_ftm_report_t *event = (wifi_event_ftm_report_t *) event_data;

    if (event->status == FTM_STATUS_SUCCESS) {
        g_rtt_est = event->rtt_est;
        g_dist_est = event->dist_est;
        xEventGroupSetBits(ftm_event_group, FTM_REPORT_BIT);
    } else {
        Serial.print("FTM procedure with Peer failed! Status: ");
        Serial.println(event->status);
        xEventGroupSetBits(ftm_event_group, FTM_FAILURE_BIT);
    }
}

// Wi-Fi initialization with FTM handler register
void wifi_init(void)
{
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    static bool initialized = false;

    if (initialized) {
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ftm_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_CONNECTED,
                    &wifi_connected_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_DISCONNECTED,
                    &wifi_disconnect_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_FTM_REPORT,
                    &ftm_report_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    initialized = true;
}

/*  */
static bool wifi_connect(const char *ssid, const char *pass)
{
    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);

    wifi_config_t wifi_config = { 0 };

    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password,
                         pass,
                         sizeof(wifi_config.sta.password));
    }

    if (bits & CONNECTED_BIT) {
        s_reconnect = false;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 0, 1, portTICK_RATE_MS);
    }

    s_reconnect = true;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    Serial.println("STA connecting...");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 5000 / portTICK_RATE_MS);
    return true;
}

static int app_get_ftm(void)
{
    wifi_ftm_initiator_cfg_t ftmi_cfg = {
        .frm_count = FTM_FRAME_COUNT,
        .burst_period = FTM_BURST_PERIOD,
    };

    Serial.print("Requesting FTM session with Frm Count - ");
    Serial.print(ftmi_cfg.frm_count);
    Serial.print(" Burst Period ");
    Serial.println(ftmi_cfg.burst_period*100);
    
    // Request FTM session with the Responder
    if (ESP_OK != esp_wifi_ftm_initiate_session(&ftmi_cfg)) {
        Serial.println("Failed to start FTM session");
        return 0;
    }

    EventBits_t bits = xEventGroupWaitBits(ftm_event_group, FTM_REPORT_BIT | FTM_FAILURE_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    /* Processing data from FTM session */
    if (bits & FTM_REPORT_BIT) {
        /* The estimated distance in meters may vary depending on some factors (see README file) */
        Serial.print("Estimated RTT (ns) ");
        Serial.print(g_rtt_est); // Estimated round trip time
        Serial.print(" Estimated Distance (in meters) ");
        Serial.print(g_dist_est / 100); // Estimated distance in meters
        Serial.print(".");
        Serial.println(g_dist_est % 100); // Estimated distance in meters
        xEventGroupClearBits(ftm_event_group, FTM_REPORT_BIT);
    } else {
        Serial.print("Error getting FTM!");
    }
    return 0;
}

void setup() {

    Serial.begin(115200);
    Serial.println("Starting the FTM Initiator");

    // NVS initialization for the Wi-Fi (needed!)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // Wi-Fi Initialization
    wifi_init();
    // Connect to STA with FTM Enabled
    wifi_connect(WIFI_FTM_SSID, WIFI_FTM_PASS);
    // Get FTM first time
    app_get_ftm();
}

void loop() {

}
