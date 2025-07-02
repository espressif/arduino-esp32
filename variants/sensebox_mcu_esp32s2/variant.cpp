#include "esp32-hal-gpio.h"
#include "pins_arduino.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include <esp_chip_info.h>

extern "C" {

// Initialize variant/board, called before setup()
void initVariant(void) {
  //enable IO Pins by default
  pinMode(IO_ENABLE, OUTPUT);
  digitalWrite(IO_ENABLE, LOW);

  //reset RGB
  pinMode(PIN_RGB_LED, OUTPUT);
  digitalWrite(PIN_RGB_LED, LOW);

  //enable XBEE by default
  pinMode(PIN_XB1_ENABLE, OUTPUT);
  digitalWrite(PIN_XB1_ENABLE, LOW);

  //enable UART only for chip without PSRAM
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  if (chip_info.revision <= 0) {
    pinMode(PIN_UART_ENABLE, OUTPUT);
    digitalWrite(PIN_UART_ENABLE, LOW);
  }

  //enable PD-Sensor by default
  pinMode(PD_ENABLE, OUTPUT);
  digitalWrite(PD_ENABLE, HIGH);

  // define button pin
  const int PIN_BUTTON = 0;
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // keep button pressed
  unsigned long pressStartTime = 0;
  bool buttonPressed = false;

  // Wait 5 seconds for the button to be pressed
  unsigned long startTime = millis();

  // Check if button is pressed
  while (millis() - startTime < 5000) {
    if (digitalRead(PIN_BUTTON) == LOW) {
      if (!buttonPressed) {
        // The button was pressed
        buttonPressed = true;
      }
    } else if (buttonPressed) {
      // When the button is pressed and then released, boot into the OTA1 partition
      const esp_partition_t *ota1_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);

      if (ota1_partition) {
        esp_err_t err = esp_ota_set_boot_partition(ota1_partition);
        if (err == ESP_OK) {
          esp_restart();  // restart, to boot OTA1 partition
        } else {
          ESP_LOGE("OTA", "Error setting OTA1 partition: %s", esp_err_to_name(err));
        }
      }
      // Abort after releasing the button
      break;
    }
  }
}
}
