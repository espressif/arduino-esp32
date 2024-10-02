#pragma once
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"

#include "openthread/cli.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"
#include "openthread/dataset_ftd.h"

#include "Arduino.h"

typedef std::function<void(void)> OnReceiveCb_t;

class OpenThreadCLI : public Stream {
private:
  static size_t setBuffer(QueueHandle_t &queue, size_t len);
  bool otStarted = false;

public:
  OpenThreadCLI();
  ~OpenThreadCLI();
  // returns true if OpenThread CLI is running
  operator bool() const;

  // starts a task to read/write otStream. Default prompt is "ot> ". Set it to NULL to make it invisible.
  void startConsole(Stream &otStream, bool echoback = true, const char *prompt = "ot> ");
  void stopConsole();
  void setPrompt(char *prompt);        // changes the console prompt. NULL is an empty prompt.
  void setEchoBack(bool echoback);     // changes the console echoback option
  void setStream(Stream &otStream);    // changes the console Stream object
  void onReceive(OnReceiveCb_t func);  // called on a complete line of output from OT CLI, as OT Response

  void begin(bool OThreadAutoStart = true);
  void end();

  // default size is 256 bytes
  size_t setTxBufferSize(size_t tx_queue_len);
  // default size is 1024 bytes
  size_t setRxBufferSize(size_t rx_queue_len);

  size_t write(uint8_t);
  int available();
  int read();
  int peek();
  void flush();
};

extern OpenThreadCLI OThreadCLI;

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
