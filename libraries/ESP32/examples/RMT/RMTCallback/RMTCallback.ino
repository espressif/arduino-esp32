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
 * @brief This example demonstrate how to use a C++ Class to read several GPIO RMT signals
 * calling a data processor when data is available in background, using tasks.
 *
 * The output is the last RMT data read in the GPIO, just to illustrate how it works.
 *
 */


class MyProcessor {
private:
  uint32_t buff;  // rolling buffer of most recent 32 bits.
  int at = 0;
  size_t rx_num_symbols = RMT_MEM_NUM_BLOCKS_1 * RMT_SYMBOLS_PER_CHANNEL_BLOCK;
  rmt_data_t rx_symbols[RMT_MEM_NUM_BLOCKS_1 * RMT_SYMBOLS_PER_CHANNEL_BLOCK];

public:
  int8_t gpio = -1;

  MyProcessor(uint8_t pin, uint32_t rmtFreqHz) {
    if (!rmtInit(pin, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_1, rmtFreqHz)) {
      Serial.println("init receiver failed\n");
      return;
    }
    gpio = pin;
  }

  void begin() {
    // Creating RMT RX Callback Task
    xTaskCreate(readTask, "MyProcessor", 2048, this, 4, NULL);
  }

  static void readTask(void *args) {
    MyProcessor *me = (MyProcessor *)args;

    while (1) {
      // blocks until RMT has read data
      rmtRead(me->gpio, me->rx_symbols, &me->rx_num_symbols, RMT_WAIT_FOR_EVER);
      // process the data like a callback whenever there is data available
      process(me->rx_symbols, me->rx_num_symbols, me);
    }
    vTaskDelete(NULL);
  }

  static void process(rmt_data_t *data, size_t len, void *args) {
    MyProcessor *me = (MyProcessor *)args;
    uint32_t *buff = &me->buff;

    for (int i = 0; len; len--) {
      if (data[i].duration0 == 0)
        break;
      *buff = (*buff << 1) | (data[i].level0 ? 1 : 0);
      i++;

      if (data[i].duration1 == 0)
        break;
      *buff = (*buff << 1) | (data[i].level1 ? 1 : 0);
      i++;
    };
  }

  uint32_t val() {
    return buff;
  }
};

// Attach 3 processors to GPIO 4, 5 and 10 with different tick/speeds.
MyProcessor mp1 = MyProcessor(4, 1000000);
MyProcessor mp2 = MyProcessor(5, 1000000);
MyProcessor mp3 = MyProcessor(10, 2000000);

void setup() {
  Serial.begin(115200);
  mp1.begin();
  mp2.begin();
  mp3.begin();
}

void loop() {
  // The reading values will come from the 3 tasks started by setup()
  Serial.printf("GPIO %d: %08lx | %d: %08lx | %d: %08lx\n", mp1.gpio, mp1.val(), mp2.gpio, mp2.val(), mp3.gpio, mp3.val());
  delay(500);
}
