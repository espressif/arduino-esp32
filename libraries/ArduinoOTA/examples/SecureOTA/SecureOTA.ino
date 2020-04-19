#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include "ArduinoOTA.h"
#include "UpdateProcessor.h"
#include "UpdateProcessorRFC3161.h"

#include "hardcoded-roots.h"

UpdateProcessorRFC3161 rfcChecker = UpdateProcessorRFC3161();

void setup() {
  Serial.begin(115200);
  Serial.println("Booting " __DATE__ " " __TIME__);

  WiFi.mode(WIFI_STA);
  WiFi.begin("wifi-network", "password");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // ArduinoOTA.setPort(3232);
  // ArduinoOTA.setHostname("some-name");

  // No authentication by default
  // ArduinoOTA.setPassword(OTA_PASSWD);

  // Specify a (root) signature we trust during
  // updates.
  rfcChecker.addTrustedCertAsDER(ca_interop_redwax_der, ca_interop_redwax_der_len);
 
  // Allow unsiged uploads: 
  // rfcChecker.setAllowLegacyUploads(true); // default is not to allow this.

  // Wire this check into the normal upload process.
  //
  ArduinoOTA.setProcessor(&rfcChecker);

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Updating " + type);
  })
  .onEnd([]() {
    Serial.println(" Ok, completed without errors.");
  })
  .onError([](ota_error_t error) {
    Serial.printf("\nAborted with error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.approveReboot([]() {
    Serial.println("Reboot ok");
    return true;
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ArduinoOTA.handle();
}

