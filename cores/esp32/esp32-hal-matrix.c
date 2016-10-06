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

#include "esp32-hal-matrix.h"
#include "esp_attr.h"
#include "rom/gpio.h"

#define MATRIX_DETACH_OUT_SIG 0x100
#define MATRIX_DETACH_IN_LOW_PIN 0x30
#define MATRIX_DETACH_IN_LOW_HIGH 0x38

void IRAM_ATTR pinMatrixOutAttach(uint8_t pin, uint8_t function, bool invertOut, bool invertEnable)
{
    gpio_matrix_out(pin, function, invertOut, invertEnable);
}

void IRAM_ATTR pinMatrixOutDetach(uint8_t pin, bool invertOut, bool invertEnable)
{
    gpio_matrix_out(pin, MATRIX_DETACH_OUT_SIG, invertOut, invertEnable);
}

void IRAM_ATTR pinMatrixInAttach(uint8_t pin, uint8_t signal, bool inverted)
{
    gpio_matrix_in(pin, signal, inverted);
}

void IRAM_ATTR pinMatrixInDetach(uint8_t signal, bool high, bool inverted)
{
    gpio_matrix_in(high?MATRIX_DETACH_IN_LOW_HIGH:MATRIX_DETACH_IN_LOW_PIN, signal, inverted);
}
/*
void IRAM_ATTR intrMatrixAttach(uint32_t source, uint32_t inum){
  intr_matrix_set(PRO_CPU_NUM, source, inum);
}
*/

