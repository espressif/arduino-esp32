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
 * @brief This example demonstrates usage of RGB LED driven by RMT to verify
 * that RMT works on any CPU/APB Frequency.
 *
 * It uses an ESP32 Arduino builtin RGB NeoLED function based on RMT:
 * void neopixelWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val)
 *
 * The output is a visual WS2812 RGB LED color change routine using each time a
 * different CPU Frequency, just to illustrate how it works. Serial output indicates
 * information about the CPU Frequency while controlling the RGB LED using RMT.
 */


// Default DevKit RGB LED GPIOs:
// The effect seen in (Espressif devkits) ESP32C6, ESP32H2, ESP32C3, ESP32S2 and ESP32S3 is like a Blink of RGB LED
#ifdef PIN_NEOPIXEL
#define MY_LED_GPIO PIN_NEOPIXEL
#else
#define MY_LED_GPIO 21  // ESP32 has no builtin RGB LED (PIN_NEOPIXEL)
#endif

// Set the correct GPIO to any necessary by changing RGB_LED_GPIO value
#define RGB_LED_GPIO MY_LED_GPIO  // Any GPIO valid in the board

// Change the RGB Brightness to any value from 0 (off) to 255 (max)
#define BRIGHTNESS 20  // Change color brightness (max 255)

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\nUsing GPIO %d attached to the RGB LED.\nInitial CPU setup:\n", RGB_LED_GPIO);

  Serial.printf("CPU Freq = %lu MHz\n", getCpuFrequencyMhz());
  Serial.printf("XTAL Freq = %lu MHz\n", getXtalFrequencyMhz());
  Serial.printf("APB Freq = %lu Hz\n", getApbFrequency());
}

void loop() {
  const uint8_t cpufreqs[] = { 240, 160, 80, 40, 20, 10 };
  static uint8_t i = 0;

  setCpuFrequencyMhz(cpufreqs[i]);
  // moves to the next CPU freq for the next loop
  i = (i + 1) % sizeof(cpufreqs);

  // Changing the CPU Freq demands RMT to reset internals parameters setting it correctly
  // This is fixed by reinitializing the RMT peripheral as done below
  // 100ns RMT Tick for driving the NeoLED as in the code of esp32-hal-rgb-led.c (github)
  rmtInit(RGB_LED_GPIO, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);

  // resets also UART to adapt to the new CPU Freq
  Serial.updateBaudRate(115200);
  Serial.printf("\n--changed CPU Frequency to %lu MHz\n", getCpuFrequencyMhz());

  neopixelWrite(RGB_LED_GPIO, BRIGHTNESS, BRIGHTNESS, BRIGHTNESS);  // White
  Serial.println("White");
  delay(1000);
  neopixelWrite(RGB_LED_GPIO, 0, 0, 0);  // Off
  Serial.println("Off");
  delay(1000);
  neopixelWrite(RGB_LED_GPIO, BRIGHTNESS, 0, 0);  // Red
  Serial.println("Red");
  delay(1000);
  neopixelWrite(RGB_LED_GPIO, 0, BRIGHTNESS, 0);  // Green
  Serial.println("Green");
  delay(1000);
  neopixelWrite(RGB_LED_GPIO, 0, 0, BRIGHTNESS);  // Blue
  Serial.println("Blue");
  delay(1000);
  neopixelWrite(RGB_LED_GPIO, 0, 0, 0);  // Off
  Serial.println("Off");
  delay(1000);
}
