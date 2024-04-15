
#include "ESP_I2S.h"
#include "ESP_SR.h"

#define I2S_PIN_BCK 17
#define I2S_PIN_WS 47
#define I2S_PIN_DIN 16

#define LIGHT_PIN 40
#define FAN_PIN 41

I2SClass i2s;

// Generated using the following command:
// python3 tools/gen_sr_commands.py "Turn on the light,Switch on the light;Turn off the light,Switch off the light,Go dark;Start fan;Stop fan"
enum {
  SR_CMD_TURN_ON_THE_LIGHT,
  SR_CMD_TURN_OFF_THE_LIGHT,
  SR_CMD_START_FAN,
  SR_CMD_STOP_FAN,
};
static const sr_cmd_t sr_commands[] = {
  { 0, "Turn on the light", "TkN nN jc LiT" },
  { 0, "Switch on the light", "SWgp nN jc LiT" },
  { 1, "Turn off the light", "TkN eF jc LiT" },
  { 1, "Switch off the light", "SWgp eF jc LiT" },
  { 1, "Go dark", "Gb DnRK" },
  { 2, "Start fan", "STnRT FaN" },
  { 3, "Stop fan", "STnP FaN" },
};

void onSrEvent(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_WAKEWORD:
      Serial.println("WakeWord Detected!");
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
      Serial.printf("Command %d Detected! %s\n", command_id, sr_commands[phrase_id].str);
      switch (command_id) {
        case SR_CMD_TURN_ON_THE_LIGHT:
          digitalWrite(LIGHT_PIN, HIGH);
          break;
        case SR_CMD_TURN_OFF_THE_LIGHT:
          digitalWrite(LIGHT_PIN, LOW);
          break;
        case SR_CMD_START_FAN:
          digitalWrite(FAN_PIN, HIGH);
          break;
        case SR_CMD_STOP_FAN:
          digitalWrite(FAN_PIN, LOW);
          break;
        default:
          Serial.println("Unknown Command!");
          break;
      }
      ESP_SR.setMode(SR_MODE_COMMAND);  // Allow for more commands to be given, before timeout
      // ESP_SR.setMode(SR_MODE_WAKEWORD); // Switch back to WakeWord detection
      break;
    default:
      Serial.println("Unknown Event!");
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, -1, I2S_PIN_DIN);
  i2s.setTimeout(1000);
  i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);


  ESP_SR.onEvent(onSrEvent);
  ESP_SR.begin(i2s, sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t), SR_CHANNELS_STEREO, SR_MODE_WAKEWORD);
}

void loop() {
}
