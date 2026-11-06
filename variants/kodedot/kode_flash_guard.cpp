/**
 * @file kode_flash_guard.cpp
 * @brief Teach IDF's flash-write guard and OTA lookups where a sketch runs.
 *
 * A sketch runs in place from kodeOS's apps region, a DATA partition, so
 * esp_ota_get_running_partition() (APP partitions only) finds nothing and, with
 * CONFIG_SPI_FLASH_DANGEROUS_WRITE_ABORTS on, the first Preferences/EEPROM/FFat/LittleFS write
 * aborts ("Partition table is invalid or corrupt"); ESP.getSketchSize(), getSketchMD5() and
 * Update ask the same question. The board entry wraps three IDF symbols
 * (compiler.c.elf.extra_flags):
 *  - esp_ota_get_running_partition: a synthetic APP partition covering this sketch's image
 *  - esp_ota_get_next_update_partition: NULL, a sketch has no OTA slot (Update.begin() fails cleanly)
 *  - esp_image_verify: an image reaching 16 MB or above reads its headers only, the prebuilt libraries cannot mmap that range
 */
#include <stddef.h>
#include <stdint.h>

#include "esp_partition.h"
#include "esp_app_desc.h"
#include "esp_flash.h"
#include "esp_image_format.h"
#include "spi_flash_mmap.h"

/** @brief This image's descriptor, at image offset 0x20 (kode_app_desc.cpp). */
extern "C" const esp_app_desc_t esp_app_desc;

/** @brief The real lookup, still linked, used when the image cannot be located. */
extern "C" const esp_partition_t *__real_esp_ota_get_running_partition(void);

/** @brief The prebuilt libraries address flash with 24 bits: no mmap, and no image they read, reaches past 16 MB. */
#define KODE_FLASH_24BIT_END 0x1000000U

/**
 * @brief Build a partition describing this sketch's image in place.
 * @return A pointer to a static partition, or NULL if the image cannot be located.
 */
static const esp_partition_t *kode_sketch_partition(void) {
  static esp_partition_t part;
  static bool ready = false;
  if (ready) {
    return &part;
  }

  size_t const phys = spi_flash_cache2phys((const void *)&esp_app_desc);
  if (phys == SPI_FLASH_CACHE2PHYS_FAIL) {
    return NULL;
  }
  /* The image starts 0x20 before its descriptor; its headers give its length (the size here only bounds them). */
  esp_partition_pos_t const pos = {.offset = (uint32_t)phys - 0x20U, .size = KODE_FLASH_24BIT_END};
  esp_image_metadata_t meta;
  if (esp_image_get_metadata(&pos, &meta) != ESP_OK) {
    return NULL;
  }

  part = esp_partition_t{
    .flash_chip = esp_flash_default_chip,
    .type = ESP_PARTITION_TYPE_APP,
    .subtype = ESP_PARTITION_SUBTYPE_APP_TEST,
    .address = pos.offset,
    .size = (meta.image_len + 0xFFFU) & ~0xFFFU,
    .erase_size = 0x1000U,
    .label = "sketch",
  };
  ready = true;
  return &part;
}

/**
 * @brief Answer the running-partition question with this sketch's image.
 * @return The synthetic sketch partition, or the stock answer if the image cannot be located.
 */
extern "C" const esp_partition_t *__wrap_esp_ota_get_running_partition(void) {
  const esp_partition_t *p = kode_sketch_partition();
  if (p != NULL) {
    return p;
  }
  return __real_esp_ota_get_running_partition();
}

/**
 * @brief A sketch on the Dot has no OTA slot, whichever partition the search starts from.
 * @return Always NULL.
 */
extern "C" const esp_partition_t *__wrap_esp_ota_get_next_update_partition(const esp_partition_t *) {
  return NULL;
}

/** @brief The real verify, still linked, for images the cache can map. */
extern "C" esp_err_t __real_esp_image_verify(esp_image_load_mode_t mode, const esp_partition_pos_t *part, esp_image_metadata_t *data);

/**
 * @brief Let ESP.getSketchSize()/getSketchMD5() work for an image that reaches 16 MB.
 *
 * Esp.cpp sizes the sketch with esp_image_verify(), which mmaps every segment, and the prebuilt
 * libraries refuse an mmap at or above 16 MB. When any part of the image is there, read the headers
 * instead, whether the image starts above 16 MB or only ends there: image_len is
 * all a size query needs, and the bootloader already verified this image at adoption and at boot.
 *
 * @param mode Passed through to the real verify for an image wholly below 16 MB.
 * @param part The image position; where it ends selects the path.
 * @param data Receives the metadata, image_len among it.
 * @return ESP_OK when the headers read, else the failure from the chosen path.
 */
extern "C" esp_err_t __wrap_esp_image_verify(esp_image_load_mode_t mode, const esp_partition_pos_t *part, esp_image_metadata_t *data) {
  if ((part != NULL) && ((part->offset + part->size) > KODE_FLASH_24BIT_END)) {
    return esp_image_get_metadata(part, data);
  }
  return __real_esp_image_verify(mode, part, data);
}
