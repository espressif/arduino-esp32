#include "RMakerWiFi.h"
#include "WiFi.h"

#include <esp_rmaker_user_mapping.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <esp_wifi.h>

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

static void get_device_pop(char *pop, size_t max)
{
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(pop, max, "%02x%02x%02x%02x", eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);
}

void RMakerWiFiClass::wifiProvision()
{
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    wifi_prov_mgr_init(config);
    bool provisioned = false;
    WiFi.mode(WIFI_MODE_AP);
    wifi_prov_mgr_is_provisioned(&provisioned);
    if(provisioned == false) {
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        char pop[15];
        get_device_pop(pop, sizeof(pop));
        const char *service_key = NULL;
        uint8_t custom_service_uuid[] = {
            /* This is a random uuid. This can be modified if you want to change the BLE uuid. */
            /* 12th and 13th bit will be replaced by internal bits. */
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
        esp_rmaker_user_mapping_endpoint_create();
        wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key);
        esp_rmaker_user_mapping_endpoint_register();
        log_i("Starting AP using BLE\n service_name : %s\n pop : %s",service_name,pop);
    } else {
        wifi_prov_mgr_deinit();
        WiFi.mode(WIFI_MODE_STA);
        WiFi.begin();
    } 
}
