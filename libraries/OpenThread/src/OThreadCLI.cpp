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

#include "OThreadCLI.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "Arduino.h"
#include "OThreadCLI.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_vfs_eventfd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_netif_net_stack.h"
#include "lwip/netif.h"

bool OpenThreadCLI::otCLIStarted = false;
static TaskHandle_t s_cli_task = NULL;
static TaskHandle_t s_console_cli_task = NULL;
static QueueHandle_t rx_queue = NULL;
static QueueHandle_t tx_queue = NULL;

#define OT_CLI_MAX_LINE_LENGTH 512

typedef struct {
  Stream *cliStream;
  bool echoback;
  String prompt;
  OnReceiveCb_t responseCallBack;
} ot_cli_console_t;
static ot_cli_console_t otConsole = {nullptr, false, (const char *)nullptr, nullptr};

// process the CLI commands sent to the OpenThread stack
static void ot_cli_loop(void *context) {
  String sTxString("");

  while (true) {
    if (tx_queue != NULL) {
      uint8_t c;
      if (xQueueReceive(tx_queue, &c, portMAX_DELAY)) {
        // avoids sending a empty command, specially when the terminal send "\r\n" together
        if (sTxString.length() > 0 && (c == '\r' || c == '\n')) {
          esp_openthread_cli_input(sTxString.c_str());
          xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
          sTxString = "";
        } else {
          if (c == '\b' || c == 127) {
            if (sTxString.length() > 0) {
              sTxString.remove(sTxString.length() - 1);
            }
          } else {
            // only allow printable characters
            if (c > 31 && c < 127) {
              sTxString += (char)c;
            }
          }
        }
      }
    }
  }
}

// process the CLI responses received from the OpenThread stack
static int ot_cli_output_callback(void *context, const char *format, va_list args) {
  char prompt_check[3];
  int ret = 0;

  vsnprintf(prompt_check, sizeof(prompt_check), format, args);
  if (!strncmp(prompt_check, "> ", sizeof(prompt_check))) {
    if (s_cli_task) {
      xTaskNotifyGive(s_cli_task);
    }
    if (s_console_cli_task) {
      xTaskNotifyGive(s_console_cli_task);
    }
  } else {
    char buf[OT_CLI_MAX_LINE_LENGTH];
    ret = vsnprintf(buf, sizeof(buf), format, args);
    if (ret) {
      // store received data in the RX buffer
      if (rx_queue != NULL) {
        size_t freeSpace = uxQueueSpacesAvailable(rx_queue);
        if (freeSpace < ret) {
          // Drop the oldest data to make room for the new data
          for (int i = 0; i < (ret - freeSpace); i++) {
            uint8_t c;
            xQueueReceive(rx_queue, &c, 0);
          }
        }
        for (int i = 0; i < ret; i++) {
          xQueueSend(rx_queue, &buf[i], 0);
        }
        // if there is a user callback function in place, it shall have the priority
        // to process/consume the Stream data received from OpenThread CLI, which is available in its RX Buffer
        if (otConsole.responseCallBack != nullptr) {
          otConsole.responseCallBack();
        }
      }
    }
  }
  return ret;
}

// helper task to process CLI from a Stream (e.g. Serial)
static void ot_cli_console_worker(void *context) {
  ot_cli_console_t *cli = (ot_cli_console_t *)context;

  // prints the prompt as first action
  if (cli->prompt && cli->echoback) {
    cli->cliStream->print(cli->prompt.c_str());
  }
  // manages and synchronizes the Stream flow with OpenThread CLI response
  char lastReadChar;
  char c = '\n';
  while (true) {
    if (cli->cliStream->available() > 0) {
      lastReadChar = c;
      c = cli->cliStream->read();
      // if EOL is received, it may contain a combination of '\n'
      // and/or '\r' depending on the Host OS and Terminal used.
      // remove all leading '\r' '\n'
      if (c == '\r') {
        c = '\n';  // just mark it as New Line
      }
      if (c == '\n' && lastReadChar == '\n') {
        continue;
      }

      // echo it back to the console
      if (cli->echoback) {
        if (c == '\n') {
          cli->cliStream->println();  // follows whatever is defined as EOL in Arduino
        } else {
          cli->cliStream->write(c);
        }
      }
      // send it to be processed by Open Thread CLI
      OThreadCLI.write(c);
      // if EOL, it shall wait for the command to be processed in background
      if (c == '\n' && lastReadChar != '\n') {
        // wait for the OpenThread CLI to finish processing the command
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
        // read response from OpenThread CLI and send it to the Stream
        while (OThreadCLI.available() > 0) {
          char c = OThreadCLI.read();
          // echo it back to the console
          if (cli->echoback) {
            if (c == '\n') {
              cli->cliStream->println();  // follows whatever is defined as EOL in Arduino
            } else {
              cli->cliStream->write(c);
            }
          }
        }
        // print the prompt
        if (cli->prompt && cli->echoback) {
          cli->cliStream->printf(cli->prompt.c_str());
        }
      }
    }
  }
}

void OpenThreadCLI::setEchoBack(bool echoback) {
  otConsole.echoback = echoback;
}

void OpenThreadCLI::setPrompt(char *prompt) {
  otConsole.prompt = prompt;  // nullptr can make the prompt not visible
}

void OpenThreadCLI::setStream(Stream &otStream) {
  otConsole.cliStream = &otStream;
}

void OpenThreadCLI::onReceive(OnReceiveCb_t func) {
  otConsole.responseCallBack = func;  // nullptr will set it off
}

// Stream object shall be already started and configured before calling this function
void OpenThreadCLI::startConsole(Stream &otStream, bool echoback, const char *prompt) {
  if (!otCLIStarted) {
    log_e("OpenThread CLI has not started. Please begin() it before starting the console.");
    return;
  }

  if (s_console_cli_task == NULL) {
    otConsole.cliStream = &otStream;
    otConsole.echoback = echoback;
    otConsole.prompt = prompt;  // nullptr will invalidate the String
    // it will run in the same priority (1) as the Arduino setup()/loop() task
    xTaskCreate(ot_cli_console_worker, "ot_cli_console", 4096, &otConsole, 1, &s_console_cli_task);
  } else {
    log_w("A console is already running. Please stop it before starting a new one.");
  }
}

void OpenThreadCLI::stopConsole() {
  if (s_console_cli_task) {
    vTaskDelete(s_console_cli_task);
    s_console_cli_task = NULL;
  }
}

OpenThreadCLI::OpenThreadCLI() {
  //sTxString = "";
}

OpenThreadCLI::~OpenThreadCLI() {
  end();
}

OpenThreadCLI::operator bool() const {
  return otCLIStarted;
}

void OpenThreadCLI::begin() {
  if (otCLIStarted) {
    log_w("OpenThread CLI already started. Please end() it before starting again.");
    return;
  }

  if (!OpenThread::otStarted) {
    log_w("OpenThread not started. Please begin() it before starting  CLI.");
    return;
  }

  //RX Buffer default has 1024 bytes if not preset
  if (rx_queue == NULL) {
    if (!setRxBufferSize(1024)) {
      log_e("HW CDC RX Buffer error");
    }
  }
  //TX Buffer default has 256 bytes if not preset
  if (tx_queue == NULL) {
    if (!setTxBufferSize(256)) {
      log_e("HW CDC RX Buffer error");
    }
  }

  xTaskCreate(ot_cli_loop, "ot_cli", 4096, xTaskGetCurrentTaskHandle(), 2, &s_cli_task);
  // Initialize the OpenThread cli
  otCliInit(esp_openthread_get_instance(), ot_cli_output_callback, NULL);

  otCLIStarted = true;
  return;
}

void OpenThreadCLI::end() {
  if (!otCLIStarted) {
    log_w("OpenThread CLI already stopped. Please begin() it before stopping again.");
    return;
  }
  if (s_cli_task != NULL) {
    vTaskDelete(s_cli_task);
    s_cli_task = NULL;
  }
  stopConsole();
  esp_event_loop_delete_default();
  setRxBufferSize(0);
  setTxBufferSize(0);
  otCLIStarted = false;
}

size_t OpenThreadCLI::write(uint8_t c) {
  if (tx_queue == NULL) {
    return 0;
  }
  if (xQueueSend(tx_queue, &c, 0) != pdPASS) {
    return 0;
  }
  return 1;
}

size_t OpenThreadCLI::setBuffer(QueueHandle_t &queue, size_t queue_len) {
  if (queue) {
    vQueueDelete(queue);
    queue = NULL;
  }
  if (!queue_len) {
    return 0;
  }
  queue = xQueueCreate(queue_len, sizeof(uint8_t));
  if (!queue) {
    return 0;
  }
  return queue_len;
}

size_t OpenThreadCLI::setTxBufferSize(size_t tx_queue_len) {
  return setBuffer(tx_queue, tx_queue_len);
}

size_t OpenThreadCLI::setRxBufferSize(size_t rx_queue_len) {
  return setBuffer(rx_queue, rx_queue_len);
}

int OpenThreadCLI::available(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  return uxQueueMessagesWaiting(rx_queue);
}

int OpenThreadCLI::peek(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c;
  if (xQueuePeek(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

int OpenThreadCLI::read(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c = 0;
  if (xQueueReceive(rx_queue, &c, 0) == pdTRUE) {
    return c;
  }
  return -1;
}

void OpenThreadCLI::flush() {
  if (tx_queue == NULL) {
    return;
  }
  // wait for the TX Queue to be empty
  while (uxQueueMessagesWaiting(tx_queue));
}

OpenThreadCLI OThreadCLI;

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
