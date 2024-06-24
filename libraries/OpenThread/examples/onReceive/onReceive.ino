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
