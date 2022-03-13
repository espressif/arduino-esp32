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

#include "firmware_msc_fat.h"
//copy up to max_len chars from src to dst and do not terminate
static size_t cplstr(void *dst, const void * src, size_t max_len){
    if(!src || !dst || !max_len){
        return 0;
    }
    size_t l = strlen((const char *)src);
    if(l > max_len){
        l = max_len;
    }
    memcpy(dst, src, l);
    return l;
}

//copy up to max_len chars from src to dst, adding spaces up to max_len. do not terminate
static void cplstrsp(void *dst, const void * src, size_t max_len){
    size_t l = cplstr(dst, src, max_len);
    for(; l < max_len; l++){
      ((uint8_t*)dst)[l] = 0x20;
    }
}

// FAT12
static const char * FAT12_FILE_SYSTEM_TYPE = "FAT12";

static uint16_t fat12_sectors_per_alloc_table(uint32_t sector_num){
  uint32_t required_bytes = (((sector_num * 3)+1)/2);
  return (required_bytes / DISK_SECTOR_SIZE) + ((required_bytes & (DISK_SECTOR_SIZE - 1))?1:0);
}

static uint8_t * fat12_add_table(uint8_t * dst, fat_boot_sector_t * boot){
  memset(dst+DISK_SECTOR_SIZE, 0, boot->sectors_per_alloc_table * DISK_SECTOR_SIZE);
  uint8_t * d = dst + DISK_SECTOR_SIZE;
  d[0] = 0xF8;
  d[1] = 0xFF;
  d[2] = 0xFF;
  return d;
}

static void fat12_set_table_index(uint8_t * table, uint16_t index, uint16_t value){
  uint16_t offset = (index >> 1) * 3;
  uint8_t * data = table + offset;
  if(index & 1){
    data[2] = (value >> 4) & 0xFF;
    data[1] = (data[1] & 0xF) | ((value & 0xF) << 4);
  } else {
    data[0] = value & 0xFF;
    data[1] = (data[1] & 0xF0) | ((value >> 8) & 0xF);
  }
}

//FAT16
static const char * FAT16_FILE_SYSTEM_TYPE = "FAT16";

static uint16_t fat16_sectors_per_alloc_table(uint32_t sector_num){
  uint32_t required_bytes = sector_num * 2;
  return (required_bytes / DISK_SECTOR_SIZE) + ((required_bytes & (DISK_SECTOR_SIZE - 1))?1:0);
}

static uint8_t * fat16_add_table(uint8_t * dst, fat_boot_sector_t * boot){
  memset(dst+DISK_SECTOR_SIZE, 0, boot->sectors_per_alloc_table * DISK_SECTOR_SIZE);
  uint16_t * d = (uint16_t *)(dst + DISK_SECTOR_SIZE);
  d[0] = 0xFFF8;
  d[1] = 0xFFFF;
  return (uint8_t *)d;
}

static void fat16_set_table_index(uint8_t * table, uint16_t index, uint16_t value){
  uint16_t offset = index * 2;
  *(uint16_t *)(table + offset) = value;
}

//Interface
const char * fat_file_system_type(bool fat16) {
	return ((fat16)?FAT16_FILE_SYSTEM_TYPE:FAT12_FILE_SYSTEM_TYPE);
}

uint16_t fat_sectors_per_alloc_table(uint32_t sector_num, bool fat16){
  if(fat16){
    return fat16_sectors_per_alloc_table(sector_num);
  }
  return fat12_sectors_per_alloc_table(sector_num);
}

uint8_t * fat_add_table(uint8_t * dst, fat_boot_sector_t * boot, bool fat16){
  if(fat16){
    return fat16_add_table(dst, boot);
  }
  return fat12_add_table(dst, boot);
}

void fat_set_table_index(uint8_t * table, uint16_t index, uint16_t value, bool fat16){
  if(fat16){
    fat16_set_table_index(table, index, value);
  } else {
    fat12_set_table_index(table, index, value);
  }
}

fat_boot_sector_t * fat_add_boot_sector(uint8_t * dst, uint16_t sector_num, uint16_t table_sectors, const char * file_system_type, const char * volume_label, uint32_t serial_number){
  fat_boot_sector_t *boot = (fat_boot_sector_t*)dst;
  boot->jump_instruction[0] = 0xEB;
  boot->jump_instruction[1] = 0x3C;
  boot->jump_instruction[2] = 0x90;
  cplstr(boot->oem_name, "MSDOS5.0", 8);
  boot->bytes_per_sector = DISK_SECTOR_SIZE;
  boot->sectors_per_cluster = 1;
  boot->reserved_sectors_count = 1;
  boot->file_alloc_tables_num = 1;
  boot->max_root_dir_entries = 16;
  boot->fat12_sector_num = sector_num;
  boot->media_descriptor = 0xF8;
  boot->sectors_per_alloc_table = table_sectors;
  boot->sectors_per_track = 1;
  boot->num_heads = 1;
  boot->hidden_sectors_count = 0;
  boot->total_sectors_32 = 0;
  boot->physical_drive_number = 0x80;
  boot->reserved0 = 0x00;
  boot->extended_boot_signature = 0x29;
  boot->serial_number = serial_number;
  cplstrsp(boot->volume_label, volume_label, 11);
  memset(boot->reserved, 0, 448);
  cplstrsp(boot->file_system_type, file_system_type, 8);
  boot->signature = 0xAA55;
  return boot;
}

fat_dir_entry_t * fat_add_label(uint8_t * dst, const char * volume_label){
  fat_boot_sector_t * boot = (fat_boot_sector_t *)dst;
  fat_dir_entry_t * entry = (fat_dir_entry_t *)(dst + ((boot->sectors_per_alloc_table+1) * DISK_SECTOR_SIZE));
  memset(entry, 0, sizeof(fat_dir_entry_t));
  cplstrsp(entry->volume_label, volume_label, 11);
  entry->file_attr = FAT_FILE_ATTR_VOLUME_LABEL;
  return entry;
}

fat_dir_entry_t * fat_add_root_file(uint8_t * dst, uint8_t index, const char * file_name, const char * file_extension, size_t file_size, uint16_t data_start_sector, bool is_fat16){
  fat_boot_sector_t * boot = (fat_boot_sector_t *)dst;
  uint8_t * table = dst + DISK_SECTOR_SIZE;
  fat_dir_entry_t * entry = (fat_dir_entry_t *)(dst + ((boot->sectors_per_alloc_table+1) * DISK_SECTOR_SIZE) + (index * sizeof(fat_dir_entry_t)));
  memset(entry, 0, sizeof(fat_dir_entry_t));
  cplstrsp(entry->file_name, file_name, 8);
  cplstrsp(entry->file_extension, file_extension, 3);
  entry->file_attr = FAT_FILE_ATTR_ARCHIVE;
  entry->file_size = file_size;
  entry->data_start_sector = data_start_sector;
  entry->extended_attr = 0;

  uint16_t file_sectors = file_size / DISK_SECTOR_SIZE;
  if(file_size % DISK_SECTOR_SIZE){
    file_sectors++;
  }

  uint16_t data_end_sector = data_start_sector + file_sectors;
  for(uint16_t i=data_start_sector; i<(data_end_sector-1); i++){
    fat_set_table_index(table, i, i+1, is_fat16);
  }
  fat_set_table_index(table, data_end_sector-1, 0xFFFF, is_fat16);

  //Set Firmware Date based on the build time
  static const char * month_names_short[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char mstr[8] = {'\0',};
  const char *str = __DATE__ " " __TIME__;
  int ms=0, seconds=0, minutes=0, hours=0, year=0, date=0, month=0;
  int r = sscanf(str,"%s %d %d %d:%d:%d", mstr, &date, &year, &hours, &minutes, &seconds);
  if(r >= 0){
    for(int i=0; i<12; i++){
      if(!strcmp(mstr, month_names_short[i])){
        month = i;
        break;
      }
    }
    entry->creation_time_ms = FAT_MS2V(seconds, ms);
    entry->creation_time_hms = FAT_HMS2V(hours, minutes, seconds);
    entry->creation_time_ymd = FAT_YMD2V(year, month, date);
    entry->last_access_ymd = entry->creation_time_ymd;
    entry->last_modified_hms = entry->creation_time_hms;
    entry->last_modified_ymd = entry->creation_time_ymd;
  }
  return entry;
}

uint8_t fat_lfn_checksum(const uint8_t *short_filename){
   uint8_t sum = 0;
   for (uint8_t i = 11; i; i--) {
      sum = ((sum & 1) << 7) + (sum >> 1) + *short_filename++;
   }
   return sum;
}
