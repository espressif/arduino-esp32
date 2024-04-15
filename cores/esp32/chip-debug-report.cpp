#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_arduino_version.h"
#include "esp_rom_spiflash.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_app_format.h"
#include "soc/efuse_reg.h"
#include "soc/rtc.h"
#include "soc/spi_reg.h"
#if CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/spi_flash.h"
#endif
#include "esp_bit_defs.h"

#include "Arduino.h"
#include "esp32-hal-periman.h"

#define chip_report_printf log_printf

#define printMemCapsInfo(caps) _printMemCapsInfo(MALLOC_CAP_##caps, #caps)
#define b2kb(b) ((float)b / 1024.0)
#define b2mb(b) ((float)b / (1024.0 * 1024.0))
static void _printMemCapsInfo(uint32_t caps, const char* caps_str) {
  multi_heap_info_t info;
  size_t total = heap_caps_get_total_size(caps);
  heap_caps_get_info(&info, caps);
  chip_report_printf("%s Memory Info:\n", caps_str);
  chip_report_printf("------------------------------------------\n");
  chip_report_printf("  Total Size        : %8u B (%6.1f KB)\n", total, b2kb(total));
  chip_report_printf("  Free Bytes        : %8u B (%6.1f KB)\n", info.total_free_bytes, b2kb(info.total_free_bytes));
  chip_report_printf("  Allocated Bytes   : %8u B (%6.1f KB)\n", info.total_allocated_bytes, b2kb(info.total_allocated_bytes));
  chip_report_printf("  Minimum Free Bytes: %8u B (%6.1f KB)\n", info.minimum_free_bytes, b2kb(info.minimum_free_bytes));
  chip_report_printf("  Largest Free Block: %8u B (%6.1f KB)\n", info.largest_free_block, b2kb(info.largest_free_block));
}

static void printPkgVersion(void) {
  chip_report_printf("  Package           : ");
#if CONFIG_IDF_TARGET_ESP32
  uint32_t pkg_ver = REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG, EFUSE_RD_CHIP_PACKAGE);
  switch (pkg_ver) {
    case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3: chip_report_printf("D0WD-R2-V3"); break;
    case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6: chip_report_printf("D0WD-Q6"); break;
    case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ5: chip_report_printf("D0WD-Q5"); break;
    case EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5: chip_report_printf("D2WD-Q5"); break;
    case EFUSE_RD_CHIP_VER_PKG_ESP32U4WDH: chip_report_printf("U4WD-H"); break;
    case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4: chip_report_printf("PICO-D4"); break;
    case EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302: chip_report_printf("PICO-V3-02"); break;
  }
#elif CONFIG_IDF_TARGET_ESP32S2
  uint32_t pkg_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_PKG_VERSION);
  switch (pkg_ver) {
    case 1: chip_report_printf("FH16"); break;
    case 2: chip_report_printf("FH32"); break;
    default: chip_report_printf("%lu", pkg_ver); break;
  }
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
  uint32_t pkg_ver = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_3_REG, EFUSE_PKG_VERSION);
  chip_report_printf("%lu", pkg_ver);
#elif CONFIG_IDF_TARGET_ESP32C2
  uint32_t pkg_ver = REG_GET_FIELD(EFUSE_RD_BLK2_DATA1_REG, EFUSE_PKG_VERSION);
  chip_report_printf("%lu", pkg_ver);
#elif CONFIG_IDF_TARGET_ESP32H2
  uint32_t pkg_ver = REG_GET_FIELD(EFUSE_RD_MAC_SYS_4_REG, EFUSE_PKG_VERSION);
  chip_report_printf("%lu", pkg_ver);
#else
  chip_report_printf("Unknown");
#endif
  chip_report_printf("\n");
}

static void printChipInfo(void) {
  esp_chip_info_t info;
  esp_chip_info(&info);
  chip_report_printf("Chip Info:\n");
  chip_report_printf("------------------------------------------\n");
  chip_report_printf("  Model             : ");
  switch (info.model) {
    case CHIP_ESP32: chip_report_printf("ESP32\n"); break;
    case CHIP_ESP32S2: chip_report_printf("ESP32-S2\n"); break;
    case CHIP_ESP32S3: chip_report_printf("ESP32-S3\n"); break;
    case CHIP_ESP32C2: chip_report_printf("ESP32-C2\n"); break;
    case CHIP_ESP32C3: chip_report_printf("ESP32-C3\n"); break;
    case CHIP_ESP32C6: chip_report_printf("ESP32-C6\n"); break;
    case CHIP_ESP32H2: chip_report_printf("ESP32-H2\n"); break;
    default: chip_report_printf("Unknown %d\n", info.model); break;
  }
  printPkgVersion();
  chip_report_printf("  Revision          : ");
  if (info.revision > 0xFF) {
    chip_report_printf("%d.%d\n", info.revision >> 8, info.revision & 0xFF);
  } else {
    chip_report_printf("%d\n", info.revision);
  }
  chip_report_printf("  Cores             : %d\n", info.cores);
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);
  chip_report_printf("  Frequency         : %lu MHz\n", conf.freq_mhz);
  chip_report_printf("  Embedded Flash    : %s\n", (info.features & CHIP_FEATURE_EMB_FLASH) ? "Yes" : "No");
  chip_report_printf("  Embedded PSRAM    : %s\n", (info.features & CHIP_FEATURE_EMB_PSRAM) ? "Yes" : "No");
  chip_report_printf("  2.4GHz WiFi       : %s\n", (info.features & CHIP_FEATURE_WIFI_BGN) ? "Yes" : "No");
  chip_report_printf("  Classic BT        : %s\n", (info.features & CHIP_FEATURE_BT) ? "Yes" : "No");
  chip_report_printf("  BT Low Energy     : %s\n", (info.features & CHIP_FEATURE_BLE) ? "Yes" : "No");
  chip_report_printf("  IEEE 802.15.4     : %s\n", (info.features & CHIP_FEATURE_IEEE802154) ? "Yes" : "No");
}

static void printFlashInfo(void) {
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define ESP_FLASH_IMAGE_BASE 0x1000
#else
#define ESP_FLASH_IMAGE_BASE 0x0000
#endif
// REG_SPI_BASE is not defined for S3/C3 ??
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
#ifndef REG_SPI_BASE
#define REG_SPI_BASE(i) (DR_REG_SPI1_BASE + (((i) > 1) ? (((i)*0x1000) + 0x20000) : (((~(i)) & 1) * 0x1000)))
#endif  // REG_SPI_BASE
#endif  // TARGET

  chip_report_printf("Flash Info:\n");
  chip_report_printf("------------------------------------------\n");
  uint32_t hw_size = 1 << (g_rom_flashchip.device_id & 0xFF);
  chip_report_printf("  Chip Size         : %8lu B (%.0f MB)\n", hw_size, b2mb(hw_size));
  chip_report_printf("  Block Size        : %8lu B (%6.1f KB)\n", g_rom_flashchip.block_size, b2kb(g_rom_flashchip.block_size));
  chip_report_printf("  Sector Size       : %8lu B (%6.1f KB)\n", g_rom_flashchip.sector_size, b2kb(g_rom_flashchip.sector_size));
  chip_report_printf("  Page Size         : %8lu B (%6.1f KB)\n", g_rom_flashchip.page_size, b2kb(g_rom_flashchip.page_size));
  esp_image_header_t fhdr;
  esp_flash_read(esp_flash_default_chip, (void*)&fhdr, ESP_FLASH_IMAGE_BASE, sizeof(esp_image_header_t));
  if (fhdr.magic == ESP_IMAGE_HEADER_MAGIC) {
    uint32_t f_freq = 0;
    switch (fhdr.spi_speed) {
#if CONFIG_IDF_TARGET_ESP32H2
      case 0x0: f_freq = 32; break;
      case 0x2: f_freq = 16; break;
      case 0xf: f_freq = 64; break;
#else
      case 0x0: f_freq = 40; break;
      case 0x1: f_freq = 26; break;
      case 0x2: f_freq = 20; break;
      case 0xf: f_freq = 80; break;
#endif
      default: f_freq = fhdr.spi_speed; break;
    }
    chip_report_printf("  Bus Speed         : %lu MHz\n", f_freq);
  }
  chip_report_printf("  Bus Mode          : ");
#if CONFIG_ESPTOOLPY_OCT_FLASH
  chip_report_printf("OPI\n");
#elif CONFIG_ESPTOOLPY_FLASHMODE_QIO
  chip_report_printf("QIO\n");
#elif CONFIG_ESPTOOLPY_FLASHMODE_QOUT
  chip_report_printf("QOUT\n");
#elif CONFIG_ESPTOOLPY_FLASHMODE_DIO
  chip_report_printf("DIO\n");
#elif CONFIG_ESPTOOLPY_FLASHMODE_DOUT
  chip_report_printf("DOUT\n");
#endif
}

static void printPartitionsInfo(void) {
  chip_report_printf("Partitions Info:\n");
  chip_report_printf("------------------------------------------\n");
  esp_partition_iterator_t iterator = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (iterator != NULL) {
    esp_partition_iterator_t it = iterator;
    while (it != NULL) {
      const esp_partition_t* partition = esp_partition_get(it);
      if (partition) {
        chip_report_printf("  %17s : addr: 0x%08X, size: %7.1f KB", partition->label, partition->address, b2kb(partition->size));
        if (partition->type == ESP_PARTITION_TYPE_APP) {
          chip_report_printf(", type:  APP");
          if (partition->subtype == 0) {
            chip_report_printf(", subtype: FACTORY");
          } else if (partition->subtype >= 0x10 && partition->subtype < 0x20) {
            chip_report_printf(", subtype: OTA_%lu", partition->subtype - 0x10);
          } else if (partition->subtype == 0x20) {
            chip_report_printf(", subtype: TEST");
          } else {
            chip_report_printf(", subtype: 0x%02X", partition->subtype);
          }
        } else {
          chip_report_printf(", type: DATA");
          chip_report_printf(", subtype: ");
          switch (partition->subtype) {
            case ESP_PARTITION_SUBTYPE_DATA_OTA: chip_report_printf("OTA"); break;
            case ESP_PARTITION_SUBTYPE_DATA_PHY: chip_report_printf("PHY"); break;
            case ESP_PARTITION_SUBTYPE_DATA_NVS: chip_report_printf("NVS"); break;
            case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: chip_report_printf("COREDUMP"); break;
            case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS: chip_report_printf("NVS_KEYS"); break;
            case ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM: chip_report_printf("EFUSE_EM"); break;
            case ESP_PARTITION_SUBTYPE_DATA_UNDEFINED: chip_report_printf("UNDEFINED"); break;
            case ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD: chip_report_printf("ESPHTTPD"); break;
            case ESP_PARTITION_SUBTYPE_DATA_FAT: chip_report_printf("FAT"); break;
            case ESP_PARTITION_SUBTYPE_DATA_SPIFFS: chip_report_printf("SPIFFS"); break;
            case ESP_PARTITION_SUBTYPE_DATA_LITTLEFS: chip_report_printf("LITTLEFS"); break;
            default: chip_report_printf("0x%02X", partition->subtype); break;
          }
        }
        chip_report_printf("\n");
      }
      it = esp_partition_next(it);
    }
    //esp_partition_iterator_release(iterator);
  }
}

static void printSoftwareInfo(void) {
  chip_report_printf("Software Info:\n");
  chip_report_printf("------------------------------------------\n");
  chip_report_printf("  Compile Date/Time : %s %s\n", __DATE__, __TIME__);
#ifdef ARDUINO_HOST_OS
  chip_report_printf("  Compile Host OS   : %s\n", ARDUINO_HOST_OS);
#endif
  chip_report_printf("  ESP-IDF Version   : %s\n", esp_get_idf_version());
  chip_report_printf("  Arduino Version   : %s\n", ESP_ARDUINO_VERSION_STR);
}

static void printBoardInfo(void) {
  chip_report_printf("Board Info:\n");
  chip_report_printf("------------------------------------------\n");
  chip_report_printf("  Arduino Board     : %s\n", ARDUINO_BOARD);
  chip_report_printf("  Arduino Variant   : %s\n", ARDUINO_VARIANT);
#ifdef ARDUINO_FQBN
  chip_report_printf("  Arduino FQBN      : %s\n", ARDUINO_FQBN);
#else
#ifdef CORE_DEBUG_LEVEL
  chip_report_printf("  Core Debug Level  : %d\n", CORE_DEBUG_LEVEL);
#endif
#ifdef ARDUINO_RUNNING_CORE
  chip_report_printf("  Arduino Runs Core : %d\n", ARDUINO_RUNNING_CORE);
  chip_report_printf("  Arduino Events on : %d\n", ARDUINO_EVENT_RUNNING_CORE);
#endif
#ifdef ARDUINO_USB_MODE
  chip_report_printf("  Arduino USB Mode  : %d\n", ARDUINO_USB_MODE);
#endif
#ifdef ARDUINO_USB_CDC_ON_BOOT
  chip_report_printf("  CDC On Boot       : %d\n", ARDUINO_USB_CDC_ON_BOOT);
#endif
#endif /* ARDUINO_FQBN */
}

static void printPerimanInfo(void) {
  chip_report_printf("GPIO Info:\n");
  chip_report_printf("------------------------------------------\n");
#if defined(BOARD_HAS_PIN_REMAP)
  chip_report_printf("  DPIN|GPIO : BUS_TYPE[bus/unit][chan]\n");
#else
  chip_report_printf("  GPIO : BUS_TYPE[bus/unit][chan]\n");
#endif
  chip_report_printf("  --------------------------------------  \n");
  for (uint8_t i = 0; i < SOC_GPIO_PIN_COUNT; i++) {
    if (!perimanPinIsValid(i)) {
      continue;  //invalid pin
    }
    peripheral_bus_type_t type = perimanGetPinBusType(i);
    if (type == ESP32_BUS_TYPE_INIT) {
      continue;  //unused pin
    }
#if defined(BOARD_HAS_PIN_REMAP)
    int dpin = gpioNumberToDigitalPin(i);
    if (dpin < 0) {
      continue;  //pin is not exported
    } else {
      chip_report_printf("  D%-3d|%4u : ", dpin, i);
    }
#else
    chip_report_printf("  %4u : ", i);
#endif
    const char* extra_type = perimanGetPinBusExtraType(i);
    if (extra_type) {
      chip_report_printf("%s", extra_type);
    } else {
      chip_report_printf("%s", perimanGetTypeName(type));
    }
    int8_t bus_number = perimanGetPinBusNum(i);
    if (bus_number != -1) {
      chip_report_printf("[%u]", bus_number);
    }
    int8_t bus_channel = perimanGetPinBusChannel(i);
    if (bus_channel != -1) {
      chip_report_printf("[%u]", bus_channel);
    }
    chip_report_printf("\n");
  }
}

void printBeforeSetupInfo(void) {
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.begin(0);
  Serial.setDebugOutput(true);
  uint8_t t = 0;
  while (!Serial && (t++ < 200)) delay(10);  //wait up to 2 seconds for the IDE to connect
#endif
  chip_report_printf("=========== Before Setup Start ===========\n");
  printChipInfo();
  chip_report_printf("------------------------------------------\n");
  printMemCapsInfo(INTERNAL);
  chip_report_printf("------------------------------------------\n");
  if (psramFound()) {
    printMemCapsInfo(SPIRAM);
    chip_report_printf("  Bus Mode          : ");
#if CONFIG_SPIRAM_MODE_OCT
    chip_report_printf("OPI\n");
#else
    chip_report_printf("QSPI\n");
#endif
    chip_report_printf("------------------------------------------\n");
  }
  printFlashInfo();
  chip_report_printf("------------------------------------------\n");
  printPartitionsInfo();
  chip_report_printf("------------------------------------------\n");
  printSoftwareInfo();
  chip_report_printf("------------------------------------------\n");
  printBoardInfo();
  chip_report_printf("============ Before Setup End ============\n");
  delay(100);  //allow the print to finish
}

void printAfterSetupInfo(void) {
  chip_report_printf("=========== After Setup Start ============\n");
  printMemCapsInfo(INTERNAL);
  chip_report_printf("------------------------------------------\n");
  if (psramFound()) {
    printMemCapsInfo(SPIRAM);
    chip_report_printf("------------------------------------------\n");
  }
  printPerimanInfo();
  chip_report_printf("============ After Setup End =============\n");
  delay(20);  //allow the print to finish
}
