// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
