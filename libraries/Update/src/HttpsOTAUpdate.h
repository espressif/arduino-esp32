#include "esp_http_client.h"
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
    void begin(const char *url, const char *cert_pem, bool skip_cert_common_name_check = true);
    void onHttpEvent(void (*http_event_cb_t)(HttpEvent_t *));
    HttpsOTAStatus_t status();
};

extern HttpsOTAUpdateClass HttpsOTA;
