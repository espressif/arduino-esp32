// Copyright 2015-2025 Espressif Systems (Shanghai) PTE LTD
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
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#include "esp32-hal.h"
#if !CONFIG_DISABLE_HAL_LOCKS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif
#include "esp_attr.h"
#include "esp_system.h"
#include "soc/soc_caps.h"
#include "driver/i2c_master.h"
#include "esp32-hal-periman.h"

typedef volatile struct {
  bool initialized;
  uint32_t frequency;
#if !CONFIG_DISABLE_HAL_LOCKS
  SemaphoreHandle_t lock;
#endif
  int8_t scl;
  int8_t sda;
  i2c_master_bus_handle_t bus_handle;
  i2c_master_dev_handle_t dev_handles[128];
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
  esp_err_t ret = ESP_OK;
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
    ret = ESP_FAIL;
    goto init_fail;
  }

  if (!frequency) {
    frequency = 100000UL;
  } else if (frequency > 1000000UL) {
    frequency = 1000000UL;
  }

  perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_MASTER_SDA, i2cDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_MASTER_SCL, i2cDetachBus);

  if (!perimanClearPinBus(sda) || !perimanClearPinBus(scl)) {
    ret = ESP_FAIL;
    goto init_fail;
  }

  log_i("Initializing I2C Master: num=%u sda=%d scl=%d freq=%lu", i2c_num, sda, scl, frequency);

  i2c_master_bus_handle_t bus_handle = NULL;
  i2c_master_bus_config_t bus_config;
  memset(&bus_config, 0, sizeof(i2c_master_bus_config_t));
  bus_config.i2c_port = (i2c_port_num_t)i2c_num;
  bus_config.sda_io_num = (gpio_num_t)sda;
  bus_config.scl_io_num = (gpio_num_t)scl;
#if SOC_LP_I2C_SUPPORTED
  if (i2c_num >= SOC_HP_I2C_NUM) {
    bus_config.lp_source_clk = LP_I2C_SCLK_DEFAULT;
  } else
#endif
  {
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  }
  bus_config.glitch_ignore_cnt = 7;
  bus_config.intr_priority = 0;      // auto
  bus_config.trans_queue_depth = 0;  // only valid in asynchronous transaction, which Arduino does not use
  bus_config.flags.enable_internal_pullup = 1;
#if SOC_I2C_SUPPORT_SLEEP_RETENTION
  bus_config.flags.allow_pd = 1;  // backup/restore the I2C registers before/after entering/exist sleep mode
#endif

  ret = i2c_new_master_bus(&bus_config, &bus_handle);
  if (ret != ESP_OK) {
    log_e("i2c_new_master_bus failed: [%d] %s", ret, esp_err_to_name(ret));
  } else {
    bus[i2c_num].initialized = true;
    bus[i2c_num].frequency = frequency;
    bus[i2c_num].scl = scl;
    bus[i2c_num].sda = sda;
    bus[i2c_num].bus_handle = bus_handle;
    for (uint8_t i = 0; i < 128; i++) {
      bus[i2c_num].dev_handles[i] = NULL;
    }
    if (!perimanSetPinBus(sda, ESP32_BUS_TYPE_I2C_MASTER_SDA, (void *)(i2c_num + 1), i2c_num, -1)
        || !perimanSetPinBus(scl, ESP32_BUS_TYPE_I2C_MASTER_SCL, (void *)(i2c_num + 1), i2c_num, -1)) {
#if !CONFIG_DISABLE_HAL_LOCKS
      //release lock so that i2cDetachBus can execute i2cDeinit
      xSemaphoreGive(bus[i2c_num].lock);
#endif
      i2cDetachBus((void *)(i2c_num + 1));
      return ESP_FAIL;
    }
  }

init_fail:
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
    // remove devices from the bus
    for (uint8_t i = 0; i < 128; i++) {
      if (bus[i2c_num].dev_handles[i] != NULL) {
        err = i2c_master_bus_rm_device(bus[i2c_num].dev_handles[i]);
        bus[i2c_num].dev_handles[i] = NULL;
        if (err != ESP_OK) {
          log_e("i2c_master_bus_rm_device failed: [%d] %s", err, esp_err_to_name(err));
        }
      }
    }
    err = i2c_del_master_bus(bus[i2c_num].bus_handle);
    if (err != ESP_OK) {
      log_e("i2c_del_master_bus failed: [%d] %s", err, esp_err_to_name(err));
    } else {
      bus[i2c_num].initialized = false;
      perimanClearPinBus(bus[i2c_num].scl);
      perimanClearPinBus(bus[i2c_num].sda);
      bus[i2c_num].scl = -1;
      bus[i2c_num].sda = -1;
      bus[i2c_num].bus_handle = NULL;
    }
  }
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return err;
}

static esp_err_t i2cAddDeviceIfNeeded(uint8_t i2c_num, uint16_t address) {
  esp_err_t ret = ESP_OK;
  if (bus[i2c_num].dev_handles[address] == NULL) {
    i2c_master_dev_handle_t dev_handle = NULL;
    i2c_device_config_t dev_config;
    memset(&dev_config, 0, sizeof(i2c_device_config_t));
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;  // Arduino supports only 7bit addresses
    dev_config.device_address = address;
    dev_config.scl_speed_hz = bus[i2c_num].frequency;
    dev_config.scl_wait_us = 0;
    dev_config.flags.disable_ack_check = 0;

    ret = i2c_master_bus_add_device(bus[i2c_num].bus_handle, &dev_config, &dev_handle);
    if (ret != ESP_OK) {
      log_e("i2c_master_bus_add_device failed: [%d] %s", ret, esp_err_to_name(ret));
    } else {
      bus[i2c_num].dev_handles[address] = dev_handle;
      log_v("added device: bus=%u addr=0x%x handle=0x%08x", i2c_num, address, dev_handle);
    }
  }
  return ret;
}

esp_err_t i2cWrite(uint8_t i2c_num, uint16_t address, const uint8_t *buff, size_t size, uint32_t timeOutMillis) {
  esp_err_t ret = ESP_FAIL;
  // i2c_cmd_handle_t cmd = NULL;
  if (i2c_num >= SOC_I2C_NUM) {
    return ESP_ERR_INVALID_ARG;
  }
  if (address >= 128) {
    log_e("Only 7bit I2C addresses are supported");
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

  if (size == 0) {
    // Probe device
    ret = i2c_master_probe(bus[i2c_num].bus_handle, address, timeOutMillis);
    if (ret != ESP_OK) {
      log_v("i2c_master_probe failed: [%d] %s", ret, esp_err_to_name(ret));
    }
  } else {
    // writing data to device
    ret = i2cAddDeviceIfNeeded(i2c_num, address);
    if (ret != ESP_OK) {
      goto end;
    }

    log_v("i2c_master_transmit: bus=%u addr=0x%x handle=0x%08x size=%u", i2c_num, address, bus[i2c_num].dev_handles[address], size);
    ret = i2c_master_transmit(bus[i2c_num].dev_handles[address], buff, size, timeOutMillis);
    if (ret != ESP_OK) {
      log_e("i2c_master_transmit failed: [%d] %s", ret, esp_err_to_name(ret));
      goto end;
    }

    // wait for transactions to finish (is it needed with sync transactions?)
    // ret = i2c_master_bus_wait_all_done(bus[i2c_num].bus_handle, timeOutMillis);
    // if (ret != ESP_OK) {
    //   log_e("i2c_master_bus_wait_all_done failed: [%d] %s", ret, esp_err_to_name(ret));
    //   goto end;
    // }
  }

end:
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return ret;
}

esp_err_t i2cRead(uint8_t i2c_num, uint16_t address, uint8_t *buff, size_t size, uint32_t timeOutMillis, size_t *readCount) {
  esp_err_t ret = ESP_FAIL;
  *readCount = 0;
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

  ret = i2cAddDeviceIfNeeded(i2c_num, address);
  if (ret != ESP_OK) {
    goto end;
  }

  log_v("i2c_master_receive: bus=%u addr=0x%x handle=0x%08x size=%u", i2c_num, address, bus[i2c_num].dev_handles[address], size);
  ret = i2c_master_receive(bus[i2c_num].dev_handles[address], buff, size, timeOutMillis);
  if (ret != ESP_OK) {
    log_e("i2c_master_receive failed: [%d] %s", ret, esp_err_to_name(ret));
    goto end;
  }

  // wait for transactions to finish (is it needed with sync transactions?)
  // ret = i2c_master_bus_wait_all_done(bus[i2c_num].bus_handle, timeOutMillis);
  // if (ret != ESP_OK) {
  //   log_e("i2c_master_bus_wait_all_done failed: [%d] %s", ret, esp_err_to_name(ret));
  //   goto end;
  // }
  *readCount = size;

end:
#if !CONFIG_DISABLE_HAL_LOCKS
  //release lock
  xSemaphoreGive(bus[i2c_num].lock);
#endif
  return ret;
}

esp_err_t i2cWriteReadNonStop(
  uint8_t i2c_num, uint16_t address, const uint8_t *wbuff, size_t wsize, uint8_t *rbuff, size_t rsize, uint32_t timeOutMillis, size_t *readCount
) {
  esp_err_t ret = ESP_FAIL;
  *readCount = 0;
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

  ret = i2cAddDeviceIfNeeded(i2c_num, address);
  if (ret != ESP_OK) {
    goto end;
  }

  log_v("i2c_master_transmit_receive: bus=%u addr=0x%x handle=0x%08x write=%u read=%u", i2c_num, address, bus[i2c_num].dev_handles[address], wsize, rsize);
  ret = i2c_master_transmit_receive(bus[i2c_num].dev_handles[address], wbuff, wsize, rbuff, rsize, timeOutMillis);
  if (ret != ESP_OK) {
    log_e("i2c_master_transmit_receive failed: [%d] %s", ret, esp_err_to_name(ret));
    goto end;
  }

  // wait for transactions to finish (is it needed with sync transactions?)
  // ret = i2c_master_bus_wait_all_done(bus[i2c_num].bus_handle, timeOutMillis);
  // if (ret != ESP_OK) {
  //   log_e("i2c_master_bus_wait_all_done failed: [%d] %s", ret, esp_err_to_name(ret));
  //   goto end;
  // }
  *readCount = rsize;

end:
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

  bus[i2c_num].frequency = frequency;

  // loop through devices, remove them and then re-add them with the new frequency
  for (uint8_t i = 0; i < 128; i++) {
    if (bus[i2c_num].dev_handles[i] != NULL) {
      ret = i2c_master_bus_rm_device(bus[i2c_num].dev_handles[i]);
      if (ret != ESP_OK) {
        log_e("i2c_master_bus_rm_device failed: [%d] %s", ret, esp_err_to_name(ret));
        goto end;
      } else {
        bus[i2c_num].dev_handles[i] = NULL;
        ret = i2cAddDeviceIfNeeded(i2c_num, i);
        if (ret != ESP_OK) {
          goto end;
        }
      }
    }
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

#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) */
#endif /* SOC_I2C_SUPPORTED */
