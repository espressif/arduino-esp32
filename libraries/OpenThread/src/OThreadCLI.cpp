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

static TaskHandle_t s_cli_task = NULL;
static TaskHandle_t s_console_cli_task = NULL;
static QueueHandle_t rx_queue = NULL;
static QueueHandle_t tx_queue = NULL;

static esp_openthread_platform_config_t ot_native_config;
static TaskHandle_t s_ot_task = NULL;
static esp_netif_t *openthread_netif = NULL;
#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
static struct netif *ot_lwip_netif = NULL;
#endif

#define OT_CLI_MAX_LINE_LENGTH 512

typedef struct {
  Stream *cliStream;
  bool echoback;
  String prompt;
  OnReceiveCb_t responseCallBack;
} ot_cli_console_t;
static ot_cli_console_t otConsole = {NULL, false, (const char *)NULL, NULL};

#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) {
  if (ot_lwip_netif && ot_lwip_netif == inp) {
    return 0;
  }
  if (ip6_addr_isany_val(inp->ip6_addr[0].u_addr.ip6)) {
    // We don't have an LL address -> eat this packet here, so it won't get accepted on input netif
    pbuf_free(p);
    return 1;
  }
  return 0;
}
#endif

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
        if (otConsole.responseCallBack != NULL) {
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
  otConsole.prompt = prompt;  // NULL will make the prompt not visible
}

void OpenThreadCLI::setStream(Stream &otStream) {
  otConsole.cliStream = &otStream;
}

void OpenThreadCLI::onReceive(OnReceiveCb_t func) {
  otConsole.responseCallBack = func;  // NULL will set it off
}

// Stream object shall be already started and configured before calling this function
void OpenThreadCLI::startConsole(Stream &otStream, bool echoback, const char *prompt) {
  if (!otStarted) {
    log_e("OpenThread CLI has not started. Please begin() it before starting the console.");
    return;
  }

  if (s_console_cli_task == NULL) {
    otConsole.cliStream = &otStream;
    otConsole.echoback = echoback;
    otConsole.prompt = prompt;  // NULL will invalidate the String
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
  memset(&ot_native_config, 0, sizeof(esp_openthread_platform_config_t));
  ot_native_config.radio_config.radio_mode = RADIO_MODE_NATIVE;
  ot_native_config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
  ot_native_config.port_config.storage_partition_name = "nvs";
  ot_native_config.port_config.netif_queue_size = 10;
  ot_native_config.port_config.task_queue_size = 10;
  //sTxString = "";
}

OpenThreadCLI::~OpenThreadCLI() {
  end();
}

OpenThreadCLI::operator bool() const {
  return otStarted;
}

static void ot_task_worker(void *aContext) {
  esp_vfs_eventfd_config_t eventfd_config = {
    .max_fds = 3,
  };
  bool err = false;
  if (ESP_OK != esp_event_loop_create_default()) {
    log_e("Failed to create OpentThread event loop");
    err = true;
  }
  if (!err && ESP_OK != esp_netif_init()) {
    log_e("Failed to initialize OpentThread netif");
    err = true;
  }
  if (!err && ESP_OK != esp_vfs_eventfd_register(&eventfd_config)) {
    log_e("Failed to register OpentThread eventfd");
    err = true;
  }

  // Initialize the OpenThread stack
  if (!err && ESP_OK != esp_openthread_init(&ot_native_config)) {
    log_e("Failed to initialize OpenThread stack");
    err = true;
  }
  if (!err) {
    // Initialize the OpenThread cli
    otCliInit(esp_openthread_get_instance(), ot_cli_output_callback, NULL);

    // Initialize the esp_netif bindings
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    openthread_netif = esp_netif_new(&cfg);
#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
    // Get LwIP Netif
    if (openthread_netif != NULL) {
      ot_lwip_netif = (struct netif *)esp_netif_get_netif_impl(openthread_netif);
      if (ot_lwip_netif == NULL) {
        log_e("Failed to get OpenThread LwIP netif");
      }
    }
#endif
  }
  if (!err && openthread_netif == NULL) {
    log_e("Failed to create OpenThread esp_netif");
    err = true;
  }
  if (!err && ESP_OK != esp_netif_attach(openthread_netif, esp_openthread_netif_glue_init(&ot_native_config))) {
    log_e("Failed to attach OpenThread esp_netif");
    err = true;
  }
  if (!err && ESP_OK != esp_netif_set_default_netif(openthread_netif)) {
    log_e("Failed to set default OpenThread esp_netif");
    err = true;
  }
  if (!err) {
    // only returns in case there is an OpenThread Stack failure...
    esp_openthread_launch_mainloop();
  }
  // Clean up
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(openthread_netif);
  esp_vfs_eventfd_unregister();
  vTaskDelete(NULL);
}

void OpenThreadCLI::begin(bool OThreadAutoStart) {
  if (otStarted) {
    log_w("OpenThread CLI already started. Please end() it before starting again.");
    return;
  }

  xTaskCreate(ot_task_worker, "ot_main_loop", 10240, NULL, 20, &s_ot_task);

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

  // starts Thread with default dataset from NVS or from IDF default settings
  if (OThreadAutoStart) {
    otOperationalDatasetTlvs dataset;
    otError error = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
    //    error = OT_ERROR_FAILED; // teste para forçar NULL dataset
    if (error != OT_ERROR_NONE) {
      log_i("Failed to get active NVS dataset from OpenThread");
    } else {
      log_i("Got active NVS dataset from OpenThread");
    }
    esp_err_t err = esp_openthread_auto_start((error == OT_ERROR_NONE) ? &dataset : NULL);
    if (err != ESP_OK) {
      log_i("Failed to AUTO start OpenThread");
    } else {
      log_i("AUTO start OpenThread done");
    }
  }
  otStarted = true;
  return;
}

void OpenThreadCLI::end() {
  if (!otStarted) {
    log_w("OpenThread CLI already stopped. Please begin() it before stopping again.");
    return;
  }
  if (s_ot_task != NULL) {
    vTaskDelete(s_ot_task);
    // Clean up
    esp_openthread_deinit();
    esp_openthread_netif_glue_deinit();
#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
    ot_lwip_netif = NULL;
#endif
    esp_netif_destroy(openthread_netif);
    esp_vfs_eventfd_unregister();
  }
  if (s_cli_task != NULL) {
    vTaskDelete(s_cli_task);
  }
  if (s_console_cli_task != NULL) {
    vTaskDelete(s_console_cli_task);
  }
  esp_event_loop_delete_default();
  setRxBufferSize(0);
  setTxBufferSize(0);
  otStarted = false;
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
