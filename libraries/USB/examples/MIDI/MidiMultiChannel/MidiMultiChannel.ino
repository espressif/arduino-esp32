/*
This example demonstrates advanced USB MIDI features using an ESP32 with native USB support (S2, S3,
etc.), including Program Change, Pitch Bend, Channel Pressure (Aftertouch), and multi-channel output.

Every time the button on the ESP32 board (attached to pin 0) is pressed, the sketch cycles through
a demo sequence that exercises the USBMIDI API methods not covered by the other examples:

  1. Program Change  - Selects different instrument sounds (General MIDI)
  2. Pitch Bend      - Sweeps pitch up and down using the three available overloads
  3. Channel Pressure - Sends aftertouch for expressive control
  4. Multi-channel    - Sends notes on channels 1 through 4 simultaneously

No external hardware is required beyond the ESP32 board itself.

To test, connect the ESP32 to a DAW (Ableton, FL Studio, REAPER, etc.) or a software synthesizer
that listens on multiple MIDI channels. The Serial Monitor will log each action as it happens.

Arduino IDE settings:
  - Board: ESP32-S2 or ESP32-S3 based board
  - USB Mode: "USB-OTG (TinyUSB)"

Note: This sketch requires USB OTG mode. It will not work when USB Mode is set to
"Hardware CDC and JTAG".
*/
#include <Arduino.h>
#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include "USB.h"
#include "USBMIDI.h"
USBMIDI MIDI;

#define BUTTON_PIN 0

// General MIDI program numbers (0-indexed for the API, displayed as 1-indexed in Serial output)
enum GMProgram {
  GM_ACOUSTIC_GRAND_PIANO = 0,
  GM_ELECTRIC_PIANO       = 4,
  GM_DRAWBAR_ORGAN        = 16,
  GM_NYLON_GUITAR         = 24,
  GM_SLAP_BASS            = 36,
  GM_STRING_ENSEMBLE      = 48,
  GM_TRUMPET              = 56,
  GM_SYNTH_PAD            = 88,
};

// A few notes for the demo
#define NOTE_C4  60
#define NOTE_E4  64
#define NOTE_G4  67
#define NOTE_C5  72

// Demo state machine
enum DemoStep {
  STEP_PROGRAM_CHANGE,
  STEP_PITCH_BEND_SIGNED,
  STEP_PITCH_BEND_UNSIGNED,
  STEP_PITCH_BEND_FLOAT,
  STEP_CHANNEL_PRESSURE,
  STEP_MULTI_CHANNEL,
  STEP_COUNT  // total number of steps
};

uint8_t currentStep = 0;

// Simple button press check with debounce.
// With INPUT_PULLUP: not pressed = HIGH (1), pressed = LOW (0).
// Idle state: lower byte saturates to 0xFF from consecutive HIGH reads.
// Press event: after 8 consecutive LOW reads, lower byte reaches 0x00,
// making state == 0xFF00 true for exactly one call per debounced press.
bool isButtonPressed() {
  static uint16_t state = 0;
  state = (state << 1) | digitalRead(BUTTON_PIN) | 0xfe00;
  return (state == 0xff00);
}

// Helper: play a note for a given duration (blocking)
void playNote(uint8_t note, uint8_t velocity, uint8_t channel, uint16_t durationMs) {
  MIDI.noteOn(note, velocity, channel);
  delay(durationMs);
  MIDI.noteOff(note, 0, channel);
}

// Step 1: Cycle through General MIDI programs on channel 1
void demoProgramChange() {
  Serial.println(">> Program Change demo (Channel 1)");

  const uint8_t programs[] = {
    GM_ACOUSTIC_GRAND_PIANO,
    GM_ELECTRIC_PIANO,
    GM_NYLON_GUITAR,
    GM_SYNTH_PAD,
  };

  for (uint8_t i = 0; i < sizeof(programs) / sizeof(programs[0]); i++) {
    MIDI.programChange(programs[i], 1);
    Serial.printf("   Program: %d\n", programs[i] + 1);  // GM programs are 1-indexed for humans
    playNote(NOTE_C4, 100, 1, 400);
    delay(100);
  }

  // Return to piano
  MIDI.programChange(GM_ACOUSTIC_GRAND_PIANO, 1);
}

// Step 2: Pitch Bend using signed int16_t [-8192, 0, 8191]
void demoPitchBendSigned() {
  Serial.println(">> Pitch Bend demo - signed int16_t [-8192..8191]");

  MIDI.noteOn(NOTE_C4, 100, 1);

  // Sweep up
  for (int16_t bend = 0; bend < 8191; bend += 512) {
    MIDI.pitchBend(bend, 1);
    delay(30);
  }
  MIDI.pitchBend((int16_t)8191, 1);  // ensure exact maximum
  delay(30);
  // Sweep back to center
  for (int16_t bend = 8191; bend >= 0; bend -= 512) {
    MIDI.pitchBend(bend, 1);
    delay(30);
  }
  // Sweep down
  for (int16_t bend = 0; bend >= -8192; bend -= 512) {
    MIDI.pitchBend(bend, 1);
    delay(30);
  }
  // Return to center
  MIDI.pitchBend((int16_t)0, 1);
  MIDI.noteOff(NOTE_C4, 0, 1);

  Serial.println("   Pitch bend reset to center");
}

// Step 3: Pitch Bend using unsigned uint16_t [0, 8192, 16383]
void demoPitchBendUnsigned() {
  Serial.println(">> Pitch Bend demo - unsigned uint16_t [0..16383] (center=8192)");

  MIDI.noteOn(NOTE_E4, 100, 1);

  // Sweep from center to max
  for (uint16_t bend = 8192; bend < 16383; bend += 512) {
    MIDI.pitchBend(bend, 1);
    delay(30);
  }
  MIDI.pitchBend((uint16_t)16383, 1);  // ensure exact maximum
  delay(30);
  // Return to center
  MIDI.pitchBend((uint16_t)8192, 1);
  MIDI.noteOff(NOTE_E4, 0, 1);

  Serial.println("   Pitch bend reset to center");
}

// Step 4: Pitch Bend using double [-1.0, 0.0, 1.0]
void demoPitchBendFloat() {
  Serial.println(">> Pitch Bend demo - double [-1.0..1.0]");

  MIDI.noteOn(NOTE_G4, 100, 1);

  // Smooth vibrato effect using floating-point pitch bend
  for (int i = 0; i < 3; i++) {
    for (double bend = 0.0; bend <= 0.3; bend += 0.02) {
      MIDI.pitchBend(bend, 1);
      delay(15);
    }
    for (double bend = 0.3; bend >= -0.3; bend -= 0.02) {
      MIDI.pitchBend(bend, 1);
      delay(15);
    }
    for (double bend = -0.3; bend <= 0.0; bend += 0.02) {
      MIDI.pitchBend(bend, 1);
      delay(15);
    }
  }

  MIDI.pitchBend(0.0, 1);
  MIDI.noteOff(NOTE_G4, 0, 1);

  Serial.println("   Vibrato complete, pitch bend reset");
}

// Step 5: Channel Pressure (Aftertouch)
void demoChannelPressure() {
  Serial.println(">> Channel Pressure (Aftertouch) demo");

  MIDI.noteOn(NOTE_C4, 80, 1);

  // Gradual pressure increase (swell effect)
  for (uint8_t pressure = 0; pressure <= 127; pressure += 8) {
    MIDI.channelPressure(pressure, 1);
    delay(50);
  }
  // Release pressure
  for (int pressure = 127; pressure >= 0; pressure -= 8) {
    MIDI.channelPressure((uint8_t)pressure, 1);
    delay(50);
  }

  MIDI.noteOff(NOTE_C4, 0, 1);
  Serial.println("   Aftertouch swell complete");
}

// Step 6: Multi-channel output (chord across 4 channels)
void demoMultiChannel() {
  Serial.println(">> Multi-Channel demo (Channels 1-4)");

  // Set different instruments on each channel
  MIDI.programChange(GM_ACOUSTIC_GRAND_PIANO, 1);
  MIDI.programChange(GM_STRING_ENSEMBLE, 2);
  MIDI.programChange(GM_SLAP_BASS, 3);
  MIDI.programChange(GM_TRUMPET, 4);

  Serial.println("   Ch1: Piano, Ch2: Strings, Ch3: Bass, Ch4: Trumpet");

  // Play a C major chord spread across channels
  MIDI.noteOn(NOTE_C4, 100, 1);  // Piano: C4
  delay(100);
  MIDI.noteOn(NOTE_E4, 90, 2);   // Strings: E4
  delay(100);
  MIDI.noteOn(NOTE_G4, 85, 3);   // Bass: G4
  delay(100);
  MIDI.noteOn(NOTE_C5, 110, 4);  // Trumpet: C5
  delay(1500);

  // Release all
  MIDI.noteOff(NOTE_C4, 0, 1);
  MIDI.noteOff(NOTE_E4, 0, 2);
  MIDI.noteOff(NOTE_G4, 0, 3);
  MIDI.noteOff(NOTE_C5, 0, 4);

  // Reset all channels to piano
  for (uint8_t ch = 1; ch <= 4; ch++) {
    MIDI.programChange(GM_ACOUSTIC_GRAND_PIANO, ch);
  }

  Serial.println("   Multi-channel chord complete");
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  MIDI.begin();
  USB.begin();

  Serial.println("USB MIDI Multi-Channel Example");
  Serial.println("Press BOOT button to cycle through demos:");
  Serial.println("  1. Program Change");
  Serial.println("  2. Pitch Bend (signed)");
  Serial.println("  3. Pitch Bend (unsigned)");
  Serial.println("  4. Pitch Bend (float/vibrato)");
  Serial.println("  5. Channel Pressure (Aftertouch)");
  Serial.println("  6. Multi-Channel Output");
}

void loop() {
  if (isButtonPressed()) {
    Serial.printf("\n--- Demo %d of %d ---\n", currentStep + 1, STEP_COUNT);

    switch (currentStep) {
      case STEP_PROGRAM_CHANGE:     demoProgramChange(); break;
      case STEP_PITCH_BEND_SIGNED:  demoPitchBendSigned(); break;
      case STEP_PITCH_BEND_UNSIGNED: demoPitchBendUnsigned(); break;
      case STEP_PITCH_BEND_FLOAT:   demoPitchBendFloat(); break;
      case STEP_CHANNEL_PRESSURE:   demoChannelPressure(); break;
      case STEP_MULTI_CHANNEL:      demoMultiChannel(); break;
    }

    currentStep = (currentStep + 1) % STEP_COUNT;
    Serial.println("--- Press button for next demo ---\n");
  }
}
#endif /* ARDUINO_USB_MODE */
