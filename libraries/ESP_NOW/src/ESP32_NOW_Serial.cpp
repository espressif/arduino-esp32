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

ESP_NOW_Serial_Class::ESP_NOW_Serial_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk)
  : ESP_NOW_Peer(mac_addr, channel, iface, lmk) {
  tx_ring_buf = NULL;
  rx_queue = NULL;
  tx_sem = NULL;
  queued_size = 0;
  queued_buff = NULL;
  resend_count = 0;
}

ESP_NOW_Serial_Class::~ESP_NOW_Serial_Class() {
  end();
}

size_t ESP_NOW_Serial_Class::setTxBufferSize(size_t tx_queue_len) {
  if (tx_ring_buf) {
    vRingbufferDelete(tx_ring_buf);
    tx_ring_buf = NULL;
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
    rx_queue = NULL;
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
  if (tx_sem == NULL) {
    tx_sem = xSemaphoreCreateBinary();
    //xSemaphoreTake(tx_sem, 0);
    xSemaphoreGive(tx_sem);
  }
  setRxBufferSize(1024);  //default if not preset
  setTxBufferSize(1024);  //default if not preset
  return true;
}

void ESP_NOW_Serial_Class::end() {
  remove();
  setRxBufferSize(0);
  setTxBufferSize(0);
  if (tx_sem != NULL) {
    vSemaphoreDelete(tx_sem);
    tx_sem = NULL;
  }
}

//Stream
int ESP_NOW_Serial_Class::available(void) {
  if (rx_queue == NULL) {
    return 0;
  }
  return uxQueueMessagesWaiting(rx_queue);
}

int ESP_NOW_Serial_Class::peek(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c;
  if (xQueuePeek(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

int ESP_NOW_Serial_Class::read(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c = 0;
  if (xQueueReceive(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

size_t ESP_NOW_Serial_Class::read(uint8_t *buffer, size_t size) {
  if (rx_queue == NULL) {
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
  if (tx_ring_buf == NULL) {
    return;
  }
  UBaseType_t uxItemsWaiting = 0;
  vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
  if (uxItemsWaiting) {
    // Now trigger the ISR to read data from the ring buffer.
    if (xSemaphoreTake(tx_sem, 0) == pdTRUE) {
      checkForTxData();
    }
  }
  while (uxItemsWaiting) {
    delay(5);
    vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
  }
}

//RX callback
void ESP_NOW_Serial_Class::onReceive(const uint8_t *data, size_t len, bool broadcast) {
  if (rx_queue == NULL) {
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
  //return ESP_NOW_MAX_DATA_LEN;
  if (tx_ring_buf == NULL) {
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
    queued_buff = NULL;
    xSemaphoreGive(tx_sem);
    end();
  }
  return sent;
}

bool ESP_NOW_Serial_Class::checkForTxData() {
  //do we have something that failed the last time?
  resend_count = 0;
  if (queued_buff == NULL) {
    queued_buff = (uint8_t *)xRingbufferReceiveUpTo(tx_ring_buf, &queued_size, 0, ESP_NOW_MAX_DATA_LEN);
  } else {
    log_d(MACSTR " : PREVIOUS", MAC2STR(addr()));
  }
  if (queued_buff != NULL) {
    return tryToSend() > 0;
  }
  //log_d(MACSTR ": EMPTY", MAC2STR(addr()));
  xSemaphoreGive(tx_sem);
  return false;
}

size_t ESP_NOW_Serial_Class::write(const uint8_t *buffer, size_t size, uint32_t timeout) {
  log_v(MACSTR ", size %u", MAC2STR(addr()), size);
  if (tx_sem == NULL || tx_ring_buf == NULL || !added) {
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
  if (tx_sem == NULL || tx_ring_buf == NULL || !added) {
    return;
  }
  if (success) {
    vRingbufferReturnItem(tx_ring_buf, queued_buff);
    queued_buff = NULL;
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
      queued_buff = NULL;
      xSemaphoreGive(tx_sem);
      end();
      log_e(MACSTR " : RE-SEND_MAX[%u]", MAC2STR(addr()), resend_count);
    }
  }
}
