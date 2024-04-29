// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "USB.h"
#if SOC_USB_SERIAL_JTAG_SUPPORTED

#include "esp32-hal.h"
#include "esp32-hal-periman.h"
#include "HWCDC.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
#include "soc/io_mux_reg.h"
#include "soc/usb_serial_jtag_struct.h"
#pragma GCC diagnostic ignored "-Wvolatile"
#include "hal/usb_serial_jtag_ll.h"
#pragma GCC diagnostic warning "-Wvolatile"
#include "rom/ets_sys.h"

ESP_EVENT_DEFINE_BASE(ARDUINO_HW_CDC_EVENTS);

static RingbufHandle_t tx_ring_buf = NULL;
static QueueHandle_t rx_queue = NULL;
static uint8_t rx_data_buf[64] = {0};
static intr_handle_t intr_handle = NULL;
static SemaphoreHandle_t tx_lock = NULL;
static volatile bool connected = false;

static volatile unsigned long lastSOF_ms;
static volatile uint8_t SOF_TIMEOUT;

// timeout has no effect when USB CDC is unplugged
static uint32_t tx_timeout_ms = 100;

static esp_event_loop_handle_t arduino_hw_cdc_event_loop_handle = NULL;

static esp_err_t
  arduino_hw_cdc_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, BaseType_t *task_unblocked) {
  if (arduino_hw_cdc_event_loop_handle == NULL) {
    return ESP_FAIL;
  }
  return esp_event_isr_post_to(arduino_hw_cdc_event_loop_handle, event_base, event_id, event_data, event_data_size, task_unblocked);
}

static esp_err_t
  arduino_hw_cdc_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg) {
  if (!arduino_hw_cdc_event_loop_handle) {
    esp_event_loop_args_t event_task_args = {
      .queue_size = 5, .task_name = "arduino_hw_cdc_events", .task_priority = 5, .task_stack_size = 2048, .task_core_id = tskNO_AFFINITY
    };
    if (esp_event_loop_create(&event_task_args, &arduino_hw_cdc_event_loop_handle) != ESP_OK) {
      log_e("esp_event_loop_create failed");
    }
  }
  if (arduino_hw_cdc_event_loop_handle == NULL) {
    return ESP_FAIL;
  }
  return esp_event_handler_register_with(arduino_hw_cdc_event_loop_handle, event_base, event_id, event_handler, event_handler_arg);
}

static void hw_cdc_isr_handler(void *arg) {
  portBASE_TYPE xTaskWoken = 0;
  uint32_t usbjtag_intr_status = 0;
  arduino_hw_cdc_event_data_t event = {0};
  usbjtag_intr_status = usb_serial_jtag_ll_get_intsts_mask();

  if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY) {
    // Interrupt tells us the host picked up the data we sent.
    if (!HWCDC::isPlugged()) {
      connected = false;
      usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
      // USB is unplugged, nothing to be done here
      return;
    } else {
      connected = true;
    }
    if (tx_ring_buf != NULL && usb_serial_jtag_ll_txfifo_writable() == 1) {
      // We disable the interrupt here so that the interrupt won't be triggered if there is no data to send.
      usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
      size_t queued_size;
      uint8_t *queued_buff = (uint8_t *)xRingbufferReceiveUpToFromISR(tx_ring_buf, &queued_size, 64);
      // If the hardware fifo is available, write in it. Otherwise, do nothing.
      if (queued_buff != NULL) {  //Although tx_queued_bytes may be larger than 0. We may have interrupt before xRingbufferSend() was called.
        //Copy the queued buffer into the TX FIFO
        usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
        usb_serial_jtag_ll_write_txfifo(queued_buff, queued_size);
        usb_serial_jtag_ll_txfifo_flush();
        vRingbufferReturnItemFromISR(tx_ring_buf, queued_buff, &xTaskWoken);
        if (connected) {
          usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
        }
        //send event?
        //ets_printf("TX:%u\n", queued_size);
        event.tx.len = queued_size;
        arduino_hw_cdc_event_post(ARDUINO_HW_CDC_EVENTS, ARDUINO_HW_CDC_TX_EVENT, &event, sizeof(arduino_hw_cdc_event_data_t), &xTaskWoken);
      }
    } else {
      usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
    }
  }

  if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT) {
    // read rx buffer(max length is 64), and send available data to ringbuffer.
    // Ensure the rx buffer size is larger than RX_MAX_SIZE.
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT);
    uint32_t rx_fifo_len = usb_serial_jtag_ll_read_rxfifo(rx_data_buf, 64);
    uint32_t i = 0;
    for (i = 0; i < rx_fifo_len; i++) {
      if (rx_queue == NULL || !xQueueSendFromISR(rx_queue, rx_data_buf + i, &xTaskWoken)) {
        break;
      }
    }
    event.rx.len = i;
    arduino_hw_cdc_event_post(ARDUINO_HW_CDC_EVENTS, ARDUINO_HW_CDC_RX_EVENT, &event, sizeof(arduino_hw_cdc_event_data_t), &xTaskWoken);
    connected = true;
  }

  if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_BUS_RESET) {
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_BUS_RESET);
    arduino_hw_cdc_event_post(ARDUINO_HW_CDC_EVENTS, ARDUINO_HW_CDC_BUS_RESET_EVENT, &event, sizeof(arduino_hw_cdc_event_data_t), &xTaskWoken);
    connected = false;
  }

  if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_SOF) {
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SOF);
    lastSOF_ms = millis();
  }

  if (xTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

inline bool HWCDC::isPlugged(void) {
  return (lastSOF_ms + SOF_TIMEOUT) >= millis();
}

bool HWCDC::isCDC_Connected() {
  static bool running = false;

  // USB may be unplugged
  if (!isPlugged()) {
    connected = false;
    running = false;
    SOF_TIMEOUT = 5;  // SOF timeout when unplugged
    return false;
  } else {
    SOF_TIMEOUT = 50;  // SOF timeout when plugged
  }

  if (connected) {
    running = false;
    return true;
  }

  if (running == false && !connected) {  // enables it only once!
    usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
  }

  // this will feed CDC TX FIFO to trigger IN_EMPTY
  usb_serial_jtag_ll_txfifo_flush();
  running = true;
  return false;
}

static void flushTXBuffer(const uint8_t *buffer, size_t size) {
  if (!tx_ring_buf) {
    return;
  }
  UBaseType_t uxItemsWaiting = 0;
  vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
  size_t freeSpace = xRingbufferGetCurFreeSize(tx_ring_buf);
  size_t ringbufferLength = freeSpace + uxItemsWaiting;

  if (buffer == NULL) {
    // just flush the whole ring buffer and exit - used by HWCDC::flush()
    size_t queued_size = 0;
    uint8_t *queued_buff = (uint8_t *)xRingbufferReceiveUpTo(tx_ring_buf, &queued_size, 0, ringbufferLength);
    if (queued_size && queued_buff != NULL) {
      vRingbufferReturnItem(tx_ring_buf, (void *)queued_buff);
    }
    return;
  }
  if (size == 0) {
    return;  // nothing to do
  }
  if (freeSpace >= size) {
    // there is enough space, just add the data to the ring buffer
    if (xRingbufferSend(tx_ring_buf, (void *)buffer, size, 0) != pdTRUE) {
      return;
    }
  } else {
    // how many byte should be flushed to make space for the new data
    size_t to_flush = size - freeSpace;
    if (to_flush > ringbufferLength) {
      to_flush = ringbufferLength;
    }
    size_t queued_size = 0;
    uint8_t *queued_buff = (uint8_t *)xRingbufferReceiveUpTo(tx_ring_buf, &queued_size, 0, to_flush);
    if (queued_size && queued_buff != NULL) {
      vRingbufferReturnItem(tx_ring_buf, (void *)queued_buff);
    }
    // now add the new data that fits into the ring buffer
    uint8_t *bptr = (uint8_t *)buffer;
    if (size >= ringbufferLength) {
      size = ringbufferLength;
      bptr = (uint8_t *)buffer + (size - ringbufferLength);
    }
    if (xRingbufferSend(tx_ring_buf, (void *)bptr, size, 0) != pdTRUE) {
      return;
    }
  }
  // flushes CDC FIFO
  usb_serial_jtag_ll_txfifo_flush();
}

static void ARDUINO_ISR_ATTR cdc0_write_char(char c) {
  if (tx_ring_buf == NULL) {
    return;
  }
  if (!HWCDC::isConnected()) {
    // just pop/push RingBuffer and apply FIFO policy
    flushTXBuffer((const uint8_t *)&c, 1);
    return;
  }
  if (xPortInIsrContext()) {
    xRingbufferSendFromISR(tx_ring_buf, (void *)(&c), 1, NULL);
  } else {
    xRingbufferSend(tx_ring_buf, (void *)(&c), 1, tx_timeout_ms / portTICK_PERIOD_MS);
  }
  usb_serial_jtag_ll_txfifo_flush();
}

HWCDC::HWCDC() {
  perimanSetBusDeinit(ESP32_BUS_TYPE_USB_DM, HWCDC::deinit);
  perimanSetBusDeinit(ESP32_BUS_TYPE_USB_DP, HWCDC::deinit);
  lastSOF_ms = 0;
  SOF_TIMEOUT = 5;
}

HWCDC::~HWCDC() {
  end();
}

// It should return <true> just when USB is plugged and CDC is connected.
HWCDC::operator bool() const {
  return HWCDC::isCDC_Connected();
}

void HWCDC::onEvent(esp_event_handler_t callback) {
  onEvent(ARDUINO_HW_CDC_ANY_EVENT, callback);
}

void HWCDC::onEvent(arduino_hw_cdc_event_t event, esp_event_handler_t callback) {
  arduino_hw_cdc_event_handler_register_with(ARDUINO_HW_CDC_EVENTS, event, callback, this);
}

bool HWCDC::deinit(void *busptr) {
  // avoid any recursion issue with Peripheral Manager perimanSetPinBus() call
  static bool running = false;
  if (running) {
    return true;
  }
  running = true;
  // Setting USB D+ D- pins
  bool retCode = true;
  retCode &= perimanClearPinBus(USB_DM_GPIO_NUM);
  retCode &= perimanClearPinBus(USB_DP_GPIO_NUM);
  if (retCode) {
    // Force the host to re-enumerate (BUS_RESET)
    pinMode(USB_DM_GPIO_NUM, OUTPUT_OPEN_DRAIN);
    pinMode(USB_DP_GPIO_NUM, OUTPUT_OPEN_DRAIN);
    digitalWrite(USB_DM_GPIO_NUM, LOW);
    digitalWrite(USB_DP_GPIO_NUM, LOW);
  }
  // release the flag
  running = false;
  return retCode;
}

void HWCDC::begin(unsigned long baud) {
  if (tx_lock == NULL) {
    tx_lock = xSemaphoreCreateMutex();
  }
  //RX Buffer default has 256 bytes if not preset
  if (rx_queue == NULL) {
    if (!setRxBufferSize(256)) {
      log_e("HW CDC RX Buffer error");
    }
  }
  //TX Buffer default has 256 bytes if not preset
  if (tx_ring_buf == NULL) {
    if (!setTxBufferSize(256)) {
      log_e("HW CDC TX Buffer error");
    }
  }

  // the HW Serial pins needs to be first deinited in order to allow `if(Serial)` to work :-(
  deinit(NULL);
  delay(10);  // USB Host has to enumerate it again

  // Peripheral Manager setting for USB D+ D- pins
  uint8_t pin = USB_DM_GPIO_NUM;
  if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_USB_DM, (void *)this, -1, -1)) {
    goto err;
  }
  pin = USB_DP_GPIO_NUM;
  if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_USB_DP, (void *)this, -1, -1)) {
    goto err;
  }

  // Configure PHY
  // USB_Serial_JTAG use internal PHY
  USB_SERIAL_JTAG.conf0.phy_sel = 0;
  // Disable software control USB D+ D- pullup pulldown (Device FS: dp_pullup = 1)
  USB_SERIAL_JTAG.conf0.pad_pull_override = 0;
  // Enable USB D+ pullup
  USB_SERIAL_JTAG.conf0.dp_pullup = 1;
  // Enable USB pad function
  USB_SERIAL_JTAG.conf0.usb_pad_enable = 1;
  usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
  usb_serial_jtag_ll_ena_intr_mask(
    USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY | USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT | USB_SERIAL_JTAG_INTR_BUS_RESET | USB_SERIAL_JTAG_INTR_SOF
  );
  if (!intr_handle && esp_intr_alloc(ETS_USB_SERIAL_JTAG_INTR_SOURCE, 0, hw_cdc_isr_handler, NULL, &intr_handle) != ESP_OK) {
    isr_log_e("HW USB CDC failed to init interrupts");
    end();
    return;
  }
  return;

err:
  log_e("Serial JTAG Pin %u can't be set into Peripheral Manager.", pin);
  end();
}

void HWCDC::end() {
  //Disable/clear/free tx/rx interrupt.
  usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
  usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
  esp_intr_free(intr_handle);
  intr_handle = NULL;
  if (tx_lock != NULL) {
    vSemaphoreDelete(tx_lock);
    tx_lock = NULL;
  }
  setRxBufferSize(0);
  setTxBufferSize(0);
  if (arduino_hw_cdc_event_loop_handle) {
    esp_event_loop_delete(arduino_hw_cdc_event_loop_handle);
    arduino_hw_cdc_event_loop_handle = NULL;
  }
  HWCDC::deinit(this);
  setDebugOutput(false);
  connected = false;
}

void HWCDC::setTxTimeoutMs(uint32_t timeout) {
  tx_timeout_ms = timeout;
}

/*
 * WRITING
*/

size_t HWCDC::setTxBufferSize(size_t tx_queue_len) {
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

int HWCDC::availableForWrite(void) {
  if (tx_ring_buf == NULL || tx_lock == NULL) {
    return 0;
  }
  if (xSemaphoreTake(tx_lock, tx_timeout_ms / portTICK_PERIOD_MS) != pdPASS) {
    return 0;
  }
  size_t a = xRingbufferGetCurFreeSize(tx_ring_buf);
  xSemaphoreGive(tx_lock);
  return a;
}

size_t HWCDC::write(const uint8_t *buffer, size_t size) {
  if (buffer == NULL || size == 0 || tx_ring_buf == NULL || tx_lock == NULL) {
    return 0;
  }
  if (xSemaphoreTake(tx_lock, tx_timeout_ms / portTICK_PERIOD_MS) != pdPASS) {
    return 0;
  }
  if (!isCDC_Connected()) {
    // just pop/push RingBuffer and apply FIFO policy
    flushTXBuffer(buffer, size);
  } else {
    size_t space = xRingbufferGetCurFreeSize(tx_ring_buf);
    size_t to_send = size, so_far = 0;

    if (space > size) {
      space = size;
    }
    // Non-Blocking method, Sending data to ringbuffer, and handle the data in ISR.
    if (space > 0 && xRingbufferSend(tx_ring_buf, (void *)(buffer), space, 0) != pdTRUE) {
      size = 0;
    } else {
      to_send -= space;
      so_far += space;
      // Now trigger the ISR to read data from the ring buffer.
      usb_serial_jtag_ll_txfifo_flush();
      if (connected) {
        usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
      }
      // tracks CDC trasmission progress to avoid hanging if CDC is unplugged while still sending data
      size_t last_toSend = to_send;
      uint32_t tries = tx_timeout_ms;  // waits 1ms per sending data attempt, in case CDC is unplugged
      while (connected && to_send) {
        space = xRingbufferGetCurFreeSize(tx_ring_buf);
        if (space > to_send) {
          space = to_send;
        }
        // Blocking method, Sending data to ringbuffer, and handle the data in ISR.
        if (xRingbufferSend(tx_ring_buf, (void *)(buffer + so_far), space, tx_timeout_ms / portTICK_PERIOD_MS) != pdTRUE) {
          size = so_far;
          log_w("write failed due to ring buffer full - timeout");
          break;
        }
        so_far += space;
        to_send -= space;
        // Now trigger the ISR to read data from the ring buffer.
        usb_serial_jtag_ll_txfifo_flush();
        if (connected) {
          usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
        }
        if (last_toSend == to_send) {
          // no progress in sending data... USB CDC is probably unplugged
          tries--;
          delay(1);
        } else {
          last_toSend = to_send;
          tries = tx_timeout_ms;  // reset the timeout
        }
        if (tries == 0) {  // CDC isn't connected anymore...
          size = so_far;
          log_w("write failed due to waiting USB Host - timeout");
          connected = false;
        }
      }
    }
    // CDC was diconnected while sending data ==> flush the TX buffer keeping the last data
    if (to_send && !usb_serial_jtag_ll_txfifo_writable()) {
      connected = false;
      flushTXBuffer(buffer + so_far, to_send);
    }
  }
  xSemaphoreGive(tx_lock);
  return size;
}

size_t HWCDC::write(uint8_t c) {
  return write(&c, 1);
}

void HWCDC::flush(void) {
  if (tx_ring_buf == NULL || tx_lock == NULL) {
    return;
  }
  if (xSemaphoreTake(tx_lock, tx_timeout_ms / portTICK_PERIOD_MS) != pdPASS) {
    return;
  }
  if (!isCDC_Connected()) {
    flushTXBuffer(NULL, 0);
  } else {
    UBaseType_t uxItemsWaiting = 0;
    vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    if (uxItemsWaiting) {
      // Now trigger the ISR to read data from the ring buffer.
      usb_serial_jtag_ll_txfifo_flush();
      if (connected) {
        usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
      }
    }
    uint32_t tries = tx_timeout_ms;  // waits 1ms per ISR sending data attempt, in case CDC is unplugged
    while (connected && tries && uxItemsWaiting) {
      delay(1);
      UBaseType_t lastUxItemsWaiting = uxItemsWaiting;
      vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
      if (lastUxItemsWaiting == uxItemsWaiting) {
        tries--;
      }
      if (connected) {
        usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
      }
    }
    if (tries == 0) {  // CDC isn't connected anymore...
      connected = false;
      flushTXBuffer(NULL, 0);  // flushes all TX Buffer
    }
  }
  xSemaphoreGive(tx_lock);
}

/*
 * READING
*/

size_t HWCDC::setRxBufferSize(size_t rx_queue_len) {
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

int HWCDC::available(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  return uxQueueMessagesWaiting(rx_queue);
}

int HWCDC::peek(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c;
  if (xQueuePeek(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

int HWCDC::read(void) {
  if (rx_queue == NULL) {
    return -1;
  }
  uint8_t c = 0;
  if (xQueueReceive(rx_queue, &c, 0)) {
    return c;
  }
  return -1;
}

size_t HWCDC::read(uint8_t *buffer, size_t size) {
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

/*
 * DEBUG
*/

void HWCDC::setDebugOutput(bool en) {
  if (en) {
    uartSetDebug(NULL);
    ets_install_putc1((void (*)(char)) & cdc0_write_char);
  } else {
    ets_install_putc1(NULL);
  }
}

#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT  // Hardware JTAG CDC selected
// USBSerial is always available to be used
HWCDC HWCDCSerial;
#endif

#endif /* SOC_USB_SERIAL_JTAG_SUPPORTED */
