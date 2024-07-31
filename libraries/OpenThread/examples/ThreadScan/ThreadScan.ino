/*
   OpenThread.begin(true) will automatically start a node in a Thread Network
   Full scanning requires the thread node to be at least in Child state.

   This will scan the IEEE 802.14.5 devices in the local area using CLI "scan" command
   As soon as this device turns into a Child, Router or Leader, it will be able to
   scan for Local Thread Networks as well.

*/

#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

void setup() {
  Serial.begin(115200);
  OThreadCLI.begin(true);      // For scanning, AutoStart must be active, any setup
  OThreadCLI.setTimeout(100);  // Set a timeout for the CLI response
  Serial.println();
  Serial.println("This sketch will continuously scan the Thread Local Network and all devices IEEE 802.15.4 compatible");
}

void loop() {
  Serial.println();
  Serial.println("Scanning for nearby IEEE 802.15.4 devices:");
  // 802.15.4 Scan just needs a previous OThreadCLI.begin()
  if (!otPrintRespCLI("scan", Serial, 3000)) {
    Serial.println("Scan Failed...");
  }
  delay(5000);
  if (otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.println();
    Serial.println("This device has not started Thread yet, bypassing Discovery Scan");
    return;
  }
  Serial.println();
  Serial.println("Scanning MLE Discover:");
  if (!otPrintRespCLI("discover", Serial, 3000)) {
    Serial.println("Discover Failed...");
  }
  delay(5000);
}
