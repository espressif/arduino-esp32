/*
 * OpenThread.begin(false) will not automatically start a node in a Thread Network
 * The user will need to start it manually using the OpenThread CLI commands
 * Use the Serial Monitor to interact with the OpenThread CLI
 *
 * Type 'help' for a list of commands.
 * Documentation: https://openthread.io/reference/cli/commands
 *
 */

#include "OThreadCLI.h"

void setup() {
  Serial.begin(115200);
  OThreadCLI.begin(false);  // No AutoStart - fresh start
  Serial.println("OpenThread CLI started - type 'help' for a list of commands.");
  OThreadCLI.startConsole(Serial);
}

void loop() {}
