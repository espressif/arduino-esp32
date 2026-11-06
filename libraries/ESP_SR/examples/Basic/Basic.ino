#include <Arduino.h>
#include "ESP_I2S.h"
#include "ESP_SR.h"

// Pins for I2S Microphone
#define I2S_PIN_BCK 17
#define I2S_PIN_DIN 16
#define I2S_PIN_WS  15

// Pins for PDM Microphone
#define PDM_PIN_CLK  12
#define PDM_PIN_DATA 9

#ifndef LED_BUILTIN
#define LED_BUILTIN 3
#endif

#define LIGHT_PIN LED_BUILTIN
#define FAN_PIN   18

// ESP_SR requires 16bit audio with 16kHz sample rate
#define I2S_SAMPLE_RATE 16000
#define I2S_DATA_WIDTH  I2S_DATA_BIT_WIDTH_16BIT

/**
 * SR_INPUT_FORMAT:
 * M to represent the microphone channel
 * R to represent the playback reference channel
 * N to represent an unknown or unused channel
 *
 * For example, SR_INPUT_FORMAT="MN" indicates that the input data consists of two channels,
 * which are the microphone channel and an unused channel
 *
 * SR_INPUT_CHANNELS: Equal to how many channels is the input data.
 * This should be less or equal to the SR_INPUT_FORMAT length. Best to be equal!
 *
 * I2S_OUTPUT_CHANNELS: Equal to how many channels is the I2S data.
 * This should be equal to SR_INPUT_CHANNELS
 */
// Mono microphone on mono bus
#define SR_INPUT_FORMAT     "M"
#define SR_INPUT_CHANNELS   SR_CHANNELS_MONO
#define I2S_OUTPUT_CHANNELS I2S_SLOT_MODE_MONO

// Mono microphone (left) on stereo bus
// #define SR_INPUT_FORMAT "MN"
// #define SR_INPUT_CHANNELS SR_CHANNELS_STEREO
// #define I2S_OUTPUT_CHANNELS I2S_SLOT_MODE_STEREO

// Stereo microphones on stereo bus
// #define SR_INPUT_FORMAT "MM"
// #define SR_INPUT_CHANNELS SR_CHANNELS_STEREO
// #define I2S_OUTPUT_CHANNELS I2S_SLOT_MODE_STEREO

I2SClass i2s;

// Command IDs
enum {
  SR_CMD_LIGHT_ON,
  SR_CMD_LIGHT_OFF,
  SR_CMD_FAN_ON,
  SR_CMD_FAN_OFF,
};
// Command phrases. Can have multiple phrases for given command ID
static const sr_cmd_t sr_commands[] = {
  {SR_CMD_LIGHT_ON, "Turn on the light"},
  {SR_CMD_LIGHT_ON, "Switch on the light"},
  {SR_CMD_LIGHT_ON, "Lights on"},
  {SR_CMD_LIGHT_OFF, "Turn off the light"},
  {SR_CMD_LIGHT_OFF, "Switch off the light"},
  {SR_CMD_LIGHT_OFF, "Lights off"},
  {SR_CMD_LIGHT_OFF, "Go dark"},
  {SR_CMD_FAN_ON, "Start fan"},
  {SR_CMD_FAN_ON, "Fan on"},
  {SR_CMD_FAN_OFF, "Stop fan"},
  {SR_CMD_FAN_OFF, "Fan off"},
};

void onSrEvent(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_WAKEWORD:
      Serial.println("WakeWord Detected!");
      if (strlen(SR_INPUT_FORMAT) == 1) {  // Mono recognition does not get CHANNEL event
        ESP_SR.setMode(SR_MODE_COMMAND);   // Switch to Command detection
      }
      break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      Serial.printf("WakeWord Channel %d Verified!\n", command_id);
      ESP_SR.setMode(SR_MODE_COMMAND);  // Switch to Command detection
      break;
    case SR_EVENT_TIMEOUT:
      Serial.println("Timeout Detected!");
      ESP_SR.setMode(SR_MODE_WAKEWORD);  // Switch back to WakeWord detection
      break;
    case SR_EVENT_COMMAND:
      Serial.printf("Command ID %d Detected!\n", command_id);
      switch (command_id) {
        case SR_CMD_LIGHT_ON:
          Serial.println("Light On");
          digitalWrite(LIGHT_PIN, HIGH);
          break;
        case SR_CMD_LIGHT_OFF:
          Serial.println("Light Off");
          digitalWrite(LIGHT_PIN, LOW);
          break;
        case SR_CMD_FAN_ON:
          Serial.println("Fan On");
          digitalWrite(FAN_PIN, HIGH);
          break;
        case SR_CMD_FAN_OFF:
          Serial.println("Fan Off");
          digitalWrite(FAN_PIN, LOW);
          break;
        default: Serial.printf("Unknown Command ID %d!\n", command_id); break;
      }
      ESP_SR.setMode(SR_MODE_COMMAND);  // Allow for more commands to be given, before timeout
      //ESP_SR.setMode(SR_MODE_WAKEWORD); // Switch back to WakeWord detection
      break;
    default: Serial.println("Unknown Event!"); break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  i2s.setTimeout(1000);

  // I2S Microphone
  // i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, -1, I2S_PIN_DIN);
  // i2s.begin(I2S_MODE_STD, I2S_SAMPLE_RATE, I2S_DATA_WIDTH, I2S_OUTPUT_CHANNELS, I2S_STD_SLOT_LEFT);

  // SPH0645 I2S Microphone (requires data transform to 16bit)
  // i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, -1, I2S_PIN_DIN);
  // i2s.begin(I2S_MODE_STD, I2S_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_OUTPUT_CHANNELS, I2S_STD_SLOT_LEFT);
  // i2s.configureRX(I2S_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_OUTPUT_CHANNELS, I2S_RX_TRANSFORM_32_TO_16);

  // PDM Microphone
  i2s.setPinsPdmRx(PDM_PIN_CLK, PDM_PIN_DATA);
  i2s.begin(I2S_MODE_PDM_RX, I2S_SAMPLE_RATE, I2S_DATA_WIDTH, I2S_OUTPUT_CHANNELS, I2S_STD_SLOT_RIGHT);

  ESP_SR.onEvent(onSrEvent);
  ESP_SR.begin(i2s, sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t), SR_INPUT_CHANNELS, SR_MODE_WAKEWORD, SR_INPUT_FORMAT);
}

void loop() {
  // uint8_t buf[64];
  // i2s.readBytes((char *)buf, 64);
  // log_buf_d((const uint8_t *)buf, 64);
  delay(100);
}
