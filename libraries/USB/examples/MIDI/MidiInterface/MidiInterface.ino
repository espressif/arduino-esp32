/*
This is an example of using an ESP32 with a native USB support stack (S2, S3, etc.) as a Serial MIDI to
USB MIDI bridge (AKA "A MIDI Interface").

View this sketch in action on YouTube: https://youtu.be/BXG5i55I9s0

Receiving and decoding USB MIDI 1.0 packets is a more advanced topic than sending MIDI over USB,
please refer to the other examples in this library for a more basic example of sending MIDI over USB.

This example should still be self explanatory, please refer to the USB MIDI 1.0 specification (the spec)
for a more in-depth explanation of the packet format.

For the spec please visit: https://www.midi.org/specifications-old/item/usb-midi-1-0-specification

Note: Because ESP32 works at VCC=3.3v normal schematics for Serial MIDI connections will not suffice,
      Please refer to the Updated MIDI 1.1 Electrical Specification [1] for information on how to hookup
      Serial MIDI for 3.3v devices.

[1] - https://www.midi.org/specifications/midi-transports-specifications/5-pin-din-electrical-specs
*/
#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include "USB.h"
#include "USBMIDI.h"
USBMIDI MIDI;

#define MIDI_RX 39
#define MIDI_TX 40

void setup() {
  // USBCDC Serial
  Serial.begin(115200);

  // HW UART Serial
  Serial1.begin(31250, SERIAL_8N1, MIDI_RX, MIDI_TX);

  MIDI.begin();
  USB.begin();
}

void loop() {
  // MIDI Serial 1.0 to USB MIDI 1.0
  if (Serial1.available()) {
    byte data = Serial1.read();
    MIDI.write(data);
  }

  // USB MIDI 1.0 to MIDI Serial 1.0
  midiEventPacket_t midi_packet_in = {0};
  // See Chapter 4: USB-MIDI Event Packets (page 16) of the spec.
  int8_t cin_to_midix_size[16] = {-1, -1, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};

  if (MIDI.readPacket(&midi_packet_in)) {
    midi_code_index_number_t code_index_num = MIDI_EP_HEADER_CIN_GET(midi_packet_in.header);
    int8_t midix_size = cin_to_midix_size[code_index_num];

    // We skip Misc and Cable Events for simplicity
    if (code_index_num >= 0x2) {
      for (int i = 0; i < midix_size; i++) {
        Serial1.write(((uint8_t *)&midi_packet_in)[i + 1]);
      }
      Serial1.flush();
    }
  }
}
#endif /* ARDUINO_USB_MODE */
