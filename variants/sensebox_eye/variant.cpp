#include "esp32-hal-gpio.h"
#include "pins_arduino.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_ota_ops.h"

extern "C" {

void blinkLED(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(PIN_LED, r, g, b);
  delay(20);
  rgbLedWrite(PIN_LED, 0x00, 0x00, 0x00);  // off
}

void initVariant(void) {
  // define button pin
  pinMode(47, INPUT_PULLUP);

  // Check if button is pressed
  if (digitalRead(47) == LOW) {
    // When the button is pressed and then released, boot into the OTA1 partition
    const esp_partition_t *ota1_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);

    if (ota1_partition) {
      esp_err_t err = esp_ota_set_boot_partition(ota1_partition);
      if (err == ESP_OK) {
        blinkLED(0x00, 0x00, 0x10);  // blue
        esp_restart();               // restart, to boot OTA1 partition
      } else {
        blinkLED(0x10, 0x00, 0x00);  // red
        ESP_LOGE("OTA", "Error setting OTA1 partition: %s", esp_err_to_name(err));
      }
    }
  } else {
    blinkLED(0x00, 0x10, 0x00);  // green
  }
}
}
