#pragma once

#define SR_FLASH_TYPE   32
#define SR_FLASH_SUBTYPE   32
#define SR_FLASH_PARTITION_NAME "fr"
#define SR_FLASH_INFO_FLAG 12138

int8_t speech_command_flash_init(void);
int8_t enroll_speech_command_to_flash_with_id(char *phrase, int mn_command_id);
int get_use_flag_from_flash();
int get_enroll_num_from_flash();
char *read_speech_command_from_flash(int i);