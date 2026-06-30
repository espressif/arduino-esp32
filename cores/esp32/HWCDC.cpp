// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
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

#include "Arduino.h"  // defines ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE and ARDUINO_SERIAL_EVENT_TASK_PRIORITY
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
#include <string.h>
#pragma GCC diagnostic ignored "-Wvolatile"
#include "hal/usb_serial_jtag_ll.h"
#pragma GCC diagnostic warning "-Wvolatile"
#include "rom/ets_sys.h"

ESP_EVENT_DEFINE_BASE(ARDUINO_HW_CDC_EVENTS);

static RingbufHandle_t tx_ring_buf = NULL;
static QueueHandle_t rx_queue = NULL;
static uint8_t rx_data_buf[64] = {0};
// tx_stash_buf / tx_stash_len hold the tail of a FIFO write the hardware
// couldn't accept in one shot. They are written by the ISR (drain path and
// BUS_RESET) and cleared by task-side reset paths (flushTXBuffer(NULL,...) and
// setTxBufferSize()), so every access must go through hw_cdc_tx_mux below.
static uint8_t tx_stash_buf[64] = {0};
static volatile size_t tx_stash_len = 0;
static intr_handle_t intr_handle = NULL;
static SemaphoreHandle_t tx_lock = NULL;
static volatile bool connected = false;

// Protects TX state shared between the producer task and the ISR (which may
// run on the other core on dual-core SoCs). It covers:
//   - read-modify-write of the USB_SERIAL_JTAG INT_ENA register (without this
//     a producer's "enable IN_EMPTY" can race with the ISR's "disable
//     IN_EMPTY" in the empty-ring branch and get silently dropped, leaving
//     the link permanently stalled),
//   - all accesses to tx_stash_buf / tx_stash_len (without this a task-side
//     "clear stash" can be lost when the ISR's later write-back resurrects
//     stale state, and a torn tx_stash_len read could make the ISR
//     dereference an inconsistent stash).
static portMUX_TYPE hw_cdc_tx_mux = portMUX_INITIALIZER_UNLOCKED;

// Context-safe variants: use portENTER_CRITICAL_SAFE which picks the ISR or
// task critical-section macro at runtime based on xPortInIsrContext(). These
// are reachable from ISR context via, e.g., cdc0_write_char() (installed as
// ets_putc2 for the debug log path) -> HWCDC::isConnected() ->
// isCDC_Connected() -> hw_cdc_enable_tx_intr(). Calling the plain task
// portENTER_CRITICAL() from ISR asserts on ESP-IDF, so the helpers must
// auto-select.
static inline void hw_cdc_enable_tx_intr(void) {
  portENTER_CRITICAL_SAFE(&hw_cdc_tx_mux);
  usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
  portEXIT_CRITICAL_SAFE(&hw_cdc_tx_mux);
}

// ISR-only fast paths: skip the runtime context check when we know statically
// that we're already inside the hw_cdc_isr_handler. Functionally identical to
// the _SAFE variants but a touch leaner in the interrupt hot path.
static inline void hw_cdc_enable_tx_intr_from_isr(void) {
  portENTER_CRITICAL_ISR(&hw_cdc_tx_mux);
  usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
  portEXIT_CRITICAL_ISR(&hw_cdc_tx_mux);
}

static inline void hw_cdc_disable_tx_intr_from_isr(void) {
  portENTER_CRITICAL_ISR(&hw_cdc_tx_mux);
  usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
  portEXIT_CRITICAL_ISR(&hw_cdc_tx_mux);
}

// Atomically drops any partial FIFO bytes the ISR may have stashed. Without
// the lock, the clear could be lost if the ISR is mid-drain and writes a
// fresh tx_stash_len after the task's store. Also _SAFE because callers like
// flushTXBuffer() are reachable from cdc0_write_char() in ISR context.
static inline void hw_cdc_clear_tx_stash(void) {
  portENTER_CRITICAL_SAFE(&hw_cdc_tx_mux);
  tx_stash_len = 0;
  portEXIT_CRITICAL_SAFE(&hw_cdc_tx_mux);
}

// SOF in ISR causes problems for uploading firmware
//static volatile unsigned long lastSOF_ms;
//static volatile uint8_t SOF_TIMEOUT;

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
      .queue_size = 5,
      .task_name = "arduino_hw_cdc_events",
      .task_priority = ARDUINO_SERIAL_EVENT_TASK_PRIORITY,
      .task_stack_size = ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE,
      .task_core_id = tskNO_AFFINITY
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
    // IN_EMPTY firing means the host just picked up our last IN packet, so the
    // CDC link is healthy regardless of any transient isPlugged() glitches
    // (isPlugged() is driven by a tick-hook SOF watchdog with several ms of
    // tolerance and can briefly flap to false even while USB is fine).
    connected = true;
    if (tx_ring_buf != NULL && usb_serial_jtag_ll_txfifo_writable() == 1) {
      size_t queued_size = 0;
      uint8_t *queued_buff = NULL;
      bool from_stash = false;
      uint32_t sent_size = 0;
      bool wrote_anything = false;

      // Clear interrupt so we won't be called again until this transfer finishes.
      usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);

      // Hold hw_cdc_tx_mux across the entire stash read/write region so that a
      // task-side hw_cdc_clear_tx_stash() (from flushTXBuffer(NULL,...) or
      // setTxBufferSize()) cannot interleave with us deciding to drain the
      // stash, writing to the FIFO, and writing back the new tx_stash_len.
      // The lock is per-core re-entrant, so the nested calls in
      // hw_cdc_enable_tx_intr_from_isr() / hw_cdc_disable_tx_intr_from_isr()
      // are safe.
      portENTER_CRITICAL_ISR(&hw_cdc_tx_mux);

      if (tx_stash_len) {
        queued_buff = tx_stash_buf;
        queued_size = tx_stash_len;
        from_stash = true;
      } else {
        queued_buff = (uint8_t *)xRingbufferReceiveUpToFromISR(tx_ring_buf, &queued_size, 64);
      }

      if (queued_buff != NULL && queued_size > 0) {
        sent_size = usb_serial_jtag_ll_write_txfifo(queued_buff, queued_size);
        usb_serial_jtag_ll_txfifo_flush();
        wrote_anything = true;

        if (sent_size < queued_size) {
          tx_stash_len = queued_size - sent_size;
          memmove(tx_stash_buf, queued_buff + sent_size, tx_stash_len);
        } else {
          tx_stash_len = 0;
        }
        if (!from_stash) {
          vRingbufferReturnItemFromISR(tx_ring_buf, queued_buff, &xTaskWoken);
        }
        // Always re-arm IN_EMPTY so that the next host pickup brings us back to
        // drain whatever remains (stash + ring). Don't gate on `connected`: the
        // SOF-watchdog can lag the real state and we'd rather get one spurious
        // ISR than permanently lose the wakeup.
        hw_cdc_enable_tx_intr_from_isr();
      } else {
        // A full 64-byte packet needs an extra flush to send the terminating zero-length packet.
        usb_serial_jtag_ll_txfifo_flush();
        hw_cdc_disable_tx_intr_from_isr();

        // Re-check the ring AFTER disabling the interrupt. If a producer
        // queued data between our ReceiveUpToFromISR() above and the disable,
        // its hw_cdc_enable_tx_intr() may have raced with our disable and got
        // overwritten. Re-arming here under the same spinlock guarantees we
        // don't lose that wakeup. tx_stash_len is also re-checked because the
        // task could have cleared it (e.g. via flushTXBuffer(NULL,...)) right
        // before we took the lock, while a previous ISR pass had left
        // residual data we no longer need to chase.
        UBaseType_t bytes_waiting = 0;
        vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &bytes_waiting);
        if (bytes_waiting || tx_stash_len) {
          hw_cdc_enable_tx_intr_from_isr();
        }
      }

      portEXIT_CRITICAL_ISR(&hw_cdc_tx_mux);

      if (wrote_anything) {
        event.tx.len = sent_size;
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
    portENTER_CRITICAL_ISR(&hw_cdc_tx_mux);
    tx_stash_len = 0;
    portEXIT_CRITICAL_ISR(&hw_cdc_tx_mux);
    connected = false;
  }

  // SOF ISR is causing esptool to be unable to upload firmware to the board
  // if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_SOF) {
  //   usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SOF);
  //   lastSOF_ms = millis();
  // }

  if (xTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

// Moved to header file as inline function. Kept just as future reference.
//inline bool HWCDC::isPlugged(void) {
// SOF ISR is causing esptool to be unable to upload firmware to the board
// Timer test for SOF seems to work when uploading firmware
//  return usb_serial_jtag_is_connected();//(lastSOF_ms + SOF_TIMEOUT) >= millis();
//}
bool HWCDC::isCDC_Connected() {
  // USB may be unplugged. Do NOT touch tx_stash_len here: a single transient
  // !isPlugged() reading (the SOF watchdog has a few-ms tolerance and is
  // known to flap even on a healthy link) would otherwise silently drop bytes
  // we already pulled out of the ring buffer into the stash.
  if (!isPlugged()) {
    connected = false;
    return false;
  }

  if (connected) {
    return true;
  }

  // Not yet (re)connected. Make sure the IN_EMPTY interrupt is armed and that
  // there is something for the host to clock out so the next IN token will
  // generate the interrupt that flips us back to connected = true. We arm on
  // every call (not just once) so that a stalled int_ena -- whether from a
  // producer/ISR race or from the ISR's empty-branch disable -- always gets
  // re-enabled, breaking what would otherwise be a permanent TX deadlock.
  hw_cdc_enable_tx_intr();
  usb_serial_jtag_ll_txfifo_flush();
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
    hw_cdc_clear_tx_stash();
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
    hw_cdc_enable_tx_intr_from_isr();
  } else {
    xRingbufferSend(tx_ring_buf, (void *)(&c), 1, tx_timeout_ms / portTICK_PERIOD_MS);
    hw_cdc_enable_tx_intr();
  }
  usb_serial_jtag_ll_txfifo_flush();
}

HWCDC::HWCDC() {
  // SOF in ISR causes problems for uploading firmware
  //  lastSOF_ms = 0;
  //  SOF_TIMEOUT = 5;
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
  retCode &= perimanClearPinBus(USB_INT_PHY0_DM_GPIO_NUM);
  retCode &= perimanClearPinBus(USB_INT_PHY0_DP_GPIO_NUM);
  if (retCode) {
    // Force the host to re-enumerate (BUS_RESET)
    pinMode(USB_INT_PHY0_DM_GPIO_NUM, OUTPUT_OPEN_DRAIN);
    pinMode(USB_INT_PHY0_DP_GPIO_NUM, OUTPUT_OPEN_DRAIN);
    digitalWrite(USB_INT_PHY0_DM_GPIO_NUM, LOW);
    digitalWrite(USB_INT_PHY0_DP_GPIO_NUM, LOW);
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
  // But this is also causing terminal to hang, so they are disabled
  // deinit(NULL);
  // delay(10);  // USB Host has to enumerate it again

  // Peripheral Manager setting for USB D+ D- pins
  uint8_t pin = USB_INT_PHY0_DM_GPIO_NUM;
  if (perimanGetBusDeinit(ESP32_BUS_TYPE_USB_DM) == NULL) {
    perimanSetBusDeinit(ESP32_BUS_TYPE_USB_DM, HWCDC::deinit);
  }
  if (!perimanSetPinBus(pin, ESP32_BUS_TYPE_USB_DM, (void *)this, -1, -1)) {
    goto err;
  }
  pin = USB_INT_PHY0_DP_GPIO_NUM;
  if (perimanGetBusDeinit(ESP32_BUS_TYPE_USB_DP) == NULL) {
    perimanSetBusDeinit(ESP32_BUS_TYPE_USB_DP, HWCDC::deinit);
  }
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
  usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY | USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT | USB_SERIAL_JTAG_INTR_BUS_RESET);
  // SOF ISR is causing esptool to be unable to upload firmware to the board
  // usb_serial_jtag_ll_ena_intr_mask(
  //   USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY | USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT | USB_SERIAL_JTAG_INTR_BUS_RESET | USB_SERIAL_JTAG_INTR_SOF
  // );
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

void HWCDC::updateBaudRate(unsigned long baud) {
  log_w("USB Serial/JTAG baud rate is set by the USB standard; updateBaudRate(%lu) has no effect", baud);
}

uint32_t HWCDC::baudRate() {
  log_w("USB Serial/JTAG does not have a configurable baud rate");
  return 115200;
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
  hw_cdc_clear_tx_stash();
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
    // Kick the ISR after queuing: the not-connected path doesn't otherwise
    // re-arm IN_EMPTY, which means a transient isPlugged() glitch that flipped
    // `connected` to false would leave new data sitting in the ring with no
    // wakeup pending -- a permanent hang. Re-arming here lets the ISR drain
    // the ring and flip `connected` back to true on the next host pickup.
    hw_cdc_enable_tx_intr();
  } else {
    size_t to_send = size, so_far = 0;
    size_t ring_max = xRingbufferGetMaxItemSize(tx_ring_buf);
    if (ring_max == 0) {
      ring_max = size;
    }

    // Two independent progress caps share the same per-iteration timeout so
    // the total bounded wait is roughly max_consec_timeouts * tx_timeout_ms:
    //
    //   - "real disconnect"  -> consec_timeouts >= cap AND !isPlugged().
    //     Mark connected = false and route the remainder through the
    //     not-connected FIFO-replacement policy.
    //
    //   - "host backpressure" -> consec_timeouts >= cap, but isPlugged() is
    //     still true (e.g. the OS read buffer filled because the host app
    //     stopped reading). USB is still alive, so we do NOT touch
    //     `connected`; we just return what we managed to enqueue. Anything
    //     already in the ring stays queued for whenever the host resumes,
    //     and the caller sees a short write (Arduino's Stream::write
    //     contract) so it can decide whether to retry or drop.
    //
    // Without this second cap the loop could block forever whenever the
    // host stops consuming -- isPlugged() (SOF watchdog) keeps reporting
    // true, so the original `&& !isPlugged()` guard never fires.
    uint32_t consec_timeouts = 0;
    const uint32_t max_consec_timeouts = 20;  // ~ max_consec_timeouts * tx_timeout_ms total wait
    bool gave_up_disconnect = false;

    while (to_send) {
      size_t chunk = to_send > ring_max ? ring_max : to_send;

      // Blocking send into the TX ring buffer. The ISR drains the buffer into
      // the USB Serial/JTAG FIFO whenever the host picks up data.
      if (xRingbufferSend(tx_ring_buf, (void *)(buffer + so_far), chunk, tx_timeout_ms / portTICK_PERIOD_MS) != pdTRUE) {
        // Kick the ISR in case it stopped draining the ring.
        hw_cdc_enable_tx_intr();
        consec_timeouts++;
        if (consec_timeouts >= max_consec_timeouts) {
          // Bounded-progress deadline reached. Decide why and bail out.
          if (!isPlugged()) {
            connected = false;
            gave_up_disconnect = true;
          }
          size = so_far;
          break;
        }
        continue;
      }

      consec_timeouts = 0;
      so_far += chunk;
      to_send -= chunk;
      // Notify the ISR that new data is ready to be moved into the TX FIFO.
      hw_cdc_enable_tx_intr();
    }

    // Only dump pending bytes through the not-connected FIFO policy if we
    // actually decided this is a real disconnection. On plain host
    // backpressure we leave already-queued bytes alone and just report the
    // short write.
    if (gave_up_disconnect && to_send) {
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
      hw_cdc_enable_tx_intr();
    }
    uint32_t tries = tx_timeout_ms;  // waits 1ms per ISR sending data attempt, in case CDC is unplugged
    while (connected && tries && uxItemsWaiting) {
      delay(1);
      UBaseType_t lastUxItemsWaiting = uxItemsWaiting;
      vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
      if (lastUxItemsWaiting == uxItemsWaiting) {
        tries--;
      }
      hw_cdc_enable_tx_intr();
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
    ets_install_putc2((void (*)(char)) & cdc0_write_char);
  } else {
    ets_install_putc2(NULL);
  }
  ets_install_putc1(NULL);  // closes UART log output
}

#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT  // Hardware JTAG CDC selected
// USBSerial is always available to be used
HWCDC HWCDCSerial;
#endif

#endif /* SOC_USB_SERIAL_JTAG_SUPPORTED */
