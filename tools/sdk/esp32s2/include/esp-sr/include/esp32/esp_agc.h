// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
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
// limitations under the License
#ifndef _ESP_AGC_H_
#define _ESP_AGC_H_

////all positive value is valid, negective is error
typedef enum {
    ESP_AGC_SUCCESS = 0,   ////success
    ESP_AGC_FAIL = -1, ////agc fail
    ESP_AGC_SAMPLE_RATE_ERROR = -2,  ///sample rate can be only 8khz, 16khz, 32khz
    ESP_AGC_FRAME_SIZE_ERROR = -3,   ////the input frame size should be only 10ms, so should together with sample-rate to get the frame size
} ESP_AGE_ERR;


void *esp_agc_open(int agc_mode, int sample_rate);
void set_agc_config(void *agc_handle, int gain_dB, int limiter_enable, int target_level_dbfs);
int esp_agc_process(void *agc_handle, short *in_pcm, short *out_pcm, int frame_size, int sample_rate);
void esp_agc_close(void *agc_handle);

#endif // _ESP_AGC_H_
