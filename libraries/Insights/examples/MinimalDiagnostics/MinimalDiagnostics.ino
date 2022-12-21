#include "Insights.h"
#include "Metrics.h"
#include "WiFi.h"

const char insights_auth_key[] = "<ENTER YOUR AUTH KEY>";

#define WIFI_SSID       "<ENTER YOUR SSID>"
#define WIFI_PASSPHRASE "<ENTER YOUR PASSWORD>"

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    esp_err_t ret = Insights.init(insights_auth_key, ESP_DIAG_LOG_TYPE_ERROR | ESP_DIAG_LOG_TYPE_WARNING | ESP_DIAG_LOG_TYPE_EVENT);
    if(ret != ESP_OK) {
        ESP.restart();
    }
}

void loop()
{
    Metrics.dumpHeapMetrics();
    Metrics.dumpWiFiMetrics();
    delay(10 * 60 * 1000);
}
