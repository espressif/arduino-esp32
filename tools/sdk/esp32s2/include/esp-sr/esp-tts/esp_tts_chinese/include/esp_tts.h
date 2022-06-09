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
#ifndef _ESP_TTS_H_
#define _ESP_TTS_H_

#include "stdlib.h"
#include "stdio.h"
#include "esp_tts_voice.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	NONE_MODE      = 0,    //do not play any word before playing a specific number
	ALI_PAY_MODE,          //play zhi fu bao shou kuan before playing a specific number
	WEIXIN_PAY_MODE        //play wei xin shou kuan before playing a specific number
} pay_mode_t;

typedef void * esp_tts_handle_t;


/**
 * @brief Init an instance of the TTS voice set structure.
 *
 * @param template      The const esp_tts_voice_template.
 * @param data          The customize voice data
 * @return
 *         - NULL: Init failed
 *         - Others: The instance of voice set
 */
esp_tts_voice_t *esp_tts_voice_set_init(const esp_tts_voice_t *template, void *data);

/**
 * @brief Init an instance of the TTS voice set structure.
 *
 * @param template      The const esp_tts_voice_template.
 * @param data          The customize voice data
 * @return
 *         - NULL: Init failed
 *         - Others: The instance of voice set
 */
void esp_tts_voice_set_free(esp_tts_voice_t *voice);

/**
 * @brief Creates an instance of the TTS structure.
 *
 * @param voice      Voice set containing all basic phonemes.
 * @return
 *         - NULL: Create failed
 *         - Others: The instance of TTS structure
 */
esp_tts_handle_t esp_tts_create(esp_tts_voice_t *voice);

/**
 * @brief parse money pronuciation.
 *
 * @param tts_handle   Instance of TTS
 * @param yuan         The number of yuan    
 * @param jiao         The number of jiao
 * @param fen          The number of fen   
 * @param mode         The pay mode: please refer to pay_mode_t
 * @return
 *        - 0: failed
 *        - 1: succeeded
 */
int esp_tts_parse_money(esp_tts_handle_t tts_handle, int yuan, int jiao, int fen, pay_mode_t mode);

/**
 * @brief parse Chinese PinYin pronuciation.
 *
 * @param tts_handle   Instance of TTS
 * @param pinyin       PinYin string, like this "da4 jia1 hao3"    
 * @return
 *         - 0: failed
 *         - 1: succeeded
 */
int esp_tts_parse_pinyin(esp_tts_handle_t tts_handle, const char *pinyin);

/**
 * @brief parse Chinese string.
 *
 * @param tts_handle   Instance of TTS
 * @param str          Chinese string, like this "大家好"    
 * @return
 *         - 0: failed
 *         - 1: succeeded
 */
int esp_tts_parse_chinese(esp_tts_handle_t tts_handle, const char *str);

/**
 * @brief output TTS voice data by stream.
 *
 * @Warning The output data should not be freed. 
            Once the output length is 0, the all voice data has been output.  
 *
 * @param tts_handle   Instance of TTS
 * @param len          The length of output data 
 * @param speed        The speech speed speed of synthesized speech, 
                       range:0~5, 0: the slowest speed, 5: the fastest speech 
 * @return
 *        - voice raw data
 */
short* esp_tts_stream_play(esp_tts_handle_t tts_handle, int *len, unsigned int speed);

/**
 * @brief reset tts stream and clean all cache of TTS instance.
 *
 * @param tts_handle   Instance of TTS
 */
void esp_tts_stream_reset(esp_tts_handle_t tts_handle);

/**
 * @brief Free the TTS instance
 *
 * @param tts_handle The instance of TTS. 
 */
void esp_tts_destroy(esp_tts_handle_t tts_handle);

#ifdef __cplusplus
extern "C" {
#endif

#endif