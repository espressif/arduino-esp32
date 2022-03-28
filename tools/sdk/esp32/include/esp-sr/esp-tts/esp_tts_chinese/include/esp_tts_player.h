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
#ifndef _ESP_TTS_PLAYER_H_
#define _ESP_TTS_PLAYER_H_

#include "stdlib.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void * esp_tts_player_handle_t;

/**
 * @brief Creates an instance of the TTS Player structure.
 *
 * @param mode      mode of player, default:0 
 * @return
 *         - NULL: Create failed
 *         - Others: The instance of  TTS Player
 */
esp_tts_player_handle_t esp_tts_player_create(int mode);



/**
 * @brief Concatenate audio files. 
 *
 * @Warning Just support mono audio data.
 *
 * @param player       The handle of TTS player
 * @param file_list    The dir of files
 * @param file_num     The number of file    
 * @param len          The length of return audio buffer 
 * @param sample_rate  The sample rate of input audio file
 * @param sample_width The sample width of input audio file, sample_width=1:8-bit, sample_width=2:16-bit,...
 * @return
 *        - audio data buffer
 */
unsigned char* esp_tts_stream_play_by_concat(esp_tts_player_handle_t player, const char **file_list, int file_num, int *len, int *sample_rate, int *sample_width);


/**
 * @brief Free the TTS Player instance
 *
 * @param player The instance of TTS Player. 
 */
void esp_tts_player_destroy(esp_tts_player_handle_t player);

#ifdef __cplusplus
extern "C" {
#endif

#endif