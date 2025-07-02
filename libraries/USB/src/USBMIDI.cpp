#include "USBMIDI.h"
#if SOC_USB_OTG_SUPPORTED

#if CONFIG_TINYUSB_MIDI_ENABLED

#include "Arduino.h"
#include "esp32-hal-tinyusb.h"

// Default Cable Number (for simplified APIs that do not expose this)
#define DEFAULT_CN 0

static bool tinyusb_midi_descriptor_loaded = false;
static bool tinyusb_midi_interface_enabled = false;

extern "C" uint16_t tusb_midi_load_descriptor(uint8_t *dst, uint8_t *itf) {
  if (tinyusb_midi_descriptor_loaded) {
    return 0;
  }
  tinyusb_midi_descriptor_loaded = true;

  uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB MIDI");
  uint8_t ep_in = tinyusb_get_free_in_endpoint();
  TU_VERIFY(ep_in != 0);
  uint8_t ep_out = tinyusb_get_free_out_endpoint();
  TU_VERIFY(ep_out != 0);
  uint8_t descriptor[TUD_MIDI_DESC_LEN] = {
    TUD_MIDI_DESCRIPTOR(*itf, str_index, ep_out, (uint8_t)(0x80 | ep_in), CFG_TUD_ENDOINT_SIZE),
  };
  *itf += 2;
  memcpy(dst, descriptor, TUD_MIDI_DESC_LEN);

  return TUD_MIDI_DESC_LEN;
}

USBMIDI::USBMIDI() {
  if (!tinyusb_midi_interface_enabled) {
    tinyusb_midi_interface_enabled = true;
    tinyusb_enable_interface(USB_INTERFACE_MIDI, TUD_MIDI_DESC_LEN, tusb_midi_load_descriptor);
  } else {
    log_e("USBMIDI: Multiple instances of USBMIDI not supported!");
  }
}

void USBMIDI::begin() {}
void USBMIDI::end() {}

// uint compatible version of constrain
#define uconstrain(amt, low, high) ((amt) <= (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

#define STATUS(CIN, CHANNEL) static_cast<uint8_t>(((CIN & 0x7F) << 4) | (uconstrain(CHANNEL - 1, 0, 15) & 0x7F))

// Note: All the user-level API calls do extensive input constraining to prevent easy to make mistakes.
// (You can thank me later.)
#define _(x) static_cast<uint8_t>(uconstrain(x, 0, 127))

// Note On
void USBMIDI::noteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
  midiEventPacket_t event = {MIDI_CIN_NOTE_ON, STATUS(MIDI_CIN_NOTE_ON, channel), _(note), _(velocity)};
  writePacket(&event);
}

// Note Off
void USBMIDI::noteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
  midiEventPacket_t event = {MIDI_CIN_NOTE_OFF, STATUS(MIDI_CIN_NOTE_OFF, channel), _(note), _(velocity)};
  writePacket(&event);
}

// Program Change
void USBMIDI::programChange(uint8_t program, uint8_t channel) {
  midiEventPacket_t event = {MIDI_CIN_PROGRAM_CHANGE, STATUS(MIDI_CIN_PROGRAM_CHANGE, channel), _(program), 0x0};
  writePacket(&event);
}

// Control Change (Continuous Controller)
void USBMIDI::controlChange(uint8_t control, uint8_t value, uint8_t channel) {
  midiEventPacket_t event = {MIDI_CIN_CONTROL_CHANGE, STATUS(MIDI_CIN_CONTROL_CHANGE, channel), _(control), _(value)};
  writePacket(&event);
}

// Polyphonic Key Pressure (Aftertouch)
void USBMIDI::polyPressure(uint8_t note, uint8_t pressure, uint8_t channel) {
  midiEventPacket_t event = {MIDI_CIN_POLY_KEYPRESS, STATUS(MIDI_CIN_POLY_KEYPRESS, channel), _(note), _(pressure)};
  writePacket(&event);
}

// Channel Pressure (Aftertouch)
void USBMIDI::channelPressure(uint8_t pressure, uint8_t channel) {
  midiEventPacket_t event = {MIDI_CIN_CHANNEL_PRESSURE, STATUS(MIDI_CIN_CHANNEL_PRESSURE, channel), _(pressure), 0x0};
  writePacket(&event);
}

// Pitch Bend Change [-8192,0,8191]
void USBMIDI::pitchBend(int16_t value, uint8_t channel) {
  uint16_t pitchBendValue = constrain(value, -8192, 8191) + 8192;
  pitchBend(pitchBendValue);
}

// Pitch Bend Change [0,8192,16383]
void USBMIDI::pitchBend(uint16_t value, uint8_t channel) {
  uint16_t pitchBendValue = static_cast<uint16_t>(uconstrain(value, 0, 16383));
  // Split the 14-bit integer into two 7-bit values
  uint8_t lsb = pitchBendValue & 0x7F;         // Lower 7 bits
  uint8_t msb = (pitchBendValue >> 7) & 0x7F;  // Upper 7 bits

  midiEventPacket_t event = {MIDI_CIN_PITCH_BEND_CHANGE, STATUS(MIDI_CIN_PITCH_BEND_CHANGE, channel), lsb, msb};
  writePacket(&event);
}

// Pitch Bend Change [-1.0,0,1.0]
void USBMIDI::pitchBend(double value, uint8_t channel) {
  // Multiply by 8191 and round to nearest integer
  int16_t pitchBendValue = static_cast<int16_t>(round(constrain(value, -1.0, 1.0) * 8191.0));

  pitchBend(pitchBendValue, channel);
}

bool USBMIDI::readPacket(midiEventPacket_t *packet) {
  return tud_midi_packet_read((uint8_t *)packet);
}

bool USBMIDI::writePacket(midiEventPacket_t *packet) {
  return tud_midi_packet_write((uint8_t *)packet);
}

size_t USBMIDI::write(uint8_t c) {
  // MIDI_CIN_1BYTE_DATA => Verbatim MIDI byte-stream copy
  // (See also Table 4-1 of USB MIDI spec 1.0)
  midiEventPacket_t packet = {DEFAULT_CN | MIDI_CIN_1BYTE_DATA, c, 0, 0};

  return tud_midi_packet_write((uint8_t *)&packet);
}

#endif /* CONFIG_TINYUSB_MIDI_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
