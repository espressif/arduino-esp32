#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 1
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else
#include "USB.h"
#include "USBMSC.h"

USBMSC MSC;

#define FAT_U8(v)          ((v) & 0xFF)
#define FAT_U16(v)         FAT_U8(v), FAT_U8((v) >> 8)
#define FAT_U32(v)         FAT_U8(v), FAT_U8((v) >> 8), FAT_U8((v) >> 16), FAT_U8((v) >> 24)
#define FAT_MS2B(s, ms)    FAT_U8(((((s) & 0x1) * 1000) + (ms)) / 10)
#define FAT_HMS2B(h, m, s) FAT_U8(((s) >> 1) | (((m) & 0x7) << 5)), FAT_U8((((m) >> 3) & 0x7) | ((h) << 3))
#define FAT_YMD2B(y, m, d) FAT_U8(((d) & 0x1F) | (((m) & 0x7) << 5)), FAT_U8((((m) >> 3) & 0x1) | ((((y) - 1980) & 0x7F) << 1))
#define FAT_TBL2B(l, h)    FAT_U8(l), FAT_U8(((l >> 8) & 0xF) | ((h << 4) & 0xF0)), FAT_U8(h >> 4)

#define README_CONTENTS \
  "This is tinyusb's MassStorage Class demo.\r\n\r\nIf you find any bugs or get any questions, feel free to file an\r\nissue at github.com/hathach/tinyusb"

static const uint32_t DISK_SECTOR_COUNT = 2 * 8;   // 8KB is the smallest size that windows allow to mount
static const uint16_t DISK_SECTOR_SIZE = 512;      // Should be 512
static const uint16_t DISC_SECTORS_PER_TABLE = 1;  //each table sector can fit 170KB (340 sectors)

static uint8_t msc_disk[DISK_SECTOR_COUNT][DISK_SECTOR_SIZE] = {
  //------------- Block0: Boot Sector -------------//
  {                                                        // Header (62 bytes)
   0xEB, 0x3C, 0x90,                                       //jump_instruction
   'M', 'S', 'D', 'O', 'S', '5', '.', '0',                 //oem_name
   FAT_U16(DISK_SECTOR_SIZE),                              //bytes_per_sector
   FAT_U8(1),                                              //sectors_per_cluster
   FAT_U16(1),                                             //reserved_sectors_count
   FAT_U8(1),                                              //file_alloc_tables_num
   FAT_U16(16),                                            //max_root_dir_entries
   FAT_U16(DISK_SECTOR_COUNT),                             //fat12_sector_num
   0xF8,                                                   //media_descriptor
   FAT_U16(DISC_SECTORS_PER_TABLE),                        //sectors_per_alloc_table;//FAT12 and FAT16
   FAT_U16(1),                                             //sectors_per_track;//A value of 0 may indicate LBA-only access
   FAT_U16(1),                                             //num_heads
   FAT_U32(0),                                             //hidden_sectors_count
   FAT_U32(0),                                             //total_sectors_32
   0x00,                                                   //physical_drive_number;0x00 for (first) removable media, 0x80 for (first) fixed disk
   0x00,                                                   //reserved
   0x29,                                                   //extended_boot_signature;//should be 0x29
   FAT_U32(0x1234),                                        //serial_number: 0x1234 => 1234
   'T', 'i', 'n', 'y', 'U', 'S', 'B', ' ', 'M', 'S', 'C',  //volume_label padded with spaces (0x20)
   'F', 'A', 'T', '1', '2', ' ', ' ', ' ',                 //file_system_type padded with spaces (0x20)

   // Zero up to 2 last bytes of FAT magic code (448 bytes)
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

   //boot signature (2 bytes)
   0x55, 0xAA
  },

  //------------- Block1: FAT12 Table -------------//
  {
    FAT_TBL2B(0xFF8, 0xFFF), FAT_TBL2B(0xFFF, 0x000)  // first 2 entries must be 0xFF8 0xFFF, third entry is cluster end of readme file
  },

  //------------- Block2: Root Directory -------------//
  {
    // first entry is volume label
    'E', 'S', 'P', '3', '2', 'S', '2', ' ', 'M', 'S', 'C',
    0x08,                                                                                                                 //FILE_ATTR_VOLUME_LABEL
    0x00, FAT_MS2B(0, 0), FAT_HMS2B(0, 0, 0), FAT_YMD2B(0, 0, 0), FAT_YMD2B(0, 0, 0), FAT_U16(0), FAT_HMS2B(13, 42, 30),  //last_modified_hms
    FAT_YMD2B(2018, 11, 5),                                                                                               //last_modified_ymd
    FAT_U16(0), FAT_U32(0),

    // second entry is readme file
    'R', 'E', 'A', 'D', 'M', 'E', ' ', ' ',  //file_name[8]; padded with spaces (0x20)
    'T', 'X', 'T',                           //file_extension[3]; padded with spaces (0x20)
    0x20,                                    //file attributes: FILE_ATTR_ARCHIVE
    0x00,                                    //ignore
    FAT_MS2B(1, 980),                        //creation_time_10_ms (max 199x10 = 1s 990ms)
    FAT_HMS2B(13, 42, 36),                   //create_time_hms [5:6:5] => h:m:(s/2)
    FAT_YMD2B(2018, 11, 5),                  //create_time_ymd [7:4:5] => (y+1980):m:d
    FAT_YMD2B(2020, 11, 5),                  //last_access_ymd
    FAT_U16(0),                              //extended_attributes
    FAT_HMS2B(13, 44, 16),                   //last_modified_hms
    FAT_YMD2B(2019, 11, 5),                  //last_modified_ymd
    FAT_U16(2),                              //start of file in cluster
    FAT_U32(sizeof(README_CONTENTS) - 1)     //file size
  },

  //------------- Block3: Readme Content -------------//
  README_CONTENTS
};

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  Serial.printf("MSC WRITE: lba: %lu, offset: %lu, bufsize: %lu\n", lba, offset, bufsize);
  memcpy(msc_disk[lba] + offset, buffer, bufsize);
  return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  Serial.printf("MSC READ: lba: %lu, offset: %lu, bufsize: %lu\n", lba, offset, bufsize);
  memcpy(buffer, msc_disk[lba] + offset, bufsize);
  return bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  Serial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\n", power_condition, start, load_eject);
  return true;
}

static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT: Serial.println("USB PLUGGED"); break;
      case ARDUINO_USB_STOPPED_EVENT: Serial.println("USB UNPLUGGED"); break;
      case ARDUINO_USB_SUSPEND_EVENT: Serial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en); break;
      case ARDUINO_USB_RESUME_EVENT:  Serial.println("USB RESUMED"); break;

      default: break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  USB.onEvent(usbEventCallback);
  MSC.vendorID("ESP32");       //max 8 chars
  MSC.productID("USB_MSC");    //max 16 chars
  MSC.productRevision("1.0");  //max 4 chars
  MSC.onStartStop(onStartStop);
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);

  MSC.mediaPresent(true);
  MSC.isWritable(true);  // true if writable, false if read-only

  MSC.begin(DISK_SECTOR_COUNT, DISK_SECTOR_SIZE);
  USB.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
}
#endif /* ARDUINO_USB_MODE */
