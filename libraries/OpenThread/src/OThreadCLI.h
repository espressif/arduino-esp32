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
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_types.h"

#include "openthread/cli.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"
#include "openthread/dataset_ftd.h"

#include "Arduino.h"
#include "OThread.h"
#include <functional>

/** @brief Callback type invoked on each complete line of OpenThread CLI output. */
typedef std::function<void(void)> OnReceiveCb_t;

/**
 * @brief Arduino `Stream` front-end to the OpenThread command-line interface.
 *
 * Drives the OpenThread CLI both interactively (bridge a `Stream` such as Serial
 * with startConsole()) and programmatically (write commands and read responses
 * via the inherited `Stream` methods, or asynchronously with onReceive()).
 */
class OpenThreadCLI : public Stream {
private:
  static size_t setBuffer(QueueHandle_t &queue, size_t len);
  static bool otCLIStarted;

public:
  OpenThreadCLI();
  ~OpenThreadCLI();

  /** @brief true if the OpenThread CLI is running. */
  operator bool() const;

  /**
   * @brief Start a task that bridges a Stream to the OpenThread CLI.
   * @param otStream Stream to read commands from / write responses to (e.g. Serial).
   * @param echoback Echo typed characters back to @p otStream.
   * @param prompt   Console prompt; pass NULL for no prompt. Default "ot> ".
   */
  void startConsole(Stream &otStream, bool echoback = true, const char *prompt = "ot> ");

  /** @brief Stop the console task started by startConsole(). */
  void stopConsole();

  /**
   * @brief Change the console prompt.
   * @param prompt New prompt; NULL for an empty prompt.
   */
  void setPrompt(char *prompt);

  /**
   * @brief Enable or disable echoing of typed characters.
   * @param echoback true to echo input back to the console stream.
   */
  void setEchoBack(bool echoback);

  /**
   * @brief Change the Stream object the console reads from / writes to.
   * @param otStream New console Stream.
   */
  void setStream(Stream &otStream);

  /**
   * @brief Register a callback fired on each complete line of CLI output.
   * @param func Callback invoked when a response line is available to read from this Stream.
   * @note Runs on the OpenThread worker task. Do not block or call synchronous CLI helpers
   *       (`otGetRespCmd`, `otExecCommand`) from the callback — defer work to `loop()`.
   */
  void onReceive(OnReceiveCb_t func);

  /** @brief Initialize the OpenThread CLI layer. Call after OThread.begin(). */
  void begin();

  /** @brief Deinitialize the OpenThread CLI and release its resources. */
  void end();

  /**
   * @brief Set the transmit queue size.
   * @param tx_queue_len Size in bytes (default 256).
   * @return The size actually applied.
   */
  size_t setTxBufferSize(size_t tx_queue_len);

  /**
   * @brief Set the receive queue size.
   * @param rx_queue_len Size in bytes (default 1024).
   * @return The size actually applied.
   */
  size_t setRxBufferSize(size_t rx_queue_len);

  /** @brief Stream API: write one byte to the CLI input. @return bytes written. */
  size_t write(uint8_t);

  /** @brief Stream API: number of response bytes available to read. */
  int available();

  /** @brief Stream API: read one response byte, or -1 if none. */
  int read();

  /** @brief Stream API: peek at the next response byte without consuming it. */
  int peek();

  /** @brief Stream API: flush the CLI buffers. */
  void flush();
};

/** @brief Global OpenThread CLI instance. */
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_OPENTHREADCLI)
extern OpenThreadCLI OThreadCLI;
#endif

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
