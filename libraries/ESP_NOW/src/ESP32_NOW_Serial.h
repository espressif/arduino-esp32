#pragma once

#include "sdkconfig.h"
#if CONFIG_ESP_WIFI_REMOTE_ENABLED
#warning "ESP-NOW is only supported in SoCs with native Wi-Fi support"
#else

#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "Stream.h"
#include "ESP32_NOW.h"

class ESP_NOW_Serial_Class : public Stream, public ESP_NOW_Peer {
private:
  RingbufHandle_t tx_ring_buf;
  QueueHandle_t rx_queue;
  SemaphoreHandle_t tx_sem;
  size_t queued_size;
  uint8_t *queued_buff;
  size_t resend_count;
  bool _remove_on_fail;

  bool checkForTxData();
  size_t tryToSend();

public:
  ESP_NOW_Serial_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface = WIFI_IF_AP, const uint8_t *lmk = NULL, bool remove_on_fail = false);
  ~ESP_NOW_Serial_Class();
  size_t setRxBufferSize(size_t);
  size_t setTxBufferSize(size_t);
  bool begin(unsigned long baud = 0);
  void end();
  //Stream
  int available();
  int read();
  size_t read(uint8_t *buffer, size_t size);
  int peek();
  void flush();
  //Print
  int availableForWrite();
  size_t write(const uint8_t *buffer, size_t size, uint32_t timeout_ms);
  size_t write(const uint8_t *buffer, size_t size) {
    return write(buffer, size, 1000);
  }
  size_t write(uint8_t data) {
    return write(&data, 1);
  }
  //ESP_NOW_Peer
  void onReceive(const uint8_t *data, size_t len, bool broadcast);
  void onSent(bool success);
};

#endif
