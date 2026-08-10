// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

#include "OThreadCLI.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "OThreadCLI_Util.h"
#include <StreamString.h>
#include <string.h>

namespace {

// Discard stale CLI output so a prior timed-out command cannot supply Done/Error
// for the next command.
static void drainCliRxQueue() {
  if (!OThreadCLI) {
    return;
  }
  uint32_t idleSince = millis();
  while (millis() - idleSince < 50) {
    while (OThreadCLI.available() > 0) {
      (void)OThreadCLI.read();
      idleSince = millis();
    }
    delay(1);
  }
}

}  // namespace

bool otGetRespCmd(const char *cmd, char *resp, size_t respBufSize, uint32_t respTimeout) {
  if (!OThreadCLI) {
    log_w("otGetRespCmd: OpenThread CLI not started");
    return false;
  }
  StreamString cliRespAllLines;
  char cliResp[256] = {0};
  if (resp != NULL) {
    *resp = '\0';
  }
  if (cmd == NULL) {
    return true;
  }
  drainCliRxQueue();
  OThreadCLI.println(cmd);
  log_d("CMD[%s]", cmd);
  uint32_t timeout = millis() + respTimeout;
  while (millis() < timeout) {
    size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
    // clip it on EOL
    for (int i = 0; i < (int)len; i++) {
      if (cliResp[i] == '\r' || cliResp[i] == '\n') {
        cliResp[i] = '\0';
      }
    }
    if (len > 0) {
      log_d("Resp[%s]", cliResp);
    }
    if (strncmp(cliResp, "Done", 4) && strncmp(cliResp, "Error", 4)) {
      cliRespAllLines += cliResp;
      cliRespAllLines.println();  // Adds whatever EOL is for the OS
    } else {
      break;
    }
    if (len == 0) {
      delay(1);
    }
  }
  if (!strncmp(cliResp, "Error", 4) || millis() > timeout) {
    if (!strncmp(cliResp, "Error", 4)) {
      log_w("otGetRespCmd: CLI error for '%s': %s", cmd, cliResp);
    } else {
      log_w("otGetRespCmd: timed out waiting for '%s'", cmd);
    }
    return false;
  }
  if (resp != NULL) {
    size_t n = cliRespAllLines.length();
    if (respBufSize == 0) {
      // Legacy raw char* path: caller did not supply a buffer size. Stack-buffer
      // callers use the template overload, which always passes N > 0.
      strcpy(resp, cliRespAllLines.c_str());
    } else {
      if (n + 1 > respBufSize) {
        log_w("otGetRespCmd: response truncated (%u bytes into %u-byte buffer)", (unsigned)n, (unsigned)respBufSize);
        n = respBufSize - 1;
      }
      memcpy(resp, cliRespAllLines.c_str(), n);
      resp[n] = '\0';
    }
  }
  return true;
}

bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode, uint32_t respTimeout) {
  if (!OThreadCLI) {
    log_w("otExecCommand: OpenThread CLI not started");
    return false;
  }
  char cliResp[256] = {0};
  if (cmd == NULL) {
    return true;
  }
  drainCliRxQueue();
  if (arg == NULL) {
    OThreadCLI.println(cmd);
  } else {
    OThreadCLI.print(cmd);
    OThreadCLI.print(" ");
    OThreadCLI.println(arg);
  }
  uint32_t timeout = millis() + respTimeout;
  bool gotTerminal = false;
  while (millis() < timeout) {
    size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
    for (int i = 0; i < (int)len; i++) {
      if (cliResp[i] == '\r' || cliResp[i] == '\n') {
        cliResp[i] = '\0';
      }
    }
    if (len > 0) {
      if (arg == NULL) {
        log_d("CMD[%s] Resp[%s]", cmd, cliResp);
      } else {
        log_d("CMD[%s %s] Resp[%s]", cmd, arg, cliResp);
      }
    }
    if (!strncmp(cliResp, "Done", 4) || !strncmp(cliResp, "Error", 4)) {
      gotTerminal = true;
      break;
    }
    if (len == 0) {
      delay(1);
    }
  }
  if (!gotTerminal) {
    log_w("otExecCommand: timed out waiting for '%s'", cmd);
    return false;
  }
  // initial returnCode is success values
  if (returnCode) {
    returnCode->errorCode = 0;
    returnCode->errorMessage = "Done";
  }
  if (!strncmp(cliResp, "Done", 4)) {
    return true;
  } else {
    if (returnCode) {
      // initial setting is a bad error message or it is something else...
      // return -1 and the full returned message
      returnCode->errorCode = -1;
      returnCode->errorMessage = cliResp;
      // parse cliResp looking for errorCode and errorMessage
      // OT CLI error message format is "Error ##: msg\n" - Example:
      //Error 35: InvalidCommand
      //Error 7: InvalidArgs
      char *i = cliResp;
      char *m = cliResp;
      while (*i && *i != ':') {
        i++;
      }
      if (*i) {
        *i = '\0';
        m = i + 2;  // message is 2 characters after ':'
        while (i > cliResp && *i != ' ') {
          i--;  // search for ' ' before ':'
        }
        if (*i == ' ') {
          i++;  // move it forward to the number beginning
          returnCode->errorCode = atoi(i);
          returnCode->errorMessage = m;
        }  // otherwise, it will keep the "bad error message" information
      }  // otherwise, it will keep the "bad error message" information
    }  // returnCode is NULL pointer
    if (arg == NULL) {
      log_w("otExecCommand: CLI error for '%s': %s", cmd, cliResp);
    } else {
      log_w("otExecCommand: CLI error for '%s %s': %s", cmd, arg, cliResp);
    }
    return false;
  }
}

bool otPrintRespCLI(const char *cmd, Stream &output, uint32_t respTimeout) {
  if (!OThreadCLI) {
    log_w("otPrintRespCLI: OpenThread CLI not started");
    return false;
  }
  char cliResp[256] = {0};
  if (cmd == NULL) {
    return true;
  }
  drainCliRxQueue();
  OThreadCLI.println(cmd);
  uint32_t timeout = millis() + respTimeout;
  while (millis() < timeout) {
    size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
    if (len == 0) {
      delay(1);
      continue;
    }
    // clip it on EOL
    for (int i = 0; i < (int)len; i++) {
      if (cliResp[i] == '\r' || cliResp[i] == '\n') {
        cliResp[i] = '\0';
      }
    }
    if (strncmp(cliResp, "Done", 4) && strncmp(cliResp, "Error", 4)) {
      output.println(cliResp);
      memset(cliResp, 0, sizeof(cliResp));
      timeout = millis() + respTimeout;  // renew timeout, line per line
    } else {
      break;
    }
  }
  if (!strncmp(cliResp, "Error", 4) || millis() > timeout) {
    if (!strncmp(cliResp, "Error", 4)) {
      log_w("otPrintRespCLI: CLI error for '%s': %s", cmd, cliResp);
    } else {
      log_w("otPrintRespCLI: timed out waiting for '%s'", cmd);
    }
    return false;
  }
  return true;
}

void otCLIPrintNetworkInformation(Stream &output) {
  if (!OThreadCLI) {
    log_w("otCLIPrintNetworkInformation: OpenThread CLI not started");
    return;
  }
  char resp[512];
  output.println("Thread Setup:");
  if (otGetRespCmd("state", resp)) {
    output.printf("Node State: \t%s", resp);
  }
  if (otGetRespCmd("networkname", resp)) {
    output.printf("Network Name: \t%s", resp);
  }
  if (otGetRespCmd("channel", resp)) {
    output.printf("Channel: \t%s", resp);
  }
  if (otGetRespCmd("panid", resp)) {
    output.printf("Pan ID: \t%s", resp);
  }
  if (otGetRespCmd("extpanid", resp)) {
    output.printf("Ext Pan ID: \t%s", resp);
  }
  if (otGetRespCmd("networkkey", resp)) {
    output.printf("Network Key: \t%s", resp);
  }
  if (otGetRespCmd("ipaddr", resp)) {
    output.println("Node IP Addresses are:");
    output.printf("%s", resp);
  }
  if (otGetRespCmd("ipmaddr", resp)) {
    output.println("Node Multicast Addresses are:");
    output.printf("%s", resp);
  }
}

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
