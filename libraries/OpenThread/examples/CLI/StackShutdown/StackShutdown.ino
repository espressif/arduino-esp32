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
 * StackShutdown — minimal graceful OpenThread teardown demo
 *
 * Starts the stack with default NVS/dataset auto-start, prints network info,
 * waits RUN_MS, then shuts down in the documented reverse order.
 *
 * If your sketch uses OThreadUDP or OThreadCoAP*, call their stop()/destroy
 * helpers BEFORE OpenThread::end() (see README.md).
 */

#include <Arduino.h>
#include "OThread.h"
#include "OThreadCLI.h"

static const uint32_t RUN_MS = 30000;

static void gracefulShutdown() {
  Serial.println("Shutting down OpenThread (application layers first)...");

  // 1) CoAP / UDP — none in this sketch; see README for OThreadCoAPServer.stop(),
  //    destroy client objects, and OThreadUDP.stop() when used.

  // 2) CLI console and CLI task
  OThreadCLI.stopConsole();
  OThreadCLI.end();

  // 3) Optional Thread / interface stop before stack deinit
  OThread.stop();
  OThread.networkInterfaceDown();

  // 4) Stack deinit (also stops CLI/CoAP globals if still active)
  OThread.end();

  Serial.println("OpenThread ended. Call OThread.begin() to start again.");
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== StackShutdown demo ===");

  OThread.begin();
  OThreadCLI.begin();
  OThreadCLI.startConsole(Serial);

  Serial.println("Thread running. OThread.end() will run after RUN_MS.");
  OThread.otPrintNetworkInformation(Serial);
  Serial.printf("Waiting %lu ms before graceful shutdown...\n", (unsigned long)RUN_MS);
}

void loop() {
  static bool shutdownDone = false;
  static uint32_t startMs = 0;

  if (startMs == 0) {
    startMs = millis();
  }

  if (!shutdownDone && millis() - startMs >= RUN_MS) {
    gracefulShutdown();
    shutdownDone = true;
  }

  delay(100);
}
