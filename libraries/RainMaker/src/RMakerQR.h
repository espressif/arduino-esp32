#include "esp_system.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_ESP_RMAKER_TASK_STACK && CONFIG_IDF_TARGET_ESP32

#include <qrcode.h>

#define PROV_QR_VERSION "v1"
#define QRCODE_BASE_URL     "https://rainmaker.espressif.com/qrcode.html"

static void printQR(const char *name, const char *pop, const char *transport)
{
    if (!name || !pop || !transport) {
        log_w("Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150];
    snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"pop\":\"%s\",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, name, pop, transport);
    Serial.printf("Scan this QR code from the ESP RainMaker phone app.\n");
    qrcode_display(payload);
    Serial.printf("If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s\n", QRCODE_BASE_URL, payload);
}

#endif
