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

#pragma once

#include "Arduino.h"
#include "soc/soc_caps.h"
#include "sdkconfig.h"

#if SOC_IEEE802154_SUPPORTED

#if CONFIG_OPENTHREAD_ENABLED

/**

 * @file OThreadCLI_Util.h

 * @brief Helper functions for driving the OpenThread CLI programmatically.

 *

 * These wrappers send a CLI command through @ref OThreadCLI and parse the

 * textual response (terminated by `Done` or `Error ...`) so sketches can use the

 * CLI as an automation channel instead of an interactive console.

 */

/**

 * @brief Result of a CLI command execution.

 */

typedef struct {

  int errorCode;  ///< OpenThread error code (0 on success).

  String errorMessage;  ///< Human-readable error text, or empty on success.

} ot_cmd_return_t;

/**

 * @brief Run a CLI command and capture all response lines until Done/Error.

 * @param cmd         Null-terminated CLI command (e.g. "state").

 * @param resp        Optional buffer to receive accumulated response text; pass NULL to ignore.

 * @param respBufSize Size of @p resp in bytes (including NUL). Required for raw @c char* buffers;

 *                    stack arrays should use the template overload below (size deduced automatically).

 * @param respTimeout Maximum time to wait for the response, in milliseconds.

 * @return true if the command completed with `Done`, false on error/timeout.

 *

 * Stale lines left in the CLI RX queue from a prior command are discarded before

 * sending @p cmd.

 *

 * @par Sketch usage (command only, ignore response):

 * @code

 * otGetRespCmd("reboot");

 * @endcode

 *

 * @par Sketch usage (raw @c char* with known size):

 * @code

 * char *ptr = ...;

 * otGetRespCmd("state", ptr, len);            // default 5000 ms timeout

 * otGetRespCmd("state", ptr, len, 10000);      // custom timeout

 * @endcode

 *

 * @par Sketch usage (legacy raw @c char*, unknown buffer size — unsafe):

 * @code

 * char *ptr = ...;

 * otGetRespCmd("state", ptr, 0, 10000);        // bufsize 0, timeout last

 * @endcode

 *

 * @par Sketch usage (stack buffer — use template overload below):

 * @code

 * char resp[256];

 * otGetRespCmd("state", resp);                 // size deduced, default timeout

 * otGetRespCmd("state", resp, 10000);          // size deduced, custom timeout

 * @endcode

 */

bool otGetRespCmd(const char *cmd, char *resp = NULL, size_t respBufSize = 0, uint32_t respTimeout = 5000);

/**

 * @brief Run a CLI command and capture the response into a fixed-size stack buffer.

 * @param cmd         Null-terminated CLI command (e.g. "state").

 * @param resp        Stack buffer; its size @p N is deduced automatically.

 * @param respTimeout Maximum time to wait for the response, in milliseconds.

 * @return true if the command completed with `Done`, false on error/timeout.

 *

 * @par Sketch usage (recommended):

 * @code

 * char resp[256];

 * if (otGetRespCmd("panid", resp)) {

 *   Serial.printf("Pan ID: %s\r\n", resp);

 * }

 *

 * if (otGetRespCmd("state", resp, 10000)) {    // custom 10 s timeout

 *   Serial.printf("State: %s\r\n", resp);

 * }

 * @endcode

 */

template<size_t N> inline bool otGetRespCmd(const char *cmd, char (&resp)[N], uint32_t respTimeout = 5000) {
  return otGetRespCmd(cmd, resp, N, respTimeout);
}

/**

 * @brief Execute a CLI command (optionally with an argument) and report the outcome.

 * @param cmd         Null-terminated CLI command (e.g. "dataset networkkey").

 * @param arg         Optional argument appended to @p cmd; may be NULL.

 * @param returnCode  Optional out-parameter that receives the error code/message.

 * @param respTimeout Maximum time to wait for Done/Error, in milliseconds.

 * @return true if the command completed with `Done`, false on error/timeout.

 */

bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode = NULL, uint32_t respTimeout = 5000);

/**

 * @brief Run a CLI command and stream its full response to an output stream.

 * @param cmd         Null-terminated CLI command.

 * @param output      Stream the response lines are written to (e.g. Serial).

 * @param respTimeout Maximum time to wait for the response, in milliseconds.

 * @return true if the command completed with `Done`, false on error/timeout.

 */

bool otPrintRespCLI(const char *cmd, Stream &output, uint32_t respTimeout);

#endif /* CONFIG_OPENTHREAD_ENABLED */

#endif /* SOC_IEEE802154_SUPPORTED */
