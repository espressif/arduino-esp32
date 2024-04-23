// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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

#include "soc/soc_caps.h"

#if SOC_I2C_SUPPORT_SLAVE
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include "sdkconfig.h"
#include "esp_attr.h"
#include "rom/gpio.h"
#include "soc/gpio_sig_map.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"

#include "esp_intr_alloc.h"
#include "soc/i2c_reg.h"
#include "soc/i2c_struct.h"
#include "hal/i2c_ll.h"
#include "hal/clk_gate_ll.h"
#include "esp32-hal-log.h"
#include "esp32-hal-i2c-slave.h"
#include "esp32-hal-periman.h"

#define I2C_SLAVE_USE_RX_QUEUE 0  // 1: Queue, 0: RingBuffer

#if SOC_I2C_NUM > 1
#define I2C_SCL_IDX(p) ((p == 0) ? I2CEXT0_SCL_OUT_IDX : ((p == 1) ? I2CEXT1_SCL_OUT_IDX : 0))
#define I2C_SDA_IDX(p) ((p == 0) ? I2CEXT0_SDA_OUT_IDX : ((p == 1) ? I2CEXT1_SDA_OUT_IDX : 0))
#else
#define I2C_SCL_IDX(p) I2CEXT0_SCL_OUT_IDX
#define I2C_SDA_IDX(p) I2CEXT0_SDA_OUT_IDX
#endif

#if CONFIG_IDF_TARGET_ESP32
#define I2C_TXFIFO_WM_INT_ENA I2C_TXFIFO_EMPTY_INT_ENA
#define I2C_RXFIFO_WM_INT_ENA I2C_RXFIFO_FULL_INT_ENA
#endif

enum {
  I2C_SLAVE_EVT_RX,
  I2C_SLAVE_EVT_TX
};

typedef struct i2c_slave_struct_t {
  i2c_dev_t *dev;
  uint8_t num;
  int8_t sda;
  int8_t scl;
  i2c_slave_request_cb_t request_callback;
  i2c_slave_receive_cb_t receive_callback;
  void *arg;
  intr_handle_t intr_handle;
  TaskHandle_t task_handle;
  QueueHandle_t event_queue;
#if I2C_SLAVE_USE_RX_QUEUE
  QueueHandle_t rx_queue;
#else
  RingbufHandle_t rx_ring_buf;
#endif
  QueueHandle_t tx_queue;
  uint32_t rx_data_count;
#if !CONFIG_DISABLE_HAL_LOCKS
  SemaphoreHandle_t lock;
#endif
} i2c_slave_struct_t;

typedef union {
  struct {
    uint32_t event : 2;
    uint32_t stop  : 1;
    uint32_t param : 29;
  };
  uint32_t val;
} i2c_slave_queue_event_t;

static i2c_slave_struct_t _i2c_bus_array[SOC_I2C_NUM] = {
  {&I2C0, 0, -1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0
#if !CONFIG_DISABLE_HAL_LOCKS
   ,
   NULL
#endif
  },
#if SOC_I2C_NUM > 1
  {&I2C1, 1, -1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0
#if !CONFIG_DISABLE_HAL_LOCKS
   ,
   NULL
#endif
  }
#endif
};

#if CONFIG_DISABLE_HAL_LOCKS
#define I2C_SLAVE_MUTEX_LOCK()
#define I2C_SLAVE_MUTEX_UNLOCK()
#else
#define I2C_SLAVE_MUTEX_LOCK()                \
  if (i2c->lock) {                            \
    xSemaphoreTake(i2c->lock, portMAX_DELAY); \
  }
#define I2C_SLAVE_MUTEX_UNLOCK() \
  if (i2c->lock) {               \
    xSemaphoreGive(i2c->lock);   \
  }
#endif

//-------------------------------------- HAL_LL (Missing Functions) ------------------------------------------------
typedef enum {
  I2C_STRETCH_CAUSE_MASTER_READ,
  I2C_STRETCH_CAUSE_TX_FIFO_EMPTY,
  I2C_STRETCH_CAUSE_RX_FIFO_FULL,
  I2C_STRETCH_CAUSE_MAX
} i2c_stretch_cause_t;

static inline i2c_stretch_cause_t i2c_ll_stretch_cause(i2c_dev_t *hw) {
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3
  return hw->sr.stretch_cause;
#elif CONFIG_IDF_TARGET_ESP32S2
  return hw->status_reg.stretch_cause;
#else
  return I2C_STRETCH_CAUSE_MAX;
#endif
}

static inline void i2c_ll_set_stretch(i2c_dev_t *hw, uint16_t time) {
#ifndef CONFIG_IDF_TARGET_ESP32
  typeof(hw->scl_stretch_conf) scl_stretch_conf;
  scl_stretch_conf.val = 0;
  scl_stretch_conf.slave_scl_stretch_en = (time > 0);
  scl_stretch_conf.stretch_protect_num = time;
  scl_stretch_conf.slave_scl_stretch_clr = 1;
  hw->scl_stretch_conf.val = scl_stretch_conf.val;
  if (time > 0) {
    //enable interrupt
    hw->int_ena.val |= I2C_SLAVE_STRETCH_INT_ENA;
  } else {
    //disable interrupt
    hw->int_ena.val &= (~I2C_SLAVE_STRETCH_INT_ENA);
  }
#endif
}

static inline void i2c_ll_stretch_clr(i2c_dev_t *hw) {
#ifndef CONFIG_IDF_TARGET_ESP32
  hw->scl_stretch_conf.slave_scl_stretch_clr = 1;
#endif
}

static inline bool i2c_ll_slave_addressed(i2c_dev_t *hw) {
#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32H2
  return hw->sr.slave_addressed;
#else
  return hw->status_reg.slave_addressed;
#endif
}

static inline bool i2c_ll_slave_rw(i2c_dev_t *hw)  //not exposed by hal_ll
{
#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32H2
  return hw->sr.slave_rw;
#else
  return hw->status_reg.slave_rw;
#endif
}

//-------------------------------------- PRIVATE (Function Prototypes) ------------------------------------------------
static void i2c_slave_free_resources(i2c_slave_struct_t *i2c);
static void i2c_slave_delay_us(uint64_t us);
static void i2c_slave_gpio_mode(int8_t pin, gpio_mode_t mode);
static bool i2c_slave_check_line_state(int8_t sda, int8_t scl);
static bool i2c_slave_attach_gpio(i2c_slave_struct_t *i2c, int8_t sda, int8_t scl);
static bool i2c_slave_detach_gpio(i2c_slave_struct_t *i2c);
static bool i2c_slave_set_frequency(i2c_slave_struct_t *i2c, uint32_t clk_speed);
static bool i2c_slave_send_event(i2c_slave_struct_t *i2c, i2c_slave_queue_event_t *event);
static bool i2c_slave_handle_tx_fifo_empty(i2c_slave_struct_t *i2c);
static bool i2c_slave_handle_rx_fifo_full(i2c_slave_struct_t *i2c, uint32_t len);
static size_t i2c_slave_read_rx(i2c_slave_struct_t *i2c, uint8_t *data, size_t len);
static void i2c_slave_isr_handler(void *arg);
static void i2c_slave_task(void *pv_args);
static bool i2cSlaveDetachBus(void *bus_i2c_num);

//=====================================================================================================================
//-------------------------------------- Public Functions -------------------------------------------------------------
//=====================================================================================================================

esp_err_t i2cSlaveAttachCallbacks(uint8_t num, i2c_slave_request_cb_t request_callback, i2c_slave_receive_cb_t receive_callback, void *arg) {
  if (num >= SOC_I2C_NUM) {
    log_e("Invalid port num: %u", num);
    return ESP_ERR_INVALID_ARG;
  }
  i2c_slave_struct_t *i2c = &_i2c_bus_array[num];
  I2C_SLAVE_MUTEX_LOCK();
  i2c->request_callback = request_callback;
  i2c->receive_callback = receive_callback;
  i2c->arg = arg;
  I2C_SLAVE_MUTEX_UNLOCK();
  return ESP_OK;
}

esp_err_t i2cSlaveInit(uint8_t num, int sda, int scl, uint16_t slaveID, uint32_t frequency, size_t rx_len, size_t tx_len) {
  if (num >= SOC_I2C_NUM) {
    log_e("Invalid port num: %u", num);
    return ESP_ERR_INVALID_ARG;
  }

  if (sda < 0 || scl < 0) {
    log_e("invalid pins sda=%d, scl=%d", sda, scl);
    return ESP_ERR_INVALID_ARG;
  }

  if (!frequency) {
    frequency = 100000;
  } else if (frequency > 1000000) {
    frequency = 1000000;
  }

  perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_SLAVE_SDA, i2cSlaveDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_SLAVE_SCL, i2cSlaveDetachBus);

  if (!perimanClearPinBus(sda) || !perimanClearPinBus(scl)) {
    return false;
  }

  log_i("Initializing I2C Slave: sda=%d scl=%d freq=%d, addr=0x%x", sda, scl, frequency, slaveID);

  i2c_slave_struct_t *i2c = &_i2c_bus_array[num];
  esp_err_t ret = ESP_OK;

#if !CONFIG_DISABLE_HAL_LOCKS
  if (!i2c->lock) {
    i2c->lock = xSemaphoreCreateMutex();
    if (i2c->lock == NULL) {
      log_e("RX queue create failed");
      return ESP_ERR_NO_MEM;
    }
  }
#endif

  I2C_SLAVE_MUTEX_LOCK();
  i2c_slave_free_resources(i2c);

#if I2C_SLAVE_USE_RX_QUEUE
  i2c->rx_queue = xQueueCreate(rx_len, sizeof(uint8_t));
  if (i2c->rx_queue == NULL) {
    log_e("RX queue create failed");
    ret = ESP_ERR_NO_MEM;
    goto fail;
  }
#else
  i2c->rx_ring_buf = xRingbufferCreate(rx_len, RINGBUF_TYPE_BYTEBUF);
  if (i2c->rx_ring_buf == NULL) {
    log_e("RX RingBuf create failed");
    ret = ESP_ERR_NO_MEM;
    goto fail;
  }
#endif

  i2c->tx_queue = xQueueCreate(tx_len, sizeof(uint8_t));
  if (i2c->tx_queue == NULL) {
    log_e("TX queue create failed");
    ret = ESP_ERR_NO_MEM;
    goto fail;
  }

  i2c->event_queue = xQueueCreate(16, sizeof(i2c_slave_queue_event_t));
  if (i2c->event_queue == NULL) {
    log_e("Event queue create failed");
    ret = ESP_ERR_NO_MEM;
    goto fail;
  }

  xTaskCreate(i2c_slave_task, "i2c_slave_task", 4096, i2c, 20, &i2c->task_handle);
  if (i2c->task_handle == NULL) {
    log_e("Event thread create failed");
    ret = ESP_ERR_NO_MEM;
    goto fail;
  }

  if (frequency == 0) {
    frequency = 100000L;
  }
  frequency = (frequency * 5) / 4;

  if (i2c->num == 0) {
    periph_ll_enable_clk_clear_rst(PERIPH_I2C0_MODULE);
#if SOC_I2C_NUM > 1
  } else {
    periph_ll_enable_clk_clear_rst(PERIPH_I2C1_MODULE);
#endif
  }

  i2c_ll_slave_init(i2c->dev);
  i2c_ll_set_fifo_mode(i2c->dev, true);
  i2c_ll_set_slave_addr(i2c->dev, slaveID, false);
  i2c_ll_set_tout(i2c->dev, I2C_LL_MAX_TIMEOUT);
  i2c_slave_set_frequency(i2c, frequency);

  if (!i2c_slave_check_line_state(sda, scl)) {
    log_e("bad pin state");
    ret = ESP_FAIL;
    goto fail;
  }

  i2c_slave_attach_gpio(i2c, sda, scl);

  if (i2c_ll_is_bus_busy(i2c->dev)) {
    log_w("Bus busy, reinit");
    ret = ESP_FAIL;
    goto fail;
  }

  i2c_ll_disable_intr_mask(i2c->dev, I2C_LL_INTR_MASK);
  i2c_ll_clear_intr_mask(i2c->dev, I2C_LL_INTR_MASK);
  i2c_ll_set_fifo_mode(i2c->dev, true);

  if (!i2c->intr_handle) {
    uint32_t flags = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_SHARED;
    if (i2c->num == 0) {
      ret = esp_intr_alloc(ETS_I2C_EXT0_INTR_SOURCE, flags, &i2c_slave_isr_handler, i2c, &i2c->intr_handle);
#if SOC_I2C_NUM > 1
    } else {
      ret = esp_intr_alloc(ETS_I2C_EXT1_INTR_SOURCE, flags, &i2c_slave_isr_handler, i2c, &i2c->intr_handle);
#endif
    }

    if (ret != ESP_OK) {
      log_e("install interrupt handler Failed=%d", ret);
      goto fail;
    }
  }

  i2c_ll_txfifo_rst(i2c->dev);
  i2c_ll_rxfifo_rst(i2c->dev);
  i2c_ll_slave_enable_rx_it(i2c->dev);
  i2c_ll_set_stretch(i2c->dev, 0x3FF);
  i2c_ll_update(i2c->dev);
  if (!perimanSetPinBus(sda, ESP32_BUS_TYPE_I2C_SLAVE_SDA, (void *)(i2c->num + 1), i2c->num, -1)
      || !perimanSetPinBus(scl, ESP32_BUS_TYPE_I2C_SLAVE_SCL, (void *)(i2c->num + 1), i2c->num, -1)) {
    i2cSlaveDetachBus((void *)(i2c->num + 1));
    ret = ESP_FAIL;
  }
  I2C_SLAVE_MUTEX_UNLOCK();
  return ret;

fail:
  i2c_slave_free_resources(i2c);
  I2C_SLAVE_MUTEX_UNLOCK();
  return ret;
}

esp_err_t i2cSlaveDeinit(uint8_t num) {
  if (num >= SOC_I2C_NUM) {
    log_e("Invalid port num: %u", num);
    return ESP_ERR_INVALID_ARG;
  }

  i2c_slave_struct_t *i2c = &_i2c_bus_array[num];
#if !CONFIG_DISABLE_HAL_LOCKS
  if (!i2c->lock) {
    log_e("Lock is not initialized! Did you call i2c_slave_init()?");
    return ESP_ERR_NO_MEM;
  }
#endif
  I2C_SLAVE_MUTEX_LOCK();
  int scl = i2c->scl;
  int sda = i2c->sda;
  i2c_slave_free_resources(i2c);
  perimanClearPinBus(scl);
  perimanClearPinBus(sda);
  I2C_SLAVE_MUTEX_UNLOCK();
  return ESP_OK;
}

size_t i2cSlaveWrite(uint8_t num, const uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
  if (num >= SOC_I2C_NUM) {
    log_e("Invalid port num: %u", num);
    return 0;
  }
  uint32_t to_queue = 0, to_fifo = 0;
  i2c_slave_struct_t *i2c = &_i2c_bus_array[num];
#if !CONFIG_DISABLE_HAL_LOCKS
  if (!i2c->lock) {
    log_e("Lock is not initialized! Did you call i2c_slave_init()?");
    return ESP_ERR_NO_MEM;
  }
#endif
  if (!i2c->tx_queue) {
    return 0;
  }
  I2C_SLAVE_MUTEX_LOCK();
#if CONFIG_IDF_TARGET_ESP32
  i2c_ll_slave_disable_tx_it(i2c->dev);
  uint32_t txfifo_len = 0;
  i2c_ll_get_txfifo_len(i2c->dev, &txfifo_len);
  if (txfifo_len < SOC_I2C_FIFO_LEN) {
    i2c_ll_txfifo_rst(i2c->dev);
  }
#endif
  i2c_ll_get_txfifo_len(i2c->dev, &to_fifo);
  if (to_fifo) {
    if (len < to_fifo) {
      to_fifo = len;
    }
    i2c_ll_write_txfifo(i2c->dev, (uint8_t *)buf, to_fifo);
    buf += to_fifo;
    len -= to_fifo;
    //reset tx_queue
    xQueueReset(i2c->tx_queue);
    //write the rest of the bytes to the queue
    if (len) {
      to_queue = uxQueueSpacesAvailable(i2c->tx_queue);
      if (len < to_queue) {
        to_queue = len;
      }
      for (size_t i = 0; i < to_queue; i++) {
        if (xQueueSend(i2c->tx_queue, &buf[i], timeout_ms / portTICK_PERIOD_MS) != pdTRUE) {
          xQueueReset(i2c->tx_queue);
          to_queue = 0;
          break;
        }
      }
      //no need to enable TX_EMPTY if tx_queue is empty
      if (to_queue) {
        i2c_ll_slave_enable_tx_it(i2c->dev);
      }
    }
  }
  I2C_SLAVE_MUTEX_UNLOCK();
  return to_queue + to_fifo;
}

//=====================================================================================================================
//-------------------------------------- Private Functions ------------------------------------------------------------
//=====================================================================================================================

static void i2c_slave_free_resources(i2c_slave_struct_t *i2c) {
  i2c_slave_detach_gpio(i2c);
  i2c_ll_set_slave_addr(i2c->dev, 0, false);
  i2c_ll_disable_intr_mask(i2c->dev, I2C_LL_INTR_MASK);
  i2c_ll_clear_intr_mask(i2c->dev, I2C_LL_INTR_MASK);

  if (i2c->intr_handle) {
    esp_intr_free(i2c->intr_handle);
    i2c->intr_handle = NULL;
  }

  if (i2c->task_handle) {
    vTaskDelete(i2c->task_handle);
    i2c->task_handle = NULL;
  }

#if I2C_SLAVE_USE_RX_QUEUE
  if (i2c->rx_queue) {
    vQueueDelete(i2c->rx_queue);
    i2c->rx_queue = NULL;
  }
#else
  if (i2c->rx_ring_buf) {
    vRingbufferDelete(i2c->rx_ring_buf);
    i2c->rx_ring_buf = NULL;
  }
#endif

  if (i2c->tx_queue) {
    vQueueDelete(i2c->tx_queue);
    i2c->tx_queue = NULL;
  }

  if (i2c->event_queue) {
    vQueueDelete(i2c->event_queue);
    i2c->event_queue = NULL;
  }

  i2c->rx_data_count = 0;
}

static bool i2c_slave_set_frequency(i2c_slave_struct_t *i2c, uint32_t clk_speed) {
  if (i2c == NULL) {
    log_e("no control buffer");
    return false;
  }
  if (clk_speed > 1100000UL) {
    clk_speed = 1100000UL;
  }

  // Adjust Fifo thresholds based on frequency
  uint32_t a = (clk_speed / 50000L) + 2;
  log_d("Fifo thresholds: rx_fifo_full = %d, tx_fifo_empty = %d", SOC_I2C_FIFO_LEN - a, a);

  i2c_hal_clk_config_t clk_cal;
#if SOC_I2C_SUPPORT_APB
  i2c_ll_cal_bus_clk(APB_CLK_FREQ, clk_speed, &clk_cal);
  i2c_ll_set_source_clk(i2c->dev, SOC_MOD_CLK_APB); /*!< I2C source clock from APB, 80M*/
#elif SOC_I2C_SUPPORT_XTAL
  i2c_ll_cal_bus_clk(XTAL_CLK_FREQ, clk_speed, &clk_cal);
  i2c_ll_set_source_clk(i2c->dev, SOC_MOD_CLK_XTAL); /*!< I2C source clock from XTAL, 40M */
#endif
  i2c_ll_set_txfifo_empty_thr(i2c->dev, a);
  i2c_ll_set_rxfifo_full_thr(i2c->dev, SOC_I2C_FIFO_LEN - a);
  i2c_ll_set_bus_timing(i2c->dev, &clk_cal);
  i2c_ll_set_filter(i2c->dev, 3);
  return true;
}

static void i2c_slave_delay_us(uint64_t us) {
  uint64_t m = esp_timer_get_time();
  if (us) {
    uint64_t e = (m + us);
    if (m > e) {  //overflow
      while ((uint64_t)esp_timer_get_time() > e);
    }
    while ((uint64_t)esp_timer_get_time() < e);
  }
}

static void i2c_slave_gpio_mode(int8_t pin, gpio_mode_t mode) {
  gpio_config_t conf = {
    .pin_bit_mask = 1LL << pin, .mode = mode, .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&conf);
}

static bool i2c_slave_check_line_state(int8_t sda, int8_t scl) {
  if (sda < 0 || scl < 0) {
    return false;  //return false since there is nothing to do
  }
  // if the bus is not 'clear' try the cycling SCL until SDA goes High or 9 cycles
  gpio_set_level(sda, 1);
  gpio_set_level(scl, 1);
  i2c_slave_gpio_mode(sda, GPIO_MODE_INPUT | GPIO_MODE_DEF_OD);
  i2c_slave_gpio_mode(scl, GPIO_MODE_INPUT | GPIO_MODE_DEF_OD);
  gpio_set_level(scl, 1);

  if (!gpio_get_level(sda) || !gpio_get_level(scl)) {  // bus in busy state
    log_w("invalid state sda(%d)=%d, scl(%d)=%d", sda, gpio_get_level(sda), scl, gpio_get_level(scl));
    for (uint8_t a = 0; a < 9; a++) {
      i2c_slave_delay_us(5);
      if (gpio_get_level(sda) && gpio_get_level(scl)) {  // bus recovered
        log_w("Recovered after %d Cycles", a);
        gpio_set_level(sda, 0);  // start
        i2c_slave_delay_us(5);
        for (uint8_t a = 0; a < 9; a++) {
          gpio_set_level(scl, 1);
          i2c_slave_delay_us(5);
          gpio_set_level(scl, 0);
          i2c_slave_delay_us(5);
        }
        gpio_set_level(scl, 1);
        i2c_slave_delay_us(5);
        gpio_set_level(sda, 1);  // stop
        break;
      }
      gpio_set_level(scl, 0);
      i2c_slave_delay_us(5);
      gpio_set_level(scl, 1);
    }
  }

  if (!gpio_get_level(sda) || !gpio_get_level(scl)) {  // bus in busy state
    log_e("Bus Invalid State, Can't init sda=%d, scl=%d", gpio_get_level(sda), gpio_get_level(scl));
    return false;  // bus is busy
  }
  return true;
}

static bool i2c_slave_attach_gpio(i2c_slave_struct_t *i2c, int8_t sda, int8_t scl) {
  if (i2c == NULL) {
    log_e("no control block");
    return false;
  }

  if ((sda < 0) || (scl < 0)) {
    log_e("bad pins sda=%d, scl=%d", sda, scl);
    return false;
  }

  i2c->scl = scl;
  gpio_set_level(scl, 1);
  i2c_slave_gpio_mode(scl, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_matrix_out(scl, I2C_SCL_IDX(i2c->num), false, false);
  gpio_matrix_in(scl, I2C_SCL_IDX(i2c->num), false);

  i2c->sda = sda;
  gpio_set_level(sda, 1);
  i2c_slave_gpio_mode(sda, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_matrix_out(sda, I2C_SDA_IDX(i2c->num), false, false);
  gpio_matrix_in(sda, I2C_SDA_IDX(i2c->num), false);

  return true;
}

static bool i2c_slave_detach_gpio(i2c_slave_struct_t *i2c) {
  if (i2c == NULL) {
    log_e("no control Block");
    return false;
  }
  if (i2c->scl >= 0) {
    gpio_matrix_out(i2c->scl, 0x100, false, false);
    gpio_matrix_in(0x30, I2C_SCL_IDX(i2c->num), false);
    i2c_slave_gpio_mode(i2c->scl, GPIO_MODE_INPUT);
    i2c->scl = -1;  // un attached
  }
  if (i2c->sda >= 0) {
    gpio_matrix_out(i2c->sda, 0x100, false, false);
    gpio_matrix_in(0x30, I2C_SDA_IDX(i2c->num), false);
    i2c_slave_gpio_mode(i2c->sda, GPIO_MODE_INPUT);
    i2c->sda = -1;  // un attached
  }
  return true;
}

static bool i2c_slave_send_event(i2c_slave_struct_t *i2c, i2c_slave_queue_event_t *event) {
  bool pxHigherPriorityTaskWoken = false;
  if (i2c->event_queue) {
    if (xQueueSendFromISR(i2c->event_queue, event, (BaseType_t *const)&pxHigherPriorityTaskWoken) != pdTRUE) {
      //log_e("event_queue_full");
    }
  }
  return pxHigherPriorityTaskWoken;
}

static bool i2c_slave_handle_tx_fifo_empty(i2c_slave_struct_t *i2c) {
  bool pxHigherPriorityTaskWoken = false;
  uint32_t d = 0, moveCnt = 0;
  i2c_ll_get_txfifo_len(i2c->dev, &moveCnt);
  while (moveCnt > 0) {  // read tx queue until Fifo is full or queue is empty
    if (xQueueReceiveFromISR(i2c->tx_queue, &d, (BaseType_t *const)&pxHigherPriorityTaskWoken) == pdTRUE) {
      i2c_ll_write_txfifo(i2c->dev, (uint8_t *)&d, 1);
      moveCnt--;
    } else {
      i2c_ll_slave_disable_tx_it(i2c->dev);
      break;
    }
  }
  return pxHigherPriorityTaskWoken;
}

static bool i2c_slave_handle_rx_fifo_full(i2c_slave_struct_t *i2c, uint32_t len) {
#if I2C_SLAVE_USE_RX_QUEUE
  uint32_t d = 0;
#else
  uint8_t data[SOC_I2C_FIFO_LEN];
#endif
  bool pxHigherPriorityTaskWoken = false;
#if I2C_SLAVE_USE_RX_QUEUE
  while (len > 0) {
    i2c_ll_read_rxfifo(i2c->dev, (uint8_t *)&d, 1);
    if (xQueueSendFromISR(i2c->rx_queue, &d, (BaseType_t *const)&pxHigherPriorityTaskWoken) != pdTRUE) {
      log_e("rx_queue_full");
    } else {
      i2c->rx_data_count++;
    }
    if (--len == 0) {
      len = i2c_ll_get_rxfifo_cnt(i2c->dev);
    }
#else
  if (len) {
    i2c_ll_read_rxfifo(i2c->dev, data, len);
    if (xRingbufferSendFromISR(i2c->rx_ring_buf, (void *)data, len, (BaseType_t *const)&pxHigherPriorityTaskWoken) != pdTRUE) {
      log_e("rx_ring_buf_full");
    } else {
      i2c->rx_data_count += len;
    }
#endif
  }
  return pxHigherPriorityTaskWoken;
}

static void i2c_slave_isr_handler(void *arg) {
  bool pxHigherPriorityTaskWoken = false;
  i2c_slave_struct_t *i2c = (i2c_slave_struct_t *)arg;  // recover data

  uint32_t activeInt = 0;
  i2c_ll_get_intr_mask(i2c->dev, &activeInt);
  i2c_ll_clear_intr_mask(i2c->dev, activeInt);
  uint32_t rx_fifo_len = 0;
  i2c_ll_get_rxfifo_cnt(i2c->dev, &rx_fifo_len);
  bool slave_rw = i2c_ll_slave_rw(i2c->dev);

  if (activeInt & I2C_RXFIFO_WM_INT_ENA) {  // RX FiFo Full
    pxHigherPriorityTaskWoken |= i2c_slave_handle_rx_fifo_full(i2c, rx_fifo_len);
    i2c_ll_slave_enable_rx_it(i2c->dev);  //is this necessary?
  }

  if (activeInt & I2C_TRANS_COMPLETE_INT_ENA) {  // STOP
    if (rx_fifo_len) {                           //READ RX FIFO
      pxHigherPriorityTaskWoken |= i2c_slave_handle_rx_fifo_full(i2c, rx_fifo_len);
    }
    if (i2c->rx_data_count) {  //WRITE or RepeatedStart
      //SEND RX Event
      i2c_slave_queue_event_t event;
      event.event = I2C_SLAVE_EVT_RX;
      event.stop = !slave_rw;
      event.param = i2c->rx_data_count;
      pxHigherPriorityTaskWoken |= i2c_slave_send_event(i2c, &event);
      //Zero RX count
      i2c->rx_data_count = 0;
    }
    if (slave_rw) {  // READ
#if CONFIG_IDF_TARGET_ESP32
      if (i2c->dev->status_reg.scl_main_state_last == 6) {
        //SEND TX Event
        i2c_slave_queue_event_t event;
        event.event = I2C_SLAVE_EVT_TX;
        pxHigherPriorityTaskWoken |= i2c_slave_send_event(i2c, &event);
      }
#else
      //reset TX data
      i2c_ll_txfifo_rst(i2c->dev);
      uint8_t d;
      while (xQueueReceiveFromISR(i2c->tx_queue, &d, (BaseType_t *const)&pxHigherPriorityTaskWoken) == pdTRUE);  //flush partial write
#endif
    }
  }

#ifndef CONFIG_IDF_TARGET_ESP32
  if (activeInt & I2C_SLAVE_STRETCH_INT_ENA) {  // STRETCH
    i2c_stretch_cause_t cause = i2c_ll_stretch_cause(i2c->dev);
    if (cause == I2C_STRETCH_CAUSE_MASTER_READ) {
      //on C3 RX data disappears with repeated start, so we need to get it here
      if (rx_fifo_len) {
        pxHigherPriorityTaskWoken |= i2c_slave_handle_rx_fifo_full(i2c, rx_fifo_len);
      }
      //SEND TX Event
      i2c_slave_queue_event_t event;
      event.event = I2C_SLAVE_EVT_TX;
      pxHigherPriorityTaskWoken |= i2c_slave_send_event(i2c, &event);
      //will clear after execution
    } else if (cause == I2C_STRETCH_CAUSE_TX_FIFO_EMPTY) {
      pxHigherPriorityTaskWoken |= i2c_slave_handle_tx_fifo_empty(i2c);
      i2c_ll_stretch_clr(i2c->dev);
    } else if (cause == I2C_STRETCH_CAUSE_RX_FIFO_FULL) {
      pxHigherPriorityTaskWoken |= i2c_slave_handle_rx_fifo_full(i2c, rx_fifo_len);
      i2c_ll_stretch_clr(i2c->dev);
    }
  }
#endif

  if (activeInt & I2C_TXFIFO_WM_INT_ENA) {  // TX FiFo Empty
    pxHigherPriorityTaskWoken |= i2c_slave_handle_tx_fifo_empty(i2c);
  }

  if (pxHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

static size_t i2c_slave_read_rx(i2c_slave_struct_t *i2c, uint8_t *data, size_t len) {
  if (!len) {
    return 0;
  }
#if I2C_SLAVE_USE_RX_QUEUE
  uint8_t d = 0;
  BaseType_t res = pdTRUE;
  for (size_t i = 0; i < len; i++) {
    if (data) {
      res = xQueueReceive(i2c->rx_queue, &data[i], 0);
    } else {
      res = xQueueReceive(i2c->rx_queue, &d, 0);
    }
    if (res != pdTRUE) {
      log_e("Read Queue(%u) Failed", i);
      len = i;
      break;
    }
  }
  return (data) ? len : 0;
#else
  size_t dlen = 0, to_read = len, so_far = 0, available = 0;
  uint8_t *rx_data = NULL;

  vRingbufferGetInfo(i2c->rx_ring_buf, NULL, NULL, NULL, NULL, &available);
  if (available < to_read) {
    log_e("Less available than requested. %u < %u", available, len);
    to_read = available;
  }

  while (to_read) {
    dlen = 0;
    rx_data = (uint8_t *)xRingbufferReceiveUpTo(i2c->rx_ring_buf, &dlen, 0, to_read);
    if (!rx_data) {
      log_e("Receive %u Failed", to_read);
      return so_far;
    }
    if (data) {
      memcpy(data + so_far, rx_data, dlen);
    }
    vRingbufferReturnItem(i2c->rx_ring_buf, rx_data);
    so_far += dlen;
    to_read -= dlen;
  }
  return (data) ? so_far : 0;
#endif
}

static void i2c_slave_task(void *pv_args) {
  i2c_slave_struct_t *i2c = (i2c_slave_struct_t *)pv_args;
  i2c_slave_queue_event_t event;
  size_t len = 0;
  bool stop = false;
  uint8_t *data = NULL;
  for (;;) {
    if (xQueueReceive(i2c->event_queue, &event, portMAX_DELAY) == pdTRUE) {
      // Write
      if (event.event == I2C_SLAVE_EVT_RX) {
        len = event.param;
        stop = event.stop;
        data = (len > 0) ? (uint8_t *)malloc(len) : NULL;

        if (len && data == NULL) {
          log_e("Malloc (%u) Failed", len);
        }
        len = i2c_slave_read_rx(i2c, data, len);
        if (i2c->receive_callback) {
          i2c->receive_callback(i2c->num, data, len, stop, i2c->arg);
        }
        free(data);

        // Read
      } else if (event.event == I2C_SLAVE_EVT_TX) {
        if (i2c->request_callback) {
          i2c->request_callback(i2c->num, i2c->arg);
        }
        i2c_ll_stretch_clr(i2c->dev);
      }
    }
  }
  vTaskDelete(NULL);
}

static bool i2cSlaveDetachBus(void *bus_i2c_num) {
  uint8_t num = (int)bus_i2c_num - 1;
  i2c_slave_struct_t *i2c = &_i2c_bus_array[num];
  if (i2c->scl == -1 && i2c->sda == -1) {
    return true;
  }
  esp_err_t err = i2cSlaveDeinit(num);
  if (err != ESP_OK) {
    log_e("i2cSlaveDeinit failed with error: %d", err);
    return false;
  }
  return true;
}

#endif /* SOC_I2C_SUPPORT_SLAVE */
