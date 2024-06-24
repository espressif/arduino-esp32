/*
 * OpenThread.begin() will automatically start a node in a Thread Network
 * If NVS is empty, default configuration will be as follow:
 *
 *   NETWORK_NAME "OpenThread-ESP"
 *   MESH_LOCAL_PREFIX "fd00:db8:a0:0::/64"
 *   NETWORK_CHANNEL 15
 *   NETWORK_PANID 0x1234
 *   NETWORK_EXTPANID "dead00beef00cafe"
 *   NETWORK_KEY "00112233445566778899aabbccddeeff"
 *   NETWORK_PSKC "104810e2315100afd6bc9215a6bfac53"
 *
 * If NVS has already a dataset information, it will load it from there.
 */

#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

// The first device to start Thread will be the Leader
// Next devices will be Router or Child

void setup() {
  Serial.begin(115200);
  OThreadCLI.begin();                 // AutoStart using Thread default settings
  otPrintNetworkInformation(Serial);  // Print Current Thread Network Information
}

void loop() {
  Serial.print("Thread Node State: ");
  Serial.println(otGetStringDeviceRole());
  delay(5000);
}
