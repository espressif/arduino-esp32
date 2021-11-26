# I2S unit tests

Set of both codes and ESPs act as automated unit test to verify Arduino-like I2S lib.
These tests can be run after every commit to catch potential bugs.

unit_tests_ino contains sketch using Arduino-like I2S library. This sketch is intended to be flashed onto ESP32 (or Arduino supporting I2S) and connected to another ESP32 using code from unit_tests_idf.

unit_tests_idf contains IDF code acting as a counterpart for Arduino-like I2S lib.

### Hardware:
-   2 pcs ESP32, or 1 ESP32 and Arduino supporting I2S
-    7 wires (connecting I2S and I2C + GND)
-    2 USB cables for ESP connections
-    Optional: Logic analyzer + oscilloscope
-    Optional: breadboard

### Setup:
-   Connect pins:

Chose if you want to test ESP32 or Arduino and connect one of those to second ESP32

        Arduino MKR-Zero |   ESP32  |   ESP32
            ino code     | ino code | idf code | note
                  ?      |    GND   |   GND    |
                  ?      |     14   |    14    | I2S CLK
                  ?      |     25   |    25    | I2S WS
                  ?      |     35   |    26    | I2S ino-input  <- idf-output
                  ?      |     26   |    35    | I2S ino-output -> idf-input
                  1(TX)  |     21   |    22    | UART ino-TX -> idf-RX
                  0(RX)  |     22   |    21    | UART ino-RX <- idf-TX
-   Connect USB cables and flash
-   Boards should detect each other via I2C and start testing automatically and report partial results and overall result when finished.
-   Connect logic analyzer on I2S bus

### Build IDF counterpart
-   Download [esp-idf](https://github.com/espressif/esp-idf) and [install](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#step-3-set-up-the-tools)
-   Build with `idf.py build`
-   Flash with `idf.py -p /dev/ttyUSB0 monitor flash`


### TODO
-   ADC and DAC connections and verification
-   Pin change - connect additional set of I2S bus to different pins to verify pin change works properly