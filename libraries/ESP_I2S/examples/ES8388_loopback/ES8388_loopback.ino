/*
  ESP32-LyraT I2S ES8388 loopback example
  This simple example demonstrates using the I2S library in combination
  with the ES8388 codec on the ESP32-LyraT board to record and play back
  audio data.

  Don't forget to enable the PSRAM in the Tools menu!

  Created for arduino-esp32 on 20 Dec, 2023
  by Lucas Saavedra Vaz (lucasssvaz)
*/

#include "ESP_I2S.h"
#include "Wire.h"

#include "ES8388.h"

/* Pin definitions */

/* I2C */
const uint8_t I2C_SCL = 23;
const uint8_t I2C_SDA = 18;
const uint32_t I2C_FREQ = 400000;

/* I2S */
const uint8_t I2S_MCLK = 0;   /* Master clock */
const uint8_t I2S_SCK = 5;    /* Audio data bit clock */
const uint8_t I2S_WS = 25;    /* Audio data left and right clock */
const uint8_t I2S_SDOUT = 26; /* ESP32 audio data output (to speakers) */
const uint8_t I2S_SDIN = 35;  /* ESP32 audio data input (from microphone) */

/* PA */
const uint8_t PA_ENABLE = 21; /* Power amplifier enable */

void setup() {
  I2SClass i2s;
  ES8388 codec;
  uint8_t *wav_buffer;
  size_t wav_size;

  // Initialize the serial port
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  pinMode(PA_ENABLE, OUTPUT);
  digitalWrite(PA_ENABLE, HIGH);

  Serial.println("Initializing I2C bus...");

  // Initialize the I2C bus
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);

  Serial.println("Initializing I2S bus...");

  // Set up the pins used for audio input
  i2s.setPins(I2S_SCK, I2S_WS, I2S_SDOUT, I2S_SDIN, I2S_MCLK);

  // Initialize the I2S bus in standard mode
  if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("Failed to initialize I2S bus!");
    return;
  }

  Serial.println("Initializing ES8388...");

  if (!codec.begin(i2s)) {
    Serial.println("Failed to initialize ES8388!");
    return;
  }

  Serial.println("Recording 10 seconds of audio data...");

  // Record 10 seconds of audio data
  wav_buffer = codec.recordWAV(10, &wav_size);

  Serial.println("Recording complete. Playing audio data in 3 seconds.");
  delay(3000);

  // Play the audio data
  Serial.println("Playing audio data...");
  codec.playWAV(wav_buffer, wav_size);

  Serial.println("Application complete.");
}

void loop() {}
