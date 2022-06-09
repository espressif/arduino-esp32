#ifndef _ESP_TTS_VOICE_H_
#define _ESP_TTS_VOICE_H_

typedef struct {
	char *voice_name;       // voice set name 
	char *format;           // the format of voice data, currently support pcm and amrwb
	int sample_rate;        // the sample rate of voice data, just for pcm format
	int bit_width;          // the bit width of voice data, just for pcm format
	int syll_num;           // the syllable mumber 
	char **sylls;           // the syllable names
	int *syll_pos;          // the position of syllable in syllable audio data array
	short *pinyin_idx;      // the index of pinyin
	short *phrase_dict;     // the pinyin dictionary of common phrase
	short *extern_idx;      // the idx of extern phrases 
	short *extern_dict;     // the extern phrase dictionary
	unsigned char *data;    // the audio data of all syllables
} esp_tts_voice_t;


#endif