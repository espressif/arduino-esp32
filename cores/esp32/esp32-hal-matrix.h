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

#ifndef _ESP32_HAL_MATRIX_H_
#define _ESP32_HAL_MATRIX_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"
#include "soc/gpio_sig_map.h"

void pinMatrixOutAttach(uint8_t pin, uint8_t function, bool invertOut, bool invertEnable);
void pinMatrixOutDetach(uint8_t pin, bool invertOut, bool invertEnable);
void pinMatrixInAttach(uint8_t pin, uint8_t signal, bool inverted);
void pinMatrixInDetach(uint8_t signal, bool high, bool inverted);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_ARDUHAL_INCLUDE_ESP32_HAL_MATRIX_H_ */
