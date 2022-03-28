#ifndef _ESP_TTS_PARSER_H_
#define _ESP_TTS_PARSER_H_

#include "stdlib.h"
#include "esp_tts_voice.h"


typedef struct {
	int *syll_idx;
	int syll_num;
	int total_num;
	esp_tts_voice_t *voice;
}esp_tts_utt_t;

esp_tts_utt_t* esp_tts_parser_chinese   (const char* str, esp_tts_voice_t *voice);

esp_tts_utt_t* esp_tts_parser_money(char *play_tag, int yuan, int jiao, int fen, esp_tts_voice_t *voice);

esp_tts_utt_t* esp_tts_parser_pinyin(char* pinyin, esp_tts_voice_t *voice);

esp_tts_utt_t* esp_tts_utt_alloc(int syll_num, esp_tts_voice_t *voice);

void esp_tts_utt_free(esp_tts_utt_t *utt);

#endif