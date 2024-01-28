#include "RMakerQR.h"
#ifdef CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK
void printQR(const char *name, const char *pop, const char *transport) {
  if (!name || !pop || !transport) {
    log_w("Cannot generate QR code payload. Data missing.");
    return;
  }
  char payload[150] = { 0 };
  snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\""
                                     ",\"pop\":\"%s\",\"transport\":\"%s\"}",
           PROV_QR_VERSION, name, pop, transport);
  if (Serial) {
    Serial.printf("Scan this QR code from the ESP RainMaker phone app.\n");
  }
  //qrcode_display(payload);  // deprecated!
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  esp_qrcode_generate(&cfg, payload);
  if (Serial) {
    Serial.printf("If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s\n", QRCODE_BASE_URL, payload);
  }
}
#endif
