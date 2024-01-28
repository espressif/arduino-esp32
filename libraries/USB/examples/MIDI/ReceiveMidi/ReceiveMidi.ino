/*
This is an example of receiving USB MIDI 1.0 messages using an ESP32 with a native USB support stack
(S2, S3, etc.).

Receiving and decoding USB MIDI 1.0 packets is a more advanced topic than sending MIDI over USB,
please refer to the other examples in this library for a more basic example of sending MIDI over USB.

This example should still be self explanatory, please refer to the USB MIDI 1.0 specification (the spec)
for a more in-depth explanation of the packet format.

For the spec please visit: https://www.midi.org/specifications-old/item/usb-midi-1-0-specification
*/
#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include "USB.h"
#include "USBMIDI.h"
USBMIDI MIDI;

void setup() {
  Serial.begin(115200);

  MIDI.begin();
  USB.begin();
}

void loop() {
  midiEventPacket_t midi_packet_in = { 0 };

  if (MIDI.readPacket(&midi_packet_in)) {
    printDetails(midi_packet_in);
  }
}

void printDetails(midiEventPacket_t &midi_packet_in) {
  // See Chapter 4: USB-MIDI Event Packets (page 16) of the spec.
  uint8_t cable_num = MIDI_EP_HEADER_CN_GET(midi_packet_in.header);
  midi_code_index_number_t code_index_num = MIDI_EP_HEADER_CIN_GET(midi_packet_in.header);

  Serial.println("Received a USB MIDI packet:");
  Serial.println(".----.-----.--------.--------.--------.");
  Serial.println("| CN | CIN | STATUS | DATA 0 | DATA 1 |");
  Serial.println("+----+-----+--------+--------+--------+");
  Serial.printf("| %d  |  %X  |   %X   |   %X   |   %X   |\n", cable_num, code_index_num,
                midi_packet_in.byte1, midi_packet_in.byte2, midi_packet_in.byte3);
  Serial.println("'----'-----'--------.--------'--------'\n");
  Serial.print("Description: ");

  switch (code_index_num) {
    case MIDI_CIN_MISC:
      Serial.println("This a Miscellaneous event");
      break;
    case MIDI_CIN_CABLE_EVENT:
      Serial.println("This a Cable event");
      break;
    case MIDI_CIN_SYSCOM_2BYTE:  // 2 byte system common message e.g MTC, SongSelect
    case MIDI_CIN_SYSCOM_3BYTE:  // 3 byte system common message e.g SPP
      Serial.println("This a System Common (SysCom) event");
      break;
    case MIDI_CIN_SYSEX_START:      // SysEx starts or continue
    case MIDI_CIN_SYSEX_END_1BYTE:  // SysEx ends with 1 data, or 1 byte system common message
    case MIDI_CIN_SYSEX_END_2BYTE:  // SysEx ends with 2 data
    case MIDI_CIN_SYSEX_END_3BYTE:  // SysEx ends with 3 data
      Serial.println("This a system exclusive (SysEx) event");
      break;
    case MIDI_CIN_NOTE_ON:
      Serial.printf("This a Note-On event of Note %d with a Velocity of %d\n",
                    midi_packet_in.byte2, midi_packet_in.byte3);
      break;
    case MIDI_CIN_NOTE_OFF:
      Serial.printf("This a Note-Off event of Note %d with a Velocity of %d\n",
                    midi_packet_in.byte2, midi_packet_in.byte3);
      break;
    case MIDI_CIN_POLY_KEYPRESS:
      Serial.printf("This a Poly Aftertouch event for Note %d and Value %d\n",
                    midi_packet_in.byte2, midi_packet_in.byte3);
      break;
    case MIDI_CIN_CONTROL_CHANGE:
      Serial.printf("This a Control Change/Continuous Controller (CC) event of Controller %d "
                    "with a Value of %d\n",
                    midi_packet_in.byte2, midi_packet_in.byte3);
      break;
    case MIDI_CIN_PROGRAM_CHANGE:
      Serial.printf("This a Program Change event with a Value of %d\n", midi_packet_in.byte2);
      break;
    case MIDI_CIN_CHANNEL_PRESSURE:
      Serial.printf("This a Channel Pressure event with a Value of %d\n", midi_packet_in.byte2);
      break;
    case MIDI_CIN_PITCH_BEND_CHANGE:
      Serial.printf("This a Pitch Bend Change event with a Value of %d\n",
                    ((uint16_t)midi_packet_in.byte2) << 7 | midi_packet_in.byte3);
      break;
    case MIDI_CIN_1BYTE_DATA:
      Serial.printf("This an embedded Serial MIDI event byte with Value %X\n",
                    midi_packet_in.byte1);
      break;
  }

  Serial.println();
}
#endif /* ARDUINO_USB_MODE */
