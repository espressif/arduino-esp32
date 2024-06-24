/*
 * OpenThread.begin(false) will not automatically start a node in a Thread Network
 * A Leader node is the first device, that has a complete dataset, to start Thread
 * A complete dataset is easily achieved by using the OpenThread CLI command "dataset init new"
 *
 * In order to allow other node to join the network,
 * all of them shall use the same network master key
 * The network master key is a 16-byte key that is used to secure the network
 *
 * Using the same channel will make the process faster
 *
 */

#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

#define CLI_NETWORK_KEY    "dataset networkkey 00112233445566778899aabbccddeeff"
#define CLI_NETWORK_CHANEL "dataset channel 24"

void setup() {
  Serial.begin(115200);
  OThreadCLI.begin(false);  // No AutoStart - fresh start
  Serial.println("Setting up OpenThread Node as Leader");

  OThreadCLI.println("dataset init new");
  OThreadCLI.println(CLI_NETWORK_KEY);
  OThreadCLI.println(CLI_NETWORK_CHANEL);
  OThreadCLI.println("dataset commit active");
  OThreadCLI.println("ifconfig up");
  OThreadCLI.println("thread start");
}

void loop() {
  Serial.print("Thread Node State: ");
  Serial.println(otGetStringDeviceRole());
  delay(5000);
}
