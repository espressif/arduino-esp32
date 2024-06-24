#pragma once
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

typedef enum {
  OT_ROLE_DISABLED = 0,  ///< The Thread stack is disabled.
  OT_ROLE_DETACHED = 1,  ///< Not currently participating in a Thread network/partition.
  OT_ROLE_CHILD = 2,     ///< The Thread Child role.
  OT_ROLE_ROUTER = 3,    ///< The Thread Router role.
  OT_ROLE_LEADER = 4,    ///< The Thread Leader role.
} ot_device_role_t;

typedef struct {
  int errorCode;
  String errorMessage;
} ot_cmd_return_t;

ot_device_role_t otGetDeviceRole();
const char *otGetStringDeviceRole();
bool otGetRespCmd(const char *cmd, char *resp = NULL, uint32_t respTimeout = 5000);
bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode = NULL);
bool otPrintRespCLI(const char *cmd, Stream &output, uint32_t respTimeout);
void otPrintNetworkInformation(Stream &output);

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
