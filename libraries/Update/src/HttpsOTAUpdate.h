#ifndef HTTPSOTAUPDATE_H
#define HTTPSOTAUPDATE_H
#include "esp_http_client.h"
#include "esp_netif_types.h"
#include "UpdateNetworkInterface.h"
#define HttpEvent_t esp_http_client_event_t

typedef enum
{
    HTTPS_OTA_IDLE,
    HTTPS_OTA_UPDATING,
    HTTPS_OTA_SUCCESS,
    HTTPS_OTA_FAIL,
    HTTPS_OTA_ERR
}HttpsOTAStatus_t;

class HttpsOTAUpdateClass {

    public:
    void begin(const char *url, const char *cert_pem, UpdateNetworkInterface network_interface = UpdateNetworkInterface::DEFAULT_INTERFACE, bool skip_cert_common_name_check = false);
    void onHttpEvent(void (*http_event_cb_t)(HttpEvent_t *));
    HttpsOTAStatus_t status();

    private:
    esp_err_t get_netif_config_from_type(UpdateNetworkInterface network_interface, esp_netif_inherent_config_t& netif_inherent_config);
    esp_err_t get_bound_interface_name(const char* is_key, ifreq& freq);
};

extern HttpsOTAUpdateClass HttpsOTA;
#endif
