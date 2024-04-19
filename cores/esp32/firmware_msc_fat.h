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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAT_U8(v)  ((v) & 0xFF)
#define FAT_U16(v) FAT_U8(v), FAT_U8((v) >> 8)
#define FAT_U32(v) FAT_U8(v), FAT_U8((v) >> 8), FAT_U8((v) >> 16), FAT_U8((v) >> 24)

#define FAT12_TBL2B(l, h) FAT_U8(l), FAT_U8(((l >> 8) & 0xF) | ((h << 4) & 0xF0)), FAT_U8(h >> 4)

#define FAT_MS2B(s, ms)    FAT_U8(((((s) & 0x1) * 1000) + (ms)) / 10)
#define FAT_HMS2B(h, m, s) FAT_U8(((s) >> 1) | (((m) & 0x7) << 5)), FAT_U8((((m) >> 3) & 0x7) | ((h) << 3))
#define FAT_YMD2B(y, m, d) FAT_U8(((d) & 0x1F) | (((m) & 0x7) << 5)), FAT_U8((((m) >> 3) & 0x1) | ((((y) - 1980) & 0x7F) << 1))

#define FAT_MS2V(s, ms)    FAT_U8(((((s) & 0x1) * 1000) + (ms)) / 10)
#define FAT_HMS2V(h, m, s) (FAT_U8(((s) >> 1) | (((m) & 0x7) << 5)) | (FAT_U8((((m) >> 3) & 0x7) | ((h) << 3)) << 8))
#define FAT_YMD2V(y, m, d) (FAT_U8(((d) & 0x1F) | (((m) & 0x7) << 5)) | (FAT_U8((((m) >> 3) & 0x1) | ((((y) - 1980) & 0x7F) << 1)) << 8))

#define FAT_B2HMS(hms) ((hms >> 11) & 0x1F), ((hms >> 5) & 0x3F), ((hms & 0x1F) << 1)
#define FAT_B2YMD(ymd) (((ymd >> 9) & 0x7F) + 1980), ((ymd >> 5) & 0x0F), (ymd & 0x1F)

#define FAT_FILE_ATTR_READ_ONLY    0x01
#define FAT_FILE_ATTR_HIDDEN       0x02
#define FAT_FILE_ATTR_SYSTEM       0x04
#define FAT_FILE_ATTR_VOLUME_LABEL 0x08
#define FAT_FILE_ATTR_SUBDIRECTORY 0x10
#define FAT_FILE_ATTR_ARCHIVE      0x20
#define FAT_FILE_ATTR_DEVICE       0x40

static const uint16_t DISK_SECTOR_SIZE = 512;

#define FAT_SIZE_TO_SECTORS(bytes) ((bytes) / DISK_SECTOR_SIZE) + (((bytes) % DISK_SECTOR_SIZE) ? 1 : 0)

typedef struct __attribute__((packed)) {
  uint8_t jump_instruction[3];
  char oem_name[8];                 //padded with spaces (0x20)
  uint16_t bytes_per_sector;        //DISK_SECTOR_SIZE usually 512
  uint8_t sectors_per_cluster;      //Allowed values are 1, 2, 4, 8, 16, 32, 64, and 128
  uint16_t reserved_sectors_count;  //At least 1 for this sector, usually 32 for FAT32
  uint8_t file_alloc_tables_num;    //Almost always 2; RAM disks might use 1
  uint16_t max_root_dir_entries;    //FAT12 and FAT16
  uint16_t fat12_sector_num;        //DISK_SECTOR_NUM FAT12 and FAT16
  uint8_t media_descriptor;
  uint16_t sectors_per_alloc_table;  //FAT12 and FAT16
  uint16_t sectors_per_track;        //A value of 0 may indicate LBA-only access
  uint16_t num_heads;
  uint32_t hidden_sectors_count;
  uint32_t total_sectors_32;
  uint8_t physical_drive_number;  //0x00 for (first) removable media, 0x80 for (first) fixed disk
  uint8_t reserved0;
  uint8_t extended_boot_signature;  //should be 0x29
  uint32_t serial_number;           //0x1234 => 1234
  char volume_label[11];            //padded with spaces (0x20)
  char file_system_type[8];         //padded with spaces (0x20)
  uint8_t reserved[448];
  uint16_t signature;  //should be 0xAA55
} fat_boot_sector_t;

typedef struct __attribute__((packed)) {
  union {
    struct {
      char file_name[8];       //padded with spaces (0x20)
      char file_extension[3];  //padded with spaces (0x20)
    };
    struct {
      uint8_t file_magic;  // 0xE5:deleted, 0x05:will_be_deleted, 0x00:end_marker, 0x2E:dot_marker(. or ..)
      char file_magic_data[10];
    };
    char volume_label[11];  //padded with spaces (0x20)
  };
  uint8_t file_attr;           //mask of FAT_FILE_ATTR_*
  uint8_t reserved;            //always 0
  uint8_t creation_time_ms;    //ms * 10; max 1990 (1s 990ms)
  uint16_t creation_time_hms;  // [5:6:5] => h:m:(s/2)
  uint16_t creation_time_ymd;  // [7:4:5] => (y+1980):m:d
  uint16_t last_access_ymd;
  uint16_t extended_attr;
  uint16_t last_modified_hms;
  uint16_t last_modified_ymd;
  uint16_t data_start_sector;
  uint32_t file_size;
} fat_dir_entry_t;

typedef struct __attribute__((packed)) {
  union {
    struct {
      uint8_t number    : 5;
      uint8_t reserved0 : 1;
      uint8_t llfp      : 1;
      uint8_t reserved1 : 1;
    } seq;
    uint8_t seq_num;  //0xE5: Deleted Entry
  };
  uint16_t name0[5];
  uint8_t attr;  //ALWAYS 0x0F
  uint8_t type;  //ALWAYS 0x00
  uint8_t dos_checksum;
  uint16_t name1[6];
  uint16_t first_cluster;  //ALWAYS 0x0000
  uint16_t name2[2];
} fat_lfn_entry_t;

typedef union {
  fat_dir_entry_t dir;
  fat_lfn_entry_t lfn;
} fat_entry_t;

const char *fat_file_system_type(bool fat16);
uint16_t fat_sectors_per_alloc_table(uint32_t sector_num, bool fat16);
uint8_t *fat_add_table(uint8_t *dst, fat_boot_sector_t *boot, bool fat16);
void fat_set_table_index(uint8_t *table, uint16_t index, uint16_t value, bool fat16);
fat_boot_sector_t *fat_add_boot_sector(
  uint8_t *dst, uint16_t sector_num, uint16_t table_sectors, const char *file_system_type, const char *volume_label, uint32_t serial_number
);
fat_dir_entry_t *fat_add_label(uint8_t *dst, const char *volume_label);
fat_dir_entry_t *fat_add_root_file(
  uint8_t *dst, uint8_t index, const char *file_name, const char *file_extension, size_t file_size, uint16_t data_start_sector, bool is_fat16
);
uint8_t fat_lfn_checksum(const uint8_t *short_filename);

#ifdef __cplusplus
}
#endif
