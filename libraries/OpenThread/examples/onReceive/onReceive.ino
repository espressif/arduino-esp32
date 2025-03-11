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
   OpenThread.begin() will automatically start a node in a Thread Network
   This will demonstrate how to capture the CLI response in a callback function
   The device state shall change from "disabled" to valid Thread states along time
*/

#include "OThreadCLI.h"

// reads all the lines sent by CLI, one by one
// ignores some lines that are just a sequence of \r\n
void otReceivedLine() {
  String line = "";
  while (OThreadCLI.available() > 0) {
    char ch = OThreadCLI.read();
    if (ch != '\r' && ch != '\n') {
      line += ch;
    }
  }
  // ignores empty lines, usually EOL sequence
  if (line.length() > 0) {
    Serial.print("OpenThread CLI RESP===> ");
    Serial.println(line.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  OThreadCLI.begin();  // AutoStart
  OThreadCLI.onReceive(otReceivedLine);
}

void loop() {
  // sends the "state" command to the CLI every second
  // the onReceive() Callback Function will read and process the response
  OThreadCLI.println("state");
  delay(1000);
}
