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
 * @brief This example demonstrate how to use RMT to just blink a regular LED (GPIO)
 * It uses all the different RMT Writing APIs to blink the LED by hardware, not being
 * necessary the regular Blink code in Arduino.
 *
 * The output is the Blinking LED in the GPIO and a serial output describing what is
 * going on, along the execution.
 *
 * The circuit is just a LED and a resistor of 270 ohms connected to the GPIO
 * GPIO ---> resistor 270 ohms ---> + LED - ---> GND
 */

#define BLINK_GPIO 2

// RMT is at 400KHz with a 2.5us tick
// This RMT data sends a 0.5Hz pulse with 1s High and 1s Low signal
rmt_data_t blink_1s_rmt_data[] = {
  // 400,000 x 2.5us = 1 second ON
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  // 400,000 x 2.5us = 1 second OFF
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  // Looping mode needs a Zero ending data to mark the EOF
  {0, 0, 0, 0}
};

// RMT is at 400KHz with a 2.5us tick
// This RMT data sends a 1Hz pulse with 500ms High and 500ms Low signal
rmt_data_t blink_500ms_rmt_data[] = {
  // 200,000 x 2.5us = 0.5 second ON
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  // 200,000 x 2.5us = 0.5 second OFF
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  // Looping mode needs a Zero ending data to mark the EOF
  {0, 0, 0, 0}
};

// RMT is at 400KHz with a 2.5us tick
// This RMT data sends a 2Hz pulse with 250ms High and 250ms Low signal
rmt_data_t blink_250ms_rmt_data[] = {
  // 100,000 x 2.5us = 0.25 second ON
  {
    25000,
    1,
    25000,
    1,
  },
  {
    25000,
    1,
    25000,
    1,
  },
  // 100,000 x 2.5us = 0.25 second OFF
  {
    25000,
    0,
    25000,
    0,
  },
  {
    25000,
    0,
    25000,
    0,
  },
  // Looping mode needs a Zero ending data to mark the EOF
  {0, 0, 0, 0}
};

void RMT_Mixed_Write_Blink() {
  Serial.println("===> rmtWriteLooping() to Blink the LED.");
  Serial.println("Blinking at 1s on + 1s off :: 3 blinks");
  if (!rmtWriteLooping(BLINK_GPIO, blink_1s_rmt_data, RMT_SYMBOLS_OF(blink_1s_rmt_data))) {
    Serial.println("===> rmtWriteLooping Blink 1s Error!");
  }
  delay(6000);  // blinking happens here, done by hardware!

  Serial.println("===> rmtWrite() (Blocking Mode) to Blink the LED.");
  Serial.println("Blinking at 500ms on + 500ms off :: 4 blinks");
  for (uint8_t i = 0; i < 4; i++) {
    if (!rmtWrite(BLINK_GPIO, blink_500ms_rmt_data, RMT_SYMBOLS_OF(blink_500ms_rmt_data) - 2, RMT_WAIT_FOR_EVER)) {
      Serial.println("===> rmtWrite Blink 0.5s Error!");
    }
  }

  Serial.println("===> rmtWriteAsync() (Non-Blocking Mode) to Blink the LED.");
  Serial.println("Blinking at 250ms on + 250ms off :: 5 blinks");
  for (uint8_t i = 0; i < 5; i++) {
    if (!rmtWriteAsync(BLINK_GPIO, blink_250ms_rmt_data, RMT_SYMBOLS_OF(blink_250ms_rmt_data) - 2)) {
      Serial.println("===> rmtWrite Blink 0.25s Error!");
    }
    // wait (blocks) until all the data is sent out
    while (!rmtTransmitCompleted(BLINK_GPIO));
  }
  Serial.println("Blinking OFF for 1 seconds");
  delay(1000);
}

void RMT_Loop_Write_Blink() {
  Serial.println("Using RMT Writing loop to blink an LED.");
  Serial.println("Blinking at 1s on + 1s off :: 3 blinks");
  if (!rmtWriteLooping(BLINK_GPIO, blink_1s_rmt_data, RMT_SYMBOLS_OF(blink_1s_rmt_data))) {
    Serial.println("===> rmtWriteLooping Blink 1s Error!");
  }
  delay(6000);
  Serial.println("Blinking at 500ms on + 500ms off :: 5 blinks");
  if (!rmtWriteLooping(BLINK_GPIO, blink_500ms_rmt_data, RMT_SYMBOLS_OF(blink_500ms_rmt_data))) {
    Serial.println("===> rmtWriteLooping Blink 0.5s Error!");
  }
  delay(5000);
  Serial.println("Blinking at 250ms on + 250ms off :: 10 blinks");
  if (!rmtWriteLooping(BLINK_GPIO, blink_250ms_rmt_data, RMT_SYMBOLS_OF(blink_250ms_rmt_data))) {
    Serial.println("===> rmtWriteLooping Blink 0.25s Error!");
  }
  delay(5000);
  Serial.println("Blinking OFF for 2 seconds");
  if (!rmtWriteLooping(BLINK_GPIO, NULL, 0)) {
    Serial.println("===> rmtWriteLooping Blink OFF Error!");
  }
  delay(2000);
}

void RMT_Single_Write_Blocking_Blink() {
  Serial.println("Using RMT Writing and its Completion to blink an LED.");
  Serial.println("Blinking at 1s on + 1s off :: 2 blinks");
  for (uint8_t i = 0; i < 2; i++) {
    if (!rmtWrite(BLINK_GPIO, blink_1s_rmt_data, RMT_SYMBOLS_OF(blink_1s_rmt_data) - 2, RMT_WAIT_FOR_EVER)) {
      Serial.println("===> rmtWrite Blink 1s Error!");
    }
  }
  Serial.println("Blinking at 500ms on + 500ms off :: 4 blinks");
  for (uint8_t i = 0; i < 4; i++) {
    if (!rmtWrite(BLINK_GPIO, blink_500ms_rmt_data, RMT_SYMBOLS_OF(blink_500ms_rmt_data) - 2, RMT_WAIT_FOR_EVER)) {
      Serial.println("===> rmtWrite Blink 0.5s Error!");
    }
  }
  Serial.println("Blinking at 250ms on + 250ms off :: 8 blinks");
  for (uint8_t i = 0; i < 8; i++) {
    if (!rmtWrite(BLINK_GPIO, blink_250ms_rmt_data, RMT_SYMBOLS_OF(blink_250ms_rmt_data) - 2, RMT_WAIT_FOR_EVER)) {
      Serial.println("===> rmtWrite Blink 0.25s Error!");
    }
  }
  Serial.println("Blinking OFF for 3 seconds");
  delay(3000);
}

void RMT_Write_Aync_Non_Blocking_Blink() {
  Serial.println("Using RMT Async Writing and its Completion to blink an LED.");
  Serial.println("Blinking at 1s on + 1s off :: 5 blinks");
  for (uint8_t i = 0; i < 5; i++) {
    if (!rmtWriteAsync(BLINK_GPIO, blink_1s_rmt_data, RMT_SYMBOLS_OF(blink_1s_rmt_data) - 2)) {
      Serial.println("===> rmtWrite Blink 1s Error!");
    }
    // wait (blocks) until all the data is sent out
    while (!rmtTransmitCompleted(BLINK_GPIO));
  }
  Serial.println("Blinking at 500ms on + 500ms off :: 5 blinks");
  for (uint8_t i = 0; i < 5; i++) {
    if (!rmtWriteAsync(BLINK_GPIO, blink_500ms_rmt_data, RMT_SYMBOLS_OF(blink_500ms_rmt_data) - 2)) {
      Serial.println("===> rmtWrite Blink 0.5s Error!");
    }
    // wait (blocks) until all the data is sent out
    while (!rmtTransmitCompleted(BLINK_GPIO));
  }
  Serial.println("Blinking at 250ms on + 250ms off :: 5 blinks");
  for (uint8_t i = 0; i < 5; i++) {
    if (!rmtWriteAsync(BLINK_GPIO, blink_250ms_rmt_data, RMT_SYMBOLS_OF(blink_250ms_rmt_data) - 2)) {
      Serial.println("===> rmtWrite Blink 0.25s Error!");
    }
    // wait (blocks) until all the data is  sent out
    while (!rmtTransmitCompleted(BLINK_GPIO));
  }
  Serial.println("Blinking OFF for 1 seconds");
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Blink testing...");
  Serial.flush();

  // 1 RMT Block has 64 RMT_SYMBOLS (ESP32|ESP32S2) or 48 RMT_SYMBOLS (ESP32C3|ESP32S3)
  if (!rmtInit(BLINK_GPIO, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 400000)) {  //2.5us tick
    Serial.println("===> rmtInit Error!");
  } else {
    Serial.println("===> rmtInit OK! Tick = 2.5us - OK for testing");
  }
  Serial.println("\n======================================");
  Serial.println("All set. Starting RMT testing Routine.");
  Serial.println("======================================\n");

  RMT_Mixed_Write_Blink();
  Serial.println("End of Mixed Calls testing");
  delay(1000);

  Serial.println("\n===============================");
  Serial.println("Starting a Blinking sequence...");
  Serial.println("===============================\n");
}

void loop() {
  RMT_Write_Aync_Non_Blocking_Blink();
  RMT_Loop_Write_Blink();
  RMT_Single_Write_Blocking_Blink();
  Serial.println("\nStarting OVER...\n");
}
