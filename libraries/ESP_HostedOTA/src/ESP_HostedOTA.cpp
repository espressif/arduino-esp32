// Include necessary headers for SDK configuration, Arduino framework, and networking
#include "sdkconfig.h"
#include <stdbool.h>
#if CONFIG_ESP_WIFI_REMOTE_ENABLED
#include "Arduino.h"
#include "esp32-hal-hosted.h"  // ESP-Hosted specific functions
#include "Network.h"           // Network connectivity management
#include "HTTPClient.h"        // HTTP client for downloading updates
#include "NetworkClientSecure.h" // Secure network client for HTTPS
#endif

/**
 * Updates the ESP-Hosted co-processor firmware over-the-air (OTA)
 * This function downloads and installs firmware updates for the ESP-Hosted slave device
 * @return true if update was successful, false otherwise
 */
bool updateEspHostedSlave() {
#if CONFIG_ESP_WIFI_REMOTE_ENABLED
  bool updateSuccess = false;
  
  // Step 1: Verify ESP-Hosted is properly initialized
  if (!hostedIsInitialized()) {
    Serial.println("ERROR: esp-hosted is not initialized. Did you call WiFi.STA.begin()?");
    return updateSuccess;
  }
  
  // Step 2: Check if an update is actually available
  if (!hostedHasUpdate()) {
    // esp-hosted is already the latest version - no update needed
    return updateSuccess;
  }
  
  // Step 3: Ensure network connectivity is available
  if (!Network.isOnline()) {
    Serial.println("ERROR: Network is not online! Did you call WiFi.STA.connect(ssid, password)?");
    return updateSuccess;
  }
  
  // Step 4: Begin the update process - display update URL
  Serial.print("Updating esp-hosted co-processor from ");
  Serial.println(hostedGetUpdateURL());
  
  // Step 5: Create a secure network client for HTTPS communication
  NetworkClientSecure *client = new NetworkClientSecure();
  if (!client) {
    Serial.println("ERROR: Could not allocate client!");
    return updateSuccess;
  }
  
  // Step 6: Configure client to skip certificate verification (insecure mode)
  client->setInsecure();
  
  // Step 7: Initialize HTTP client and attempt to connect to update server
  HTTPClient https;
  int httpCode = 0;
  if (!https.begin(*client, hostedGetUpdateURL())) {
    Serial.println("ERROR: HTTP begin failed!");
    goto finish_ota;
  }
  
  // Step 8: Send HTTP GET request to download the firmware
  httpCode = https.GET();
  if (httpCode == HTTP_CODE_OK) {
    // Step 9: Get the size of the firmware file to download
    int len = https.getSize();
    if (len < 0) {
      Serial.println("ERROR: Update size not received!");
      https.end();
      goto finish_ota;
    }
    
    // Step 10: Get stream pointer for reading firmware data
    NetworkClient *stream = https.getStreamPtr();
    
    // Step 11: Initialize the ESP-Hosted update process
    if (!hostedBeginUpdate()) {
      Serial.println("ERROR: esp-hosted update start failed!");
      https.end();
      goto finish_ota;
    }
    
    // Step 12: Allocate buffer for firmware data transfer (2KB chunks)
    #define HOSTED_OTA_BUF_SIZE 2048
    uint8_t * buff = (uint8_t*)malloc(HOSTED_OTA_BUF_SIZE);
    if (!buff) {
      Serial.println("ERROR: Could not allocate OTA buffer!");
      https.end();
      goto finish_ota;
    }
    
    // Step 13: Download and write firmware data in chunks
    while (https.connected() && len > 0) {
      size_t size = stream->available();
      if (size > 0) {
        // Show progress indicator
        Serial.print(".");
        
        // Limit chunk size to buffer capacity
        if (size > HOSTED_OTA_BUF_SIZE) {
          size = HOSTED_OTA_BUF_SIZE;
        }
        
        // Prevent reading more data than expected
        if (size > len) {
          Serial.printf("\nERROR: Update received extra bytes: %u!", size - len);
          break;
        }
        
        // Read firmware data chunk into buffer
        int readLen = stream->readBytes(buff, size);
        len -= readLen;
        
        // Write the chunk to ESP-Hosted co-processor
        if (!hostedWriteUpdate(buff, readLen)) {
          Serial.println("\nERROR: esp-hosted update write failed!");
          break;
        }
        
        // Step 14: Check if entire firmware has been downloaded
        if (len == 0) {
          // Finalize the update process
          if (!hostedEndUpdate()) {
            Serial.println("\nERROR: esp-hosted update end failed!");
            break;
          }
          
          // Activate the new firmware
          if (!hostedActivateUpdate()) {
            Serial.println("\nERROR: esp-hosted update activate failed!");
            break;
          }
          
          // Update completed successfully
          updateSuccess = true;
          Serial.println("\nSUCCESS: esp-hosted co-processor updated!");
          break;
        }
      }
      // Small delay to prevent overwhelming the system
      delay(1);
    }
    
    // Step 15: Clean up allocated buffer
    free(buff);
    Serial.println();
  }

  // Step 16: Close HTTP connection
  https.end();
  
finish_ota:
  // Step 17: Clean up network client
  delete client;
  return updateSuccess;
#else
  // ESP-Hosted functionality is not enabled in SDK configuration
  return false;
#endif
}
