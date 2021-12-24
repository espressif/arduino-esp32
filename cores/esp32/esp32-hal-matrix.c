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

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/gpio.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/gpio.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/gpio.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/gpio.h"
#endif

#define MATRIX_DETACH_OUT_SIG 0x100
#define MATRIX_DETACH_IN_LOW_PIN 0x30
#define MATRIX_DETACH_IN_LOW_HIGH 0x38

void ARDUINO_ISR_ATTR pinMatrixOutAttach(uint8_t pin, uint8_t function, bool invertOut, bool invertEnable)
{
    gpio_matrix_out(pin, function, invertOut, invertEnable);
}

void ARDUINO_ISR_ATTR pinMatrixOutDetach(uint8_t pin, bool invertOut, bool invertEnable)
{
    gpio_matrix_out(pin, MATRIX_DETACH_OUT_SIG, invertOut, invertEnable);
}

void ARDUINO_ISR_ATTR pinMatrixInAttach(uint8_t pin, uint8_t signal, bool inverted)
{
    gpio_matrix_in(pin, signal, inverted);
}

void ARDUINO_ISR_ATTR pinMatrixInDetach(uint8_t signal, bool high, bool inverted)
{
    gpio_matrix_in(high?MATRIX_DETACH_IN_LOW_HIGH:MATRIX_DETACH_IN_LOW_PIN, signal, inverted);
}
/*
void ARDUINO_ISR_ATTR intrMatrixAttach(uint32_t source, uint32_t inum){
  intr_matrix_set(PRO_CPU_NUM, source, inum);
}
*/

