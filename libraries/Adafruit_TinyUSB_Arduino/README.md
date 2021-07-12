# Adafruit TinyUSB Library for Arduino

[![Build Status](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/workflows/Build/badge.svg)](https://github.com/adafruit/Adafruit_TinyUSB_Arduino/actions) [![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

This library is a Arduino-friendly version of [TinyUSB](https://github.com/hathach/tinyusb) stack. It is designed with structure and APIs that are easily integrated to existing or new Arduino Core. Supported platform including: 

- [Adafruit_nRF52_Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino)
- [Adafruit/ArduinoCore-samd](https://github.com/adafruit/ArduinoCore-samd) selectable via menu`Tools->USB Stack->TinyUSB`
- [earlephilhower/arduino-pico](https://github.com/earlephilhower/arduino-pico) selectable via menu `Tools->USB Stack->Adafruit TinyUSB`

Current class drivers supported are

- Communication (CDC): which is used to implement `Serial` monitor
- Human Interface Device (HID): Generic (In & Out), Keyboard, Mouse, Gamepad etc ...
- Mass Storage Class (MSC): with multiple LUNs
- Musical Instrument Digital Interface (MIDI)
- WebUSB with vendor specific class

For supported ArduinoCore, to use this library, you only need to have `<Adafruit_TinyUSB.h>` in your sketch. If your ArduinoCore does not support TinyUSB library yet, it is rather simple to port.

## Class Driver API

More document to write ... 

## Porting Guide

It is rather easy if you want to integrate TinyUSB lib to your ArduinoCore.

### ArduinoCore Changes

1. Add this repo as submodule (or have local copy) at your ArduioCore/libraries/Adafruit_TinyUSB_Arduino (much like SPI).
2. Since Serial as CDC is considered as part of the core, we need to have `#include "Adafruit_USBD_CDC.h"` within your `Arduino.h`. For this to work, your `platform.txt` include path need to have `"-I{runtime.platform.path}/libraries/Adafruit_TinyUSB_Arduino/src/arduino"`.
3. In your `main.cpp` before setup() invoke the `TinyUSB_Device_Init(rhport)`. This will initialize usb device hardware and tinyusb stack and also include Serial as an instance of CDC class.
4. `TinyUSB_Device_Task()` must be called whenever there is new USB event. Depending on your core and MCU with or without RTOS. There are many ways to run the task. For example:
  - Use USB IRQn to set flag then invoke function later on after exiting IRQ.
  - Just invoke function after the loop(), within yield(), and delay()
5. `TinyUSB_Device_FlushCDC()` should also be called often to send out Serial data as well.
6. Note: For low power platform that make use of WFI()/WFE(), extra care is required before mcu go into low power mode. Check out my PR to circuipython for reference https://github.com/adafruit/circuitpython/pull/2956

### Library Changes

In addition to core changes, library need to be ported to your platform. Don't worry, tinyusb stack has already done most of heavy-lifting. You only need to write a few APIs

1. `TinyUSB_Port_InitDevice()` hardware specific (clock, phy) to enable usb hardware then call tud_init(). This API is called as part of TinyUSB_Device_Init() invocation.
2. `TinyUSB_Port_EnterDFU()` which is called when device need to enter DFU mode, usually by touch1200 feature
3. `TinyUSB_Port_GetSerialNumber()` which is called to get unique MCU Serial ID to used as Serial string descriptor.
