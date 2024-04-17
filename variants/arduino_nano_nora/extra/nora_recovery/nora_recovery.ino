#include "USB.h"

#define USB_TIMEOUT_MS 15000
#define POLL_DELAY_MS 60
#define FADESTEP 8

void pulse_led() {
  static uint32_t pulse_width = 0;
  static uint8_t dir = 0;

  if (dir) {
    pulse_width -= FADESTEP;
    if (pulse_width < FADESTEP) {
      dir = 0U;
      pulse_width = FADESTEP;
    }
  } else {
    pulse_width += FADESTEP;
    if (pulse_width > 255) {
      dir = 1U;
      pulse_width = 255;
    }
  }

  analogWrite(LED_GREEN, pulse_width);
}

#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_flash_partitions.h>
#include <esp_image_format.h>
const esp_partition_t *find_previous_firmware() {
  extern bool _recovery_active;
  if (!_recovery_active) {
    // user flashed this recovery sketch to an OTA partition
    // stay here and wait for a proper firmware
    return NULL;
  }

  // booting from factory partition, look for a valid OTA image
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  for (; it != NULL; it = esp_partition_next(it)) {
    const esp_partition_t *part = esp_partition_get(it);
    if (part->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY) {
      esp_partition_pos_t candidate = { part->address, part->size };
      esp_image_metadata_t meta;
      if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &candidate, &meta) == ESP_OK) {
        // found, use it
        return part;
      }
    }
  }

  return NULL;
}

const esp_partition_t *user_part = NULL;

void setup() {
  user_part = find_previous_firmware();
  if (user_part)
    esp_ota_set_boot_partition(user_part);

  extern bool _recovery_marker_found;
  if (!_recovery_marker_found && user_part) {
    // recovery marker not found, probable cold start
    // try starting previous firmware immediately
    esp_restart();
  }

  // recovery marker found, or nothing else to load
  printf("Recovery firmware started, waiting for USB\r\n");
}

void loop() {
  static int elapsed_ms = 0;

  pulse_led();
  delay(POLL_DELAY_MS);
  if (USB) {
    // wait indefinitely for DFU to complete
    elapsed_ms = 0;
  } else {
    // wait for USB connection
    elapsed_ms += POLL_DELAY_MS;
  }

  if (elapsed_ms > USB_TIMEOUT_MS) {
    elapsed_ms = 0;
    // timed out, try loading previous firmware
    if (user_part) {
      // there was a valid FW image, load it
      analogWrite(LED_GREEN, 255);
      printf("Leaving recovery firmware\r\n");
      delay(200);
      esp_restart();  // does not return
    }
  }
}
