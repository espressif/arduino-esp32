#include "OThreadCLI.h"
#if SOC_IEEE802154_SUPPORTED
#include "OThreadCLI_Util.h"

static const char *otRoleString[] = {
  "Disabled",     ///< The Thread stack is disabled.
  "Detached",     ///< Not currently participating in a Thread network/partition.
  "Child",        ///< The Thread Child role.
  "Router",       ///< The Thread Router role.
  "Leader",       ///< The Thread Leader role.
};

ot_device_role_t getOtDeviceRole() {
  otInstance *instance = esp_openthread_get_instance();
  return (ot_device_role_t) otThreadGetDeviceRole(instance);
}

const char* getStringOtDeviceRole() {
  return otRoleString[getOtDeviceRole()];
}

bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode) {
  char cliResp[256];
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
      while (*i && *i != ':') i++;
      if (*i) {
        *i = '\0';
        m = i + 2; // message is 2 characters after ':'
        while (i > cliResp && *i != ' ') i--; // search for ' ' before ":'
        if (*i == ' ') {
          i++; // move it forward to the number begining
          returnCode->errorCode = atoi(i);
          returnCode->errorMessage = m;
        } // otherwise, it will keep the "bad error message" information
      } // otherwise, it will keep the "bad error message" information
    } // returnCode is NULL pointer
    return false;
  }
}

#endif // SOC_IEEE802154_SUPPORTED
