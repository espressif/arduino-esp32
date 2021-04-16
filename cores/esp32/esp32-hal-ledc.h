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

#ifndef _ESP32_HAL_LEDC_H_
#define _ESP32_HAL_LEDC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    NOTE_C, NOTE_Cs, NOTE_D, NOTE_Eb, NOTE_E, NOTE_F, NOTE_Fs, NOTE_G, NOTE_Gs, NOTE_A, NOTE_Bb, NOTE_B, NOTE_MAX
} note_t;

//channel 0-15 resolution 1-16bits freq limits depend on resolution
double      ledcSetup(uint8_t channel, double freq, uint8_t resolution_bits);
void        ledcWrite(uint8_t channel, uint32_t duty);
double      ledcWriteTone(uint8_t channel, double freq);
double      ledcWriteNote(uint8_t channel, note_t note, uint8_t octave);
uint32_t    ledcRead(uint8_t channel);
double      ledcReadFreq(uint8_t channel);
void        ledcAttachPin(uint8_t pin, uint8_t channel);
void        ledcDetachPin(uint8_t pin);
double      ledcChangeFrequency(uint8_t channel, double freq, uint8_t resolution_bits);


#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_LEDC_H_ */
