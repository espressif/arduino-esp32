/*
  ESP32-S2-EYE I2S record to WAV example
  This simple example demonstrates using the I2S library to record
  5 seconds of audio data and write it to a WAV file on the SD card.

  Don't forget to select the OPI PSRAM, 8MB flash size and Enable USB CDC
  on boot in the Tools menu!

  Created for arduino-esp32 on 18 Dec, 2023
  by Lucas Saavedra Vaz (lucasssvaz)
*/

#include "ESP_I2S.h"
#include "FS.h"
#include "SD_MMC.h"

const uint8_t I2S_SCK = 41;
const uint8_t I2S_WS = 42;
const uint8_t I2S_DIN = 2;

const uint8_t SD_CMD = 38;
const uint8_t SD_CLK = 39;
const uint8_t SD_DATA0 = 40;

void setup() {
  // Create an instance of the I2SClass
  I2SClass i2s;

  // Create variables to store the audio data
  uint8_t *wav_buffer;
  size_t wav_size;

  // Initialize the serial port
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("Initializing I2S bus...");

  // Set up the pins used for audio input
  i2s.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);

  // Initialize the I2S bus in standard mode
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S bus!");
    return;
  }

  Serial.println("I2S bus initialized.");
  Serial.println("Initializing SD card...");

  // Set up the pins used for SD card access
  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_DATA0)) {
    Serial.println("Failed to set SD pins!");
    return;
  }

  // Mount the SD card
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Failed to initialize SD card!");
    return;
  }

  Serial.println("SD card initialized.");
  Serial.println("Recording 5 seconds of audio data...");

  // Record 5 seconds of audio data
  wav_buffer = i2s.recordWAV(5, &wav_size);

  // Create a file on the SD card
  File file = SD_MMC.open("/test.wav", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing!");
    return;
  }

  Serial.println("Writing audio data to file...");

  // Write the audio data to the file
  if (file.write(wav_buffer, wav_size) != wav_size) {
    Serial.println("Failed to write audio data to file!");
    return;
  }

  // Close the file
  file.close();

  Serial.println("Application complete.");
}

void loop() {}
