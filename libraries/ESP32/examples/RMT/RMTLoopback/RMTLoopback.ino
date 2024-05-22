// Copyright 2023 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief This example demonstrates usage of RMT for testing a circuit loopback
 * using 2 GPIOs, one for sending RMT data and the other for receiving the data.
 * Those 2 GPIO must be connected to each other.
 *
 * The output is the RMT data comparing what was sent and received
 *
 */

#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
// ESP32 C3 has only 2 channels for RX and 2 for TX, thus MAX RMT_MEM is 128
#define RMT_TX_PIN 4
#define RMT_RX_PIN 5
#define RMT_MEM_RX RMT_MEM_NUM_BLOCKS_2
#else
#define RMT_TX_PIN 18
#define RMT_RX_PIN 21
#define RMT_MEM_RX RMT_MEM_NUM_BLOCKS_3
#endif

rmt_data_t my_data[256];
rmt_data_t data[256];

static EventGroupHandle_t events;

#define RMT_FREQ               10000000  // tick time is 100ns
#define RMT_NUM_EXCHANGED_DATA 30

void setup() {
  Serial.begin(115200);
  events = xEventGroupCreate();

  if (!rmtInit(RMT_TX_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, RMT_FREQ)) {
    Serial.println("init sender failed\n");
  }
  if (!rmtInit(RMT_RX_PIN, RMT_RX_MODE, RMT_MEM_RX, RMT_FREQ)) {
    Serial.println("init receiver failed\n");
  }

  // End of transmission shall be detected when line is idle for 2us = 20*100ns
  rmtSetRxMaxThreshold(RMT_RX_PIN, 20);
  // Disable Glitch  filter
  rmtSetRxMinThreshold(RMT_RX_PIN, 0);

  Serial.println("real tick set to: 100ns");
  Serial.printf("\nPlease connect GPIO %d to GPIO %d, now.\n", RMT_TX_PIN, RMT_RX_PIN);
}

void loop() {
  // Init data
  int i;
  for (i = 0; i < 255; i++) {
    data[i].val = 0x80010001 + ((i % 13) << 16) + 13 - (i % 13);
    my_data[i].val = 0;
  }
  data[255].val = 0;

  // Start an async data read
  size_t rx_num_symbols = RMT_NUM_EXCHANGED_DATA;
  rmtReadAsync(RMT_RX_PIN, my_data, &rx_num_symbols);

  // Write blocking the data to the loopback
  rmtWrite(RMT_TX_PIN, data, RMT_NUM_EXCHANGED_DATA, RMT_WAIT_FOR_EVER);

  // Wait until data is read
  while (!rmtReceiveCompleted(RMT_RX_PIN));

  // Once data is available, the number of RMT Symbols is stored in rx_num_symbols
  // and the received data is copied to my_data
  Serial.printf("Got %d RMT symbols\n", rx_num_symbols);

  // Printout the received data plus the original values
  for (i = 0; i < 60; i++) {
    Serial.printf("%08lx=%08lx ", my_data[i].val, data[i].val);
    if (!((i + 1) % 4)) {
      Serial.println("");
    }
  }
  Serial.println("\n");

  delay(500);
}
