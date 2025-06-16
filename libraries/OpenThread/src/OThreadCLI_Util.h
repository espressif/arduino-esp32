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

#pragma once
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

typedef struct {
  int errorCode;
  String errorMessage;
} ot_cmd_return_t;

bool otGetRespCmd(const char *cmd, char *resp = NULL, uint32_t respTimeout = 5000);
bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode = NULL);
bool otPrintRespCLI(const char *cmd, Stream &output, uint32_t respTimeout);

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
