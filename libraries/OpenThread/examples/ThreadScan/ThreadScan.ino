/*
   OpenThread.begin(true) will automatically start a node in a Thread Network
   Full scanning requires the thread node to be at least in Child state.

   This will scan the IEEE 802.14.5 devices in the local area using CLI "scan" command
   As soon as this device turns into a Child, Router or Leader, it will be able to
   scan for Local Thread Networks as well.

*/

#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

bool otPrintRespCLI(const char *cmd, Stream &output) {
  char cliResp[256];
  if (cmd == NULL) {
    return true;
  }
  OThreadCLI.println(cmd);
  while (1) {
    size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
    if (len == 0) {
      return false; // timeout for reading a response from CLI
    }
    // clip it on EOL
    for (int i = 0; i < len; i++) {
      if (cliResp[i] == '\r' || cliResp[i] == '\n') {
        cliResp[i] = '\0';
      }
    }
    if (!strncmp(cliResp, "Done", 4)) {
      return true; // finished with success
    }
    if (!strncmp(cliResp, "Error", 4)) {
      return false; // the CLI command or its arguments are not valid
    }
    output.println(cliResp); 
  }
}

void setup() {
  Serial.begin(115200);
  OThreadCLI.begin(true); // For scanning, AutoStart must be active, any setup
  OThreadCLI.setTimeout(10000); // 10 seconds for reading a line from CLI - scanning takes time
  Serial.println();
  Serial.println("This sketch will continuosly scan the Thread Local Network and all devices IEEE 802.15.4 compatible");
}

void loop() {
  Serial.println();
  Serial.println("Scanning for near by IEEE 802.15.4 devices:");
  // 802.15.4 Scan just need a previous OThreadCLI.begin() to tun on the 802.15.4 stack
  if (!otPrintRespCLI("scan", Serial)) {
    Serial.println("802.15.4 Scan Failed...");
  }
  delay(5000);
  if (otGetDeviceRole() < OT_ROLE_CHILD) {
    Serial.println();
    Serial.println("This device has not started Thread yet, bypassing Discovery Scan");
    return;
  }
  Serial.println();
  Serial.println("Scanning - MLE Discover:");
  if (!otPrintRespCLI("discover", Serial)) {
    Serial.println("MLE Discover Failed...");
  }
  delay(5000);
}
