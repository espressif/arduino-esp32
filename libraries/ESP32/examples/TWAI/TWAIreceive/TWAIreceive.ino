/* ESP32 TWAI receive example.
  Receive messages and sends them over serial.

  Connect a CAN bus transceiver to the RX/TX pins.
  For example: SN65HVD230

  TWAI_MODE_LISTEN_ONLY is used so that the TWAI controller will not influence the bus.

  The API gives other possible speeds and alerts:
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html

  Example output:
  -> Message received
  -> Message is in Standard Format
  -> ID is 604
  -> Byte 0 = 0 Byte 1 = f Byte 2 = 13 Byte 3 = 2 Byte 4 = 0 Byte 5 = 0 Byte 6 = 8 Byte 7 = 0

  created 05-11-2022 by Stephan Martin (designer2k2)
*/

#include "driver/twai.h"

//Pins used to connect to CAN bus transceiver:
#define rxPin 21
#define txPin 22

void setup() {
  // Start Serial:
  Serial.begin(115200);

  //Initialize configuration structures using macro initializers
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)txPin, (gpio_num_t)rxPin, TWAI_MODE_LISTEN_ONLY);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();  //Look in the api-reference for other speed sets.
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  //Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver installed");
  } else {
    Serial.println("Failed to install driver");
    return;
  }

  //Start TWAI driver
  if (twai_start() == ESP_OK) {
    Serial.println("Driver started");
  } else {
    Serial.println("Failed to start driver");
    return;
  }

  //Reconfigure alerts to detect Bus-Off error and RX queue full states
  uint32_t alerts_to_enable = TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL;
  if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
    Serial.println("CAN Alerts reconfigured");
  } else {
    Serial.println("Failed to reconfigure alerts");
  }

}

void loop() {
  //Check if alert happened
  uint32_t alerts_triggered;
  twai_read_alerts(&alerts_triggered, 1);
  if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
    Serial.println("Alert: TWAI controller has become error passive.");
  } else if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
    Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
  } else if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
    Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
  }

  //Wait for message to be received:
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.println("Message received");
  } else {
    Serial.println("No message received");
    return;
  }

  //Process received message
  if (message.extd) {
    Serial.println("Message is in Extended Format");
  } else {
    Serial.println("Message is in Standard Format");
  }
  Serial.printf("ID is %x\n", message.identifier);
  if (!(message.rtr)) {
    for (int i = 0; i < message.data_length_code; i++) {
      Serial.printf("Byte %d = %x ", i, message.data[i]);
    }
    Serial.println("");
  }

}
