# Over the Air through Web browser
OTAWebUpdate is done with a web browser that can be useful in the following typical scenarios:
- Once the application developed and loading directly from Arduino IDE is inconvenient or not possible
- after deployment if user is unable to expose Firmware for OTA from external update server
- provide updates after deployment to small quantity of modules when setting an update server is not practicable

## Requirements
- The ESP and the computer must be connected to the same network

## Implementation
The sample implementation has been done using:
- example sketch OTAWebUpdate.ino
- NodeMCU 1.0 (ESP-12E Module)
You can use another module also if it meets Flash chip size of the sketch
1-Before you begin, please make sure that you have the following software installed:
 - Arduino IDE
 - Host software depending on O/S you use:
  1- Avahi http://avahi.org/ for Linux
  2- Bonjour http://www.apple.com/support/bonjour/ for Windows
  3- Mac OSX and iOS - support is already built in / no any extra s/w is required
