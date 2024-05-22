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

#pragma once

#include "soc/soc_caps.h"
#if SOC_I2C_SUPPORT_SLAVE

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stddef.h"
#include "esp_err.h"

typedef void (*i2c_slave_request_cb_t)(uint8_t num, void *arg);
typedef void (*i2c_slave_receive_cb_t)(uint8_t num, uint8_t *data, size_t len, bool stop, void *arg);
esp_err_t i2cSlaveAttachCallbacks(uint8_t num, i2c_slave_request_cb_t request_callback, i2c_slave_receive_cb_t receive_callback, void *arg);

esp_err_t i2cSlaveInit(uint8_t num, int sda, int scl, uint16_t slaveID, uint32_t frequency, size_t rx_len, size_t tx_len);
esp_err_t i2cSlaveDeinit(uint8_t num);
size_t i2cSlaveWrite(uint8_t num, const uint8_t *buf, uint32_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SOC_I2C_SUPPORT_SLAVE */
