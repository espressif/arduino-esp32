// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * CoAP CRUD — notes client
 *
 * Serial menu to exercise REST operations against notes_server.
 *
 * Commands (type in Serial Monitor):
 *   list              GET notes
 *   read <id>         GET notes/<id>
 *   add <text>        POST notes  body: {"text":"..."}
 *   update <id> <txt> PUT notes/<id>
 *   del <id>          DELETE notes/<id>
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCoAP.h"

const char PSKD[] = "J01NME";
const uint8_t CHANNEL_HINT = 15;
const uint32_t JOIN_TIMEOUT_MS = 60000;
const uint32_t ATTACH_TIMEOUT_MS = 30000;
const uint32_t ATTACH_DOT_MS = 2000;

OThreadCoAPClient CoapClient;
static IPAddress serverIp;

static void printHelp() {
  Serial.println("Commands: list | read <id> | add <text> | update <id> <text> | del <id>");
}

static bool waitForAttach(uint32_t timeoutMs, uint32_t dotMs) {
  uint32_t start = millis();
  uint32_t lastDot = start;

  while (millis() - start < timeoutMs) {
    if (OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
      Serial.println();
      return true;
    }
    if (millis() - lastDot >= dotMs) {
      Serial.print('.');
      lastDot = millis();
    }
    delay(100);
  }
  Serial.println();
  return false;
}

static bool joinNetwork() {
  Serial.println("Joining Thread network (Joiner)...");
  OThread.setChannel(CHANNEL_HINT);
  OThread.networkInterfaceUp();

  Serial.printf("Commissioning with PSKd \"%s\"...\n", PSKD);
  otError err = OThread.startJoiner(PSKD, JOIN_TIMEOUT_MS);
  if (err != OT_ERROR_NONE) {
    Serial.printf("Joiner failed: %d\n", err);
    return false;
  }
  OThread.start();

  Serial.print("Waiting for attach");
  if (!waitForAttach(ATTACH_TIMEOUT_MS, ATTACH_DOT_MS)) {
    Serial.println("Attach timeout.");
    return false;
  }

  serverIp = OThread.getLeaderRloc();
  Serial.printf("Attached as %s. Notes server: %s\n", OThread.otGetStringDeviceRole(), serverIp.toString().c_str());
  return true;
}

static void printResult(int code) {
  if (code >= 0) {
    Serial.printf("-> %d %s\n", code, OThreadCoAP::responseCodeToString(code));
    if (CoapClient.payloadLength() > 0) {
      Serial.printf("   %s\n", CoapClient.getString().c_str());
    }
  } else {
    Serial.printf("-> error %s\n", OThreadCoAP::errorToString(code));
  }
}

static void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (line == "list") {
    printResult(CoapClient.GET(serverIp, "notes"));
    return;
  }

  if (line.startsWith("read ")) {
    String id = line.substring(5);
    id.trim();
    String path = "notes/" + id;
    printResult(CoapClient.GET(serverIp, path.c_str()));
    return;
  }

  if (line.startsWith("add ")) {
    String text = line.substring(4);
    text.trim();
    String body = "{\"text\":\"" + text + "\"}";
    printResult(CoapClient.POST(serverIp, "notes", body.c_str()));
    return;
  }

  if (line.startsWith("update ")) {
    int sp = line.indexOf(' ', 7);
    if (sp < 0) {
      Serial.println("Usage: update <id> <text>");
      return;
    }
    String id = line.substring(7, sp);
    String text = line.substring(sp + 1);
    text.trim();
    String path = "notes/" + id;
    String body = "{\"text\":\"" + text + "\"}";
    printResult(CoapClient.PUT(serverIp, path.c_str(), body.c_str()));
    return;
  }

  if (line.startsWith("del ")) {
    String id = line.substring(4);
    id.trim();
    String path = "notes/" + id;
    printResult(CoapClient.DELETE(serverIp, path.c_str()));
    return;
  }

  if (line == "help") {
    printHelp();
    return;
  }

  Serial.println("Unknown command. Type help.");
}

void setup() {
  Serial.begin(115200);

  Serial.println("=== CoAP CRUD — notes client ===");

  OThread.begin(false);
  CoapClient.setTimeout(4000);
  CoapClient.setConfirmable(true);
  CoapClient.setContentFormat(OT_COAP_FORMAT_JSON);  // POST/PUT bodies are JSON

  while (!joinNetwork()) {
    Serial.println("Join failed, retry in 3s...");
    OThread.stop();
    delay(3000);
  }

  printHelp();
  Serial.println("Ready. Enter a command.");
}

void loop() {
  if (Serial.available()) {
    handleCommand(Serial.readStringUntil('\n'));
  }
  delay(20);
}
