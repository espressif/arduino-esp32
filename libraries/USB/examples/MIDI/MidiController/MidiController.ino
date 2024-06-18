/*
This is an example of a Simple MIDI Controller using an ESP32 with a native USB support stack (S2, S3,
etc.).

For a hookup guide and more information on reading the ADC, please see:
https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/ (Note: This sketch uses GPIO05)

For best results, it is recommended to add an extra offset resistor between VCC and the potentiometer.
(For a standard 10kOhm potentiometer, 3kOhm - 4kOhm will do.)

View this sketch in action on YouTube: https://youtu.be/Y9TLXs_3w1M
*/
#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include <math.h>

#include "USB.h"
#include "USBMIDI.h"
USBMIDI MIDI;

#define MIDI_NOTE_C4 60

#define MIDI_CC_CUTOFF 74

///// ADC & Controller Input Handling /////

#define CONTROLLER_PIN  5

// ESP32 ADC needs a ton of smoothing
#define SMOOTHING_VALUE 1000
static double controllerInputValue = 0;

void updateControllerInputValue() {
  controllerInputValue = (controllerInputValue * (SMOOTHING_VALUE - 1) + analogRead(CONTROLLER_PIN)) / SMOOTHING_VALUE;
}

void primeControllerInputValue() {
  for (int i = 0; i < SMOOTHING_VALUE; i++) {
    updateControllerInputValue();
  }
}

uint16_t readControllerValue() {
  // Lower ADC input amplitude to get a stable value
  return round(controllerInputValue / 12);
}

///// Button Handling /////

#define BUTTON_PIN 0

// Simple button state transition function with debounce
// (See also: https://tinyurl.com/simple-debounce)
#define PRESSED    0xff00
#define RELEASED   0xfe1f
uint16_t getButtonEvent() {
  static uint16_t state = 0;
  state = (state << 1) | digitalRead(BUTTON_PIN) | 0xfe00;
  return state;
}

///// Arduino Hooks /////

void setup() {
  Serial.begin(115200);
  MIDI.begin();
  USB.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  primeControllerInputValue();
}

void loop() {
  uint16_t newControllerValue = readControllerValue();
  static uint16_t lastControllerValue = 0;

  // Auto-calibrate the controller range
  static uint16_t maxControllerValue = 0;
  static uint16_t minControllerValue = 0xFFFF;

  if (newControllerValue < minControllerValue) {
    minControllerValue = newControllerValue;
  }
  if (newControllerValue > maxControllerValue) {
    maxControllerValue = newControllerValue;
  }

  // Send update if the controller value has changed
  if (lastControllerValue != newControllerValue) {
    lastControllerValue = newControllerValue;

    // Can't map if the range is zero
    if (minControllerValue != maxControllerValue) {
      MIDI.controlChange(MIDI_CC_CUTOFF, map(newControllerValue, minControllerValue, maxControllerValue, 0, 127));
    }
  }

  updateControllerInputValue();

  // Hook Button0 to a MIDI note so that we can observe
  // the CC effect without the need for a MIDI keyboard.
  switch (getButtonEvent()) {
    case PRESSED:  MIDI.noteOn(MIDI_NOTE_C4, 64); break;
    case RELEASED: MIDI.noteOff(MIDI_NOTE_C4, 0); break;
    default:       break;
  }
}
#endif /* ARDUINO_USB_MODE */
