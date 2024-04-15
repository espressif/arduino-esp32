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
// modified Nov 2017 by Chuck Todd <StickBreaker> to support Interrupt Driven I/O
// modified Nov 2021 by Hristo Gochkov <Me-No-Dev> to support ESP-IDF API

#ifndef _ESP32_HAL_I2C_H_
#define _ESP32_HAL_I2C_H_

#include "soc/soc_caps.h"
#if SOC_I2C_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

  esp_err_t i2cInit(uint8_t i2c_num, int8_t sda, int8_t scl, uint32_t clk_speed);
  esp_err_t i2cDeinit(uint8_t i2c_num);
  esp_err_t i2cSetClock(uint8_t i2c_num, uint32_t frequency);
  esp_err_t i2cGetClock(uint8_t i2c_num, uint32_t* frequency);
  esp_err_t i2cWrite(uint8_t i2c_num, uint16_t address, const uint8_t* buff, size_t size, uint32_t timeOutMillis);
  esp_err_t i2cRead(uint8_t i2c_num, uint16_t address, uint8_t* buff, size_t size, uint32_t timeOutMillis, size_t* readCount);
  esp_err_t i2cWriteReadNonStop(uint8_t i2c_num, uint16_t address, const uint8_t* wbuff, size_t wsize, uint8_t* rbuff, size_t rsize, uint32_t timeOutMillis, size_t* readCount);
  bool i2cIsInit(uint8_t i2c_num);

#ifdef __cplusplus
}
#endif

#endif /* SOC_I2C_SUPPORTED */
#endif /* _ESP32_HAL_I2C_H_ */
