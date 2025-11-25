#include "sdkconfig.h"
#include <stdbool.h>
#if CONFIG_ESP_WIFI_REMOTE_ENABLED
#include "Arduino.h"
#include "esp32-hal-hosted.h"
#include "Network.h"
#include "HTTPClient.h"
#include "NetworkClientSecure.h"
#endif

bool updateEspHostedSlave() {
#if CONFIG_ESP_WIFI_REMOTE_ENABLED
  bool updateSuccess = false;
  if (!hostedIsInitialized()) {
    Serial.println("ERROR: esp-hosted is not initialized. Did you call WiFi.STA.begin()?");
    return updateSuccess;
  }
  if (!hostedHasUpdate()) {
    // esp-hosted is already the latest version
    return updateSuccess;
  }
  if (!Network.isOnline()) {
    Serial.println("ERROR: Network is not online! Did you call WiFi.STA.connect(ssid, password)?");
    return updateSuccess;
  }
  Serial.print("Updating esp-hosted co-processor from ");
  Serial.println(hostedGetUpdateURL());
  NetworkClientSecure *client = new NetworkClientSecure();
  if (!client) {
    Serial.println("ERROR: Could not allocate client!");
    return updateSuccess;
  }
  client->setInsecure();
  HTTPClient https;
  int httpCode = 0;
  if (!https.begin(*client, hostedGetUpdateURL())) {
    Serial.println("ERROR: HTTP begin failed!");
    goto finish_ota;
  }
  httpCode = https.GET();
  if (httpCode == HTTP_CODE_OK) {
    int len = https.getSize();
    if (len < 0) {
      Serial.println("ERROR: Update size not received!");
      https.end();
      goto finish_ota;
    }
    NetworkClient *stream = https.getStreamPtr();
    if (!hostedBeginUpdate()) {
      Serial.println("ERROR: esp-hosted update start failed!");
      https.end();
      goto finish_ota;
    }
    #define HOSTED_OTA_BUF_SIZE 2048
    uint8_t * buff = (uint8_t*)malloc(HOSTED_OTA_BUF_SIZE);
    if (!buff) {
      Serial.println("ERROR: Could not allocate OTA buffer!");
      https.end();
      goto finish_ota;
    }
    while (https.connected() && len > 0) {
      size_t size = stream->available();
      if (size > 0) {
        Serial.print(".");
        if (size > HOSTED_OTA_BUF_SIZE) {
          size = HOSTED_OTA_BUF_SIZE;
        }
        if (size > len) {
          Serial.printf("\nERROR: Update received extra bytes: %u!", size - len);
          break;
        }
        int readLen = stream->readBytes(buff, size);
        len -= readLen;
        if (!hostedWriteUpdate(buff, readLen)) {
          Serial.println("\nERROR: esp-hosted update write failed!");
          break;
        }
        if (len == 0) {
          if (!hostedEndUpdate()) {
            Serial.println("\nERROR: esp-hosted update end failed!");
            break;
          }
          if (!hostedActivateUpdate()) {
            Serial.println("\nERROR: esp-hosted update activate failed!");
            break;
          }
          updateSuccess = true;
          Serial.println("\nSUCCESS: esp-hosted co-processor updated!");
          break;
        }
      }
      delay(1);
    }
    free(buff);
    Serial.println();
  }

  https.end();
finish_ota:
  delete client;
  return updateSuccess;
#else
  return false;
#endif
}
