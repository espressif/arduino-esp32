#include "sdkconfig.h"
#if CONFIG_ESP_WIFI_REMOTE_ENABLED
#warning "ESP-NOW is only supported in SoCs with native Wi-Fi support"
#else

#include "ESP32_NOW_Serial.h"
#include <string.h>
#include "esp_now.h"
#include "esp_system.h"
#include "esp32-hal.h"
#include "esp_mac.h"

/*
 *
 *    Serial Port Implementation Class
 *
*/

ESP_NOW_Serial_Class::ESP_NOW_Serial_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk, bool remove_on_fail)
  : ESP_NOW_Peer(mac_addr, channel, iface, lmk) {
  tx_ring_buf = nullptr;
  rx_queue = nullptr;
  tx_sem = nullptr;
  queued_size = 0;
  queued_buff = nullptr;
  resend_count = 0;
  _remove_on_fail = remove_on_fail;
}

ESP_NOW_Serial_Class::~ESP_NOW_Serial_Class() {
  end();
}

size_t ESP_NOW_Serial_Class::setTxBufferSize(size_t tx_queue_len) {
  if (tx_ring_buf) {
    vRingbufferDelete(tx_ring_buf);
    tx_ring_buf = nullptr;
  }
  if (!tx_queue_len) {
    return 0;
  }
  tx_ring_buf = xRingbufferCreate(tx_queue_len, RINGBUF_TYPE_BYTEBUF);
  if (!tx_ring_buf) {
    return 0;
  }
  return tx_queue_len;
}

size_t ESP_NOW_Serial_Class::setRxBufferSize(size_t rx_queue_len) {
  if (rx_queue) {
    vQueueDelete(rx_queue);
    rx_queue = nullptr;
  }
  if (!rx_queue_len) {
    return 0;
  }
  rx_queue = xQueueCreate(rx_queue_len, sizeof(uint8_t));
  if (!rx_queue) {
    return 0;
  }
  return rx_queue_len;
}

bool ESP_NOW_Serial_Class::begin(unsigned long baud) {
  if (!ESP_NOW.begin() || !add()) {
    return false;
  }
  if (tx_sem == nullptr) {
    tx_sem = xSemaphoreCreateBinary();
    //xSemaphoreTake(tx_sem, 0);
    xSemaphoreGive(tx_sem);
  }

  size_t buf_size = 0;
  if (ESP_NOW.getVersion() == 2) {
    // ESP-NOW v2.0 has a larger maximum data length, so we need to increase the buffer sizes
    // to hold around 3-4 packets
    buf_size = setRxBufferSize(4096);
    buf_size &= setTxBufferSize(4096);
  } else {
    // ESP-NOW v1.0 has a smaller maximum data length, so we can use the default buffer sizes
    // to hold around 3-4 packets
    buf_size = setRxBufferSize(1024);
    buf_size &= setTxBufferSize(1024);
  }

  if (buf_size == 0) {
    log_e("Failed to set buffer size");
    return false;
  }

  return true;
}

void ESP_NOW_Serial_Class::end() {
  remove();
  setRxBufferSize(0);
  setTxBufferSize(0);
  if (tx_sem != nullptr) {
    vSemaphoreDelete(tx_sem);
    tx_sem = nullptr;
  }
}

//Stream
int ESP_NOW_Serial_Class::available(void) {
  if (rx_queue == nullptr) {
    return 0;
  }
  return uxQueueMessagesWaiting(rx_queue);
}

int ESP_NOW_Serial_Class::peek(void) {
  if (rx_queue == nullptr) {
    return -1;
  }
  uint8_t c;
  if (xQueuePeek(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

int ESP_NOW_Serial_Class::read(void) {
  if (rx_queue == nullptr) {
    return -1;
  }
  uint8_t c = 0;
  if (xQueueReceive(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

size_t ESP_NOW_Serial_Class::read(uint8_t *buffer, size_t size) {
  if (rx_queue == nullptr) {
    return -1;
  }
  uint8_t c = 0;
  size_t count = 0;
  while (count < size && xQueueReceive(rx_queue, &c, 0)) {
    buffer[count++] = c;
  }
  return count;
}

void ESP_NOW_Serial_Class::flush() {
  if (tx_ring_buf == nullptr) {
    return;
  }
  UBaseType_t uxItemsWaiting = 0;
  vRingbufferGetInfo(tx_ring_buf, nullptr, nullptr, nullptr, nullptr, &uxItemsWaiting);
  if (uxItemsWaiting) {
    // Now trigger the ISR to read data from the ring buffer.
    if (xSemaphoreTake(tx_sem, 0) == pdTRUE) {
      checkForTxData();
    }
  }
  while (uxItemsWaiting) {
    delay(5);
    vRingbufferGetInfo(tx_ring_buf, nullptr, nullptr, nullptr, nullptr, &uxItemsWaiting);
  }
}

//RX callback
void ESP_NOW_Serial_Class::onReceive(const uint8_t *data, size_t len, bool broadcast) {
  if (rx_queue == nullptr) {
    return;
  }
  for (uint32_t i = 0; i < len; i++) {
    if (!xQueueSend(rx_queue, data + i, 0)) {
      log_e("RX Overflow!");
      return;
    }
  }
  // Now trigger the ISR to read data from the ring buffer.
  if (xSemaphoreTake(tx_sem, 0) == pdTRUE) {
    checkForTxData();
  }
}

//Print
int ESP_NOW_Serial_Class::availableForWrite() {
  if (tx_ring_buf == nullptr) {
    return 0;
  }
  return xRingbufferGetCurFreeSize(tx_ring_buf);
}

size_t ESP_NOW_Serial_Class::tryToSend() {
  //log_d(MACSTR ": %u", MAC2STR(addr()), queued_size);
  size_t sent = send(queued_buff, queued_size);
  if (!sent) {
    //_onSent will not be called anymore
    //the data is lost in this case
    vRingbufferReturnItem(tx_ring_buf, queued_buff);
    queued_buff = nullptr;
    xSemaphoreGive(tx_sem);
    end();
  }
  return sent;
}

bool ESP_NOW_Serial_Class::checkForTxData() {
  //do we have something that failed the last time?
  resend_count = 0;
  if (queued_buff == nullptr) {
    queued_buff = (uint8_t *)xRingbufferReceiveUpTo(tx_ring_buf, &queued_size, 0, ESP_NOW.getMaxDataLen());
  } else {
    log_d(MACSTR " : PREVIOUS", MAC2STR(addr()));
  }
  if (queued_buff != nullptr) {
    return tryToSend() > 0;
  }
  //log_d(MACSTR ": EMPTY", MAC2STR(addr()));
  xSemaphoreGive(tx_sem);
  return false;
}

size_t ESP_NOW_Serial_Class::write(const uint8_t *buffer, size_t size, uint32_t timeout) {
  log_v(MACSTR ", size %u", MAC2STR(addr()), size);
  if (tx_sem == nullptr || tx_ring_buf == nullptr || !added) {
    return 0;
  }
  size_t space = availableForWrite();
  size_t left = size;

  if (space) {
    if (left < space) {
      space = left;
    }
    if (xRingbufferSend(tx_ring_buf, (void *)(buffer), space, 0) == pdTRUE) {
      buffer += space;
      left -= space;
      if (xSemaphoreTake(tx_sem, 0) == pdTRUE) {
        if (checkForTxData()) {
          if (!left) {
            //we are done
            return size;
          }
        } else {
          //send failed
          return 0;
        }
      }
    } else {
      log_e("RingbufferFastSend Failed");
      return 0;
    }
  } else if (xSemaphoreTake(tx_sem, timeout) == pdTRUE) {
    checkForTxData();
  }
  // Blocking method, Sending data to ringbuffer, and handle the data in ISR.
  if (xRingbufferSend(tx_ring_buf, (void *)(buffer), left, timeout / portTICK_PERIOD_MS) != pdTRUE) {
    log_e("RingbufferSend Failed");
    return size - left;
  }
  // Now trigger the ISR to read data from the ring buffer.
  if (xSemaphoreTake(tx_sem, 0) == pdTRUE) {
    checkForTxData();
  }

  return size;
}
//TX Done Callback
void ESP_NOW_Serial_Class::onSent(bool success) {
  log_v(MACSTR " : %s", MAC2STR(addr()), success ? "OK" : "FAIL");
  if (tx_sem == nullptr || tx_ring_buf == nullptr || !added) {
    return;
  }
  if (success) {
    vRingbufferReturnItem(tx_ring_buf, queued_buff);
    queued_buff = nullptr;
    //send next packet?
    //log_d(MACSTR ": NEXT", MAC2STR(addr()));
    checkForTxData();
  } else {
    //send failed
    //resend
    if (resend_count < 5) {
      resend_count++;
      //log_d(MACSTR ": RE-SENDING[%u]", MAC2STR(addr()), resend_count);
      tryToSend();
    } else {
      //resend limit reached
      //the data is lost in this case
      vRingbufferReturnItem(tx_ring_buf, queued_buff);
      queued_buff = nullptr;
      log_e(MACSTR " : RE-SEND_MAX[%u]", MAC2STR(addr()), resend_count);
      //if we are not able to send the data and remove_on_fail is set, remove the peer
      if (_remove_on_fail) {
        xSemaphoreGive(tx_sem);
        end();
        return;
      }
      //log_d(MACSTR ": NEXT", MAC2STR(addr()));
      checkForTxData();
    }
  }
}

#endif
