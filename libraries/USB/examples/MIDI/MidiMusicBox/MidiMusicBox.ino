/*
This is an example of a MIDI Music Box using an ESP32 with a native USB support stack (S2, S3, etc.).

Every time the button on the ESP32 board (attached to pin 0) is pressed the next note of a melody is
played. It is up to the user to get the timing of the button presses right.

One simple way of running this sketch is to download the Pianoteq evaluation version, because upon
application start it automatically listens to the first MIDI Input on Channel 1, which is the case,
if the ESP32 is the only MIDI device attached.

View this sketch in action on YouTube: https://youtu.be/JFrc-wSmcus
*/
#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include "USB.h"
#include "USBMIDI.h"
USBMIDI MIDI;

#define END_OF_SONG 255
uint8_t notes[] = {END_OF_SONG, 71, 76, 79, 78, 76, 83, 81, 78, 76, 79, 78, 75, 77, 71};
uint8_t noteIndex = 0;  // From 0 to sizeof(notes)
#define SONG_LENGTH (sizeof(notes) - 1)  // END_OF_SONG does not attribute to the length.

#define BUTTON_PIN 0

// Simple button press check with debounce
// (See also: https://tinyurl.com/simple-debounce)
bool isButtonPressed() {
  static uint16_t state = 0;
  state = (state << 1) | digitalRead(BUTTON_PIN) | 0xfe00;
  return (state == 0xff00);
}

void setup() {
  Serial.begin(115200);
  // Make the BUTTON_PIN an input:
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  MIDI.begin();
  USB.begin();
}

void loop() {
  if (isButtonPressed()) {
    // Stop current note
    MIDI.noteOff(notes[noteIndex]);

    // Play next note
    noteIndex = noteIndex < SONG_LENGTH ? noteIndex + 1 : 0;
    if (notes[noteIndex] != END_OF_SONG) {
      MIDI.noteOn(notes[noteIndex], 64);
    }
  }
}
#endif /* ARDUINO_USB_MODE */
