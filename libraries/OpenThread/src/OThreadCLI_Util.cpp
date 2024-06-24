#include "OThreadCLI.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "OThreadCLI_Util.h"
#include <StreamString.h>

static const char *otRoleString[] = {
  "Disabled",  ///< The Thread stack is disabled.
  "Detached",  ///< Not currently participating in a Thread network/partition.
  "Child",     ///< The Thread Child role.
  "Router",    ///< The Thread Router role.
  "Leader",    ///< The Thread Leader role.
};

ot_device_role_t otGetDeviceRole() {
  if (!OThreadCLI) {
    return OT_ROLE_DISABLED;
  }
  otInstance *instance = esp_openthread_get_instance();
  return (ot_device_role_t)otThreadGetDeviceRole(instance);
}

const char *otGetStringDeviceRole() {
  return otRoleString[otGetDeviceRole()];
}

bool otGetRespCmd(const char *cmd, char *resp, uint32_t respTimeout) {
  if (!OThreadCLI) {
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
  OThreadCLI.println(cmd);
  log_d("CMD[%s]", cmd);
  uint32_t timeout = millis() + respTimeout;
  while (millis() < timeout) {
    size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
    // clip it on EOL
    for (int i = 0; i < len; i++) {
      if (cliResp[i] == '\r' || cliResp[i] == '\n') {
        cliResp[i] = '\0';
      }
    }
    log_d("Resp[%s]", cliResp);
    if (strncmp(cliResp, "Done", 4) && strncmp(cliResp, "Error", 4)) {
      cliRespAllLines += cliResp;
      cliRespAllLines.println();  // Adds whatever EOL is for the OS
    } else {
      break;
    }
  }
  if (!strncmp(cliResp, "Error", 4) || millis() > timeout) {
    return false;
  }
  if (resp != NULL) {
    strcpy(resp, cliRespAllLines.c_str());
  }
  return true;
}

bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode) {
  if (!OThreadCLI) {
    return false;
  }
  char cliResp[256] = {0};
  if (cmd == NULL) {
    return true;
  }
  if (arg == NULL) {
    OThreadCLI.println(cmd);
  } else {
    OThreadCLI.print(cmd);
    OThreadCLI.print(" ");
    OThreadCLI.println(arg);
  }
  size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
  // clip it on EOL
  for (int i = 0; i < len; i++) {
    if (cliResp[i] == '\r' || cliResp[i] == '\n') {
      cliResp[i] = '\0';
    }
  }
  log_d("CMD[%s %s] Resp[%s]", cmd, arg, cliResp);
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
          i--;  // search for ' ' before ":'
        }
        if (*i == ' ') {
          i++;  // move it forward to the number begining
          returnCode->errorCode = atoi(i);
          returnCode->errorMessage = m;
        }  // otherwise, it will keep the "bad error message" information
      }  // otherwise, it will keep the "bad error message" information
    }  // returnCode is NULL pointer
    return false;
  }
}

bool otPrintRespCLI(const char *cmd, Stream &output, uint32_t respTimeout) {
  char cliResp[256] = {0};
  if (cmd == NULL) {
    return true;
  }
  OThreadCLI.println(cmd);
  uint32_t timeout = millis() + respTimeout;
  while (millis() < timeout) {
    size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
    if (cliResp[0] == '\0') {
      // Straem has timed out and it should try again using parameter respTimeout
      continue;
    }
    // clip it on EOL
    for (int i = 0; i < len; i++) {
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
    return false;
  }
  return true;
}

void otPrintNetworkInformation(Stream &output) {
  if (!OThreadCLI) {
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
