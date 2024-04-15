#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED
#include "esp32-hal-tinyusb.h"
#include "sdkconfig.h"

#if CONFIG_TINYUSB_MIDI_ENABLED

#pragma once

#define MIDI_EP_HEADER_CN_GET(x) (x >> 4)
#define MIDI_EP_HEADER_CIN_GET(x) ((midi_code_index_number_t)((x)&0xF))

typedef struct {
  uint8_t header;
  uint8_t byte1;
  uint8_t byte2;
  uint8_t byte3;
} midiEventPacket_t;

class USBMIDI {
public:
  USBMIDI(void);
  void begin(void);
  void end(void);

  /* User-level API */

  // Note On
  void noteOn(uint8_t note, uint8_t velocity = 0, uint8_t channel = 1);
  // Note Off
  void noteOff(uint8_t note, uint8_t velocity = 0, uint8_t channel = 1);
  // Program Change
  void programChange(uint8_t inProgramNumber, uint8_t channel = 1);
  // Control Change (Continuous Controller)
  void controlChange(uint8_t inControlNumber, uint8_t inControlValue = 0, uint8_t channel = 1);
  // Polyphonic Key Pressure (Aftertouch)
  void polyPressure(uint8_t note, uint8_t pressure, uint8_t channel = 1);
  // Channel Pressure (Aftertouch)
  void channelPressure(uint8_t pressure, uint8_t channel = 1);
  // Pitch Bend Change [-8192,0,8191]
  void pitchBend(int16_t pitchBendValue, uint8_t channel = 1);
  // Pitch Bend Change [0,8192,16383]
  void pitchBend(uint16_t pitchBendValue, uint8_t channel = 1);
  // Pitch Bend Change [-1.0,0,1.0]
  void pitchBend(double pitchBendValue, uint8_t channel = 1);

  /* USB MIDI 1.0 interface */

  // Attempt to read a USB MIDI packet from the USB Bus
  bool readPacket(midiEventPacket_t *packet);
  // Attempt to write a USB MIDI packet to the USB Bus
  bool writePacket(midiEventPacket_t *packet);

  /* Serial MIDI 1.0 interface */

  // Write a Serial MIDI byte (status or data) to the USB Bus
  size_t write(uint8_t c);
};

#endif /* CONFIG_TINYUSB_MIDI_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
