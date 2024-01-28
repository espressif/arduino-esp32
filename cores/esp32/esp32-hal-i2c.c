// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#include "esp32-hal-i2c.h"

#if SOC_I2C_SUPPORTED
#include "esp32-hal.h"
#if !CONFIG_DISABLE_HAL_LOCKS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif
#include "esp_attr.h"
#include "esp_system.h"
#include "soc/soc_caps.h"
#include "soc/i2c_periph.h"
#include "hal/i2c_hal.h"
#include "hal/i2c_ll.h"
#include "driver/i2c.h"
#include "esp32-hal-periman.h"

#if SOC_I2C_SUPPORT_APB || SOC_I2C_SUPPORT_XTAL
#include "esp_private/esp_clk.h"
#endif
#if SOC_I2C_SUPPORT_RTC
#include "clk_ctrl_os.h"
#endif

typedef volatile struct {
  bool initialized;
  uint32_t frequency;
#if !CONFIG_DISABLE_HAL_LOCKS
  SemaphoreHandle_t lock;
#endif
  int8_t scl;
  int8_t sda;

} i2c_bus_t;

static i2c_bus_t bus[SOC_I2C_NUM];

static bool i2cDetachBus(void *bus_i2c_num) {
  uint8_t i2c_num = (int)bus_i2c_num - 1;
  if (!bus[i2c_num].initialized) {
    return true;
  }
  esp_err_t err = i2cDeinit(i2c_num);
  if (err != ESP_OK) {
    log_e("i2cDeinit failed with error: %d", err);
    return false;
  }
  return true;
}

bool i2cIsInit(uint8_t i2c_num) {
  if (i2c_num >= SOC_I2C_NUM) {
    return false;
  }
  return bus[i2c_num].initialized;
}

esp_err_t i2cInit(uint8_t i2c_num, int8_t sda, int8_t scl, uint32_t frequency) {
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  if (bus[i2c_num].lock == NULL) {
    bus[i2c_num].lock = xSemaphoreCreateMutex();
    if (bus[i2c_num].lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return ESP_ERR_NO_MEM;
    }
  }
  //acquire lock
  if (xSemaphoreTake(bus[i2c_num].lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return ESP_FAIL;
  }
#endif
  if (bus[i2c_num].initialized) {
    log_e("bus is already initialized");
    return ESP_FAIL;
  }

  if (!frequency) {
    frequency = 100000UL;
  } else if (frequency > 1000000UL) {
    frequency = 1000000UL;
  }

  perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_MASTER_SDA, i2cDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_MASTER_SCL, i2cDetachBus);

  if (!perimanClearPinBus(sda) || !perimanClearPinBus(scl)) {
    return false;
  }

  log_i("Initializing I2C Master: sda=%d scl=%d freq=%d", sda, scl, frequency);

  i2c_config_t conf = {};
  conf.mode = I2C_MODE_MASTER;
  conf.scl_io_num = (gpio_num_t)scl;
  conf.sda_io_num = (gpio_num_t)sda;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = frequency;
  conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;  //Any one clock source that is available for the specified frequency may be chosen

  esp_err_t ret = i2c_param_config((i2c_port_t)i2c_num, &conf);
  if (ret != ESP_OK) {
    log_e("i2c_param_config failed");
  } else {
    ret = i2c_driver_install((i2c_port_t)i2c_num, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
      log_e("i2c_driver_install failed");
    } else {
      bus[i2c_num].initialized = true;
      bus[i2c_num].frequency = frequency;
      bus[i2c_num].scl = scl;
      bus[i2c_num].sda = sda;
      //Clock Stretching Timeout: 20b:esp32, 5b:esp32-c3, 24b:esp32-s2
      i2c_set_timeout((i2c_port_t)i2c_num, I2C_LL_MAX_TIMEOUT);
      if (!perimanSetPinBus(sda, ESP32_BUS_TYPE_I2C_MASTER_SDA, (void *)(i2c_num + 1), i2c_num, -1) || !perimanSetPinBus(scl, ESP32_BUS_TYPE_I2C_MASTER_SCL, (void *)(i2c_num + 1), i2c_num, -1)) {
        i2cDetachBus((void *)(i2c_num + 1));
        return false;
      }
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return ret;
}

esp_err_t i2cDeinit(uint8_t i2c_num) {
  esp_err_t err = ESP_FAIL;
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //acquire lock
  if (bus[i2c_num].lock == NULL || xSemaphoreTake(bus[i2c_num].lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return err;
  }
#endif
  if (!bus[i2c_num].initialized) {
    log_e("bus is not initialized");
  } else {
    err = i2c_driver_delete((i2c_port_t)i2c_num);
    if (err == ESP_OK) {
      bus[i2c_num].initialized = false;
      perimanClearPinBus(bus[i2c_num].scl);
      perimanClearPinBus(bus[i2c_num].sda);
      bus[i2c_num].scl = -1;
      bus[i2c_num].sda = -1;
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return err;
}

esp_err_t i2cWrite(uint8_t i2c_num, uint16_t address, const uint8_t *buff, size_t size, uint32_t timeOutMillis) {
  esp_err_t ret = ESP_FAIL;
  i2c_cmd_handle_t cmd = NULL;
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //acquire lock
  if (bus[i2c_num].lock == NULL || xSemaphoreTake(bus[i2c_num].lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return ret;
  }
#endif
  if (!bus[i2c_num].initialized) {
    log_e("bus is not initialized");
    goto end;
  }

  //short implementation does not support zero size writes (example when scanning) PR in IDF?
  //ret =  i2c_master_write_to_device((i2c_port_t)i2c_num, address, buff, size, timeOutMillis / portTICK_PERIOD_MS);

  ret = ESP_OK;
  uint8_t cmd_buff[I2C_LINK_RECOMMENDED_SIZE(1)] = { 0 };
  cmd = i2c_cmd_link_create_static(cmd_buff, I2C_LINK_RECOMMENDED_SIZE(1));
  ret = i2c_master_start(cmd);
  if (ret != ESP_OK) {
    goto end;
  }
  ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
  if (ret != ESP_OK) {
    goto end;
  }
  if (size) {
    ret = i2c_master_write(cmd, buff, size, true);
    if (ret != ESP_OK) {
      goto end;
    }
  }
  ret = i2c_master_stop(cmd);
  if (ret != ESP_OK) {
    goto end;
  }
  ret = i2c_master_cmd_begin((i2c_port_t)i2c_num, cmd, timeOutMillis / portTICK_PERIOD_MS);

end:
  if (cmd != NULL) {
    i2c_cmd_link_delete_static(cmd);
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return ret;
}

esp_err_t i2cRead(uint8_t i2c_num, uint16_t address, uint8_t *buff, size_t size, uint32_t timeOutMillis, size_t *readCount) {
  esp_err_t ret = ESP_FAIL;
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //acquire lock
  if (bus[i2c_num].lock == NULL || xSemaphoreTake(bus[i2c_num].lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return ret;
  }
#endif
  if (!bus[i2c_num].initialized) {
    log_e("bus is not initialized");
  } else {
    ret = i2c_master_read_from_device((i2c_port_t)i2c_num, address, buff, size, timeOutMillis / portTICK_PERIOD_MS);
    if (ret == ESP_OK) {
      *readCount = size;
    } else {
      *readCount = 0;
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return ret;
}

esp_err_t i2cWriteReadNonStop(uint8_t i2c_num, uint16_t address, const uint8_t *wbuff, size_t wsize, uint8_t *rbuff, size_t rsize, uint32_t timeOutMillis, size_t *readCount) {
  esp_err_t ret = ESP_FAIL;
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //acquire lock
  if (bus[i2c_num].lock == NULL || xSemaphoreTake(bus[i2c_num].lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return ret;
  }
#endif
  if (!bus[i2c_num].initialized) {
    log_e("bus is not initialized");
  } else {
    ret = i2c_master_write_read_device((i2c_port_t)i2c_num, address, wbuff, wsize, rbuff, rsize, timeOutMillis / portTICK_PERIOD_MS);
    if (ret == ESP_OK) {
      *readCount = rsize;
    } else {
      *readCount = 0;
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return ret;
}

esp_err_t i2cSetClock(uint8_t i2c_num, uint32_t frequency) {
  esp_err_t ret = ESP_FAIL;
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //acquire lock
  if (bus[i2c_num].lock == NULL || xSemaphoreTake(bus[i2c_num].lock, portMAX_DELAY) != pdTRUE) {
    log_e("could not acquire lock");
    return ret;
  }
#endif
  if (!bus[i2c_num].initialized) {
    log_e("bus is not initialized");
    goto end;
  }
  if (bus[i2c_num].frequency == frequency) {
    ret = ESP_OK;
    goto end;
  }
  if (!frequency) {
    frequency = 100000UL;
  } else if (frequency > 1000000UL) {
    frequency = 1000000UL;
  }

  typedef struct {
    soc_module_clk_t clk; /*!< I2C source clock */
    uint32_t clk_freq;    /*!< I2C source clock frequency */
  } i2c_clk_alloc_t;

  typedef enum {
    I2C_SCLK_DEFAULT = 0, /*!< I2C source clock not selected*/
#if SOC_I2C_SUPPORT_APB
    I2C_SCLK_APB, /*!< I2C source clock from APB, 80M*/
#endif
#if SOC_I2C_SUPPORT_XTAL
    I2C_SCLK_XTAL, /*!< I2C source clock from XTAL, 40M */
#endif
#if SOC_I2C_SUPPORT_RTC
    I2C_SCLK_RTC, /*!< I2C source clock from 8M RTC, 8M */
#endif
#if SOC_I2C_SUPPORT_REF_TICK
    I2C_SCLK_REF_TICK, /*!< I2C source clock from REF_TICK, 1M */
#endif
    I2C_SCLK_MAX,
  } i2c_sclk_t;

  // i2c clock characteristic, The order is the same as i2c_sclk_t.
  i2c_clk_alloc_t i2c_clk_alloc[I2C_SCLK_MAX] = {
    { 0, 0 },
#if SOC_I2C_SUPPORT_APB
    { SOC_MOD_CLK_APB, esp_clk_apb_freq() }, /*!< I2C APB clock characteristic*/
#endif
#if SOC_I2C_SUPPORT_XTAL
    { SOC_MOD_CLK_XTAL, esp_clk_xtal_freq() }, /*!< I2C XTAL characteristic*/
#endif
#if SOC_I2C_SUPPORT_RTC
    { SOC_MOD_CLK_RC_FAST, periph_rtc_dig_clk8m_get_freq() }, /*!< I2C 20M RTC characteristic*/
#endif
#if SOC_I2C_SUPPORT_REF_TICK
    { SOC_MOD_CLK_REF_TICK, REF_CLK_FREQ }, /*!< I2C REF_TICK characteristic*/
#endif
  };

  i2c_sclk_t src_clk = I2C_SCLK_DEFAULT;
  ret = ESP_OK;
  for (i2c_sclk_t clk = I2C_SCLK_DEFAULT + 1; clk < I2C_SCLK_MAX; clk++) {
#if CONFIG_IDF_TARGET_ESP32S3
    if (clk == I2C_SCLK_RTC) {  // RTC clock for s3 is inaccessible now.
      continue;
    }
#endif
    if (frequency <= i2c_clk_alloc[clk].clk_freq) {
      src_clk = clk;
      break;
    }
  }
  if (src_clk == I2C_SCLK_DEFAULT || src_clk == I2C_SCLK_MAX) {
    log_e("clock source could not be selected");
    ret = ESP_FAIL;
  } else {
    i2c_hal_context_t hal;
    hal.dev = I2C_LL_GET_HW(i2c_num);
#if SOC_I2C_SUPPORT_RTC
    if (src_clk == I2C_SCLK_RTC) {
      periph_rtc_dig_clk8m_enable();
    }
#endif
    i2c_hal_set_bus_timing(&(hal), frequency, i2c_clk_alloc[src_clk].clk, i2c_clk_alloc[src_clk].clk_freq);
    bus[i2c_num].frequency = frequency;
    //Clock Stretching Timeout: 20b:esp32, 5b:esp32-c3, 24b:esp32-s2
    i2c_set_timeout((i2c_port_t)i2c_num, I2C_LL_MAX_TIMEOUT);
  }

end:
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return ret;
}

esp_err_t i2cGetClock(uint8_t i2c_num, uint32_t *frequency) {
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!bus[i2c_num].initialized) {
    log_e("bus is not initialized");
    return ESP_FAIL;
  }
  *frequency = bus[i2c_num].frequency;
  return ESP_OK;
}

#endif /* SOC_I2C_SUPPORTED */
