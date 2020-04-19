
#include "Arduino.h"
#include "esp_spi_flash.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"

#include "UpdateProcessor.h"

/* We have four formats; this handler deals with the original format:

  1) original 'raw' package.

   Starts with 0xE7 / ESP_IMAGE_HEADER_MAGIC.

   Which has no checksum or other protection (beyond the size check and the check of the first byte).

 The EC, S/MIME and RFC3161 versons have thier own processors.

*/


UpdateProcessor::secure_update_processor_err_t UpdateProcessorLegacy::process_header(uint32_t *command, uint8_t* buffer, size_t *len) {
  if ((*command) == U_SPIFFS)
    return UpdateProcessor::secure_update_processor_OK;

  if ((*command) == U_FLASH) {
    if (buffer[0] == ESP_IMAGE_HEADER_MAGIC) {
      log_d("Valid magic at start of flash header");
      return UpdateProcessor::secure_update_processor_OK;
    };
    log_e("Missing ESP_IMAGE_HEADER_MAGIC");
  };
  log_e("Invalid command 0x%04x ", *command);

  return UpdateProcessor::secure_update_processor_ERROR;
};
