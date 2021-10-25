/*
 This example is only for ESP devices.

 This example demonstrates usage of integrated Digital to Analog Converter (DAC)
 You can display sound wave from audio device, or just measure voltage.

 To display audio prepare circuit found in following link or drafted as ASCII art
 https://forum.arduino.cc/index.php?topic=567581.0
 (!) Note that unlike in the link we are connecting the supply line to 3.3V (not 5V)
 because ADC can measure only up to around 3V. Anything above 3V will be very inaccurate.

                      ^ +3.3V
                      |
                      _
                     | |10k
                     |_|
                      |            | |10uF
   GPIO34-------------*------------| |----------- line in
(Default ADC pin)     |           +| |      
                      |
                      _
                     | |10k
                     |_|
                      |
                      |
                      V GND

Connect hot wire of your audio to Line in and GNd wire of audio cable to common ground (GND)

Second option to measure voltage on trimmer / potentiometer has following connection
                      ^ +3.3V
                      |
                      _
                     | |
   GPIO34----------->| |
(Default ADC pin)    |_|
                      |
                      |
                      _
                     | | optional resistor
                     |_|
                      |
                      |
                      V GND
 Optional resistor will decrease read value.

 Steps to run:
 1. Select target board:
   Tools -> Board -> ESP32 Arduino -> your board
 2. Upload sketch
   Press upload button (arrow in top left corner)
   When you see in console line like this: "Connecting........_____.....__"
     On your board press and hold Boot button and press EN button shortly. Now you can release both buttons.
     You should see lines like this: "Writing at 0x00010000... (12 %)" with rising percentage on each line.
     If this fails, try the board buttons right after pressing upload button, or reconnect the USB cable.
 3. Open plotter
   Tools -> Serial Plotter
     Enjoy

Created by Tomas Pilny
on 17th June 2021
*/

#include <I2S.h>

void setup() {
  // Open serial communications and wait for port to open:
  // A baud rate of 115200 is used instead of 9600 for a faster data rate
  // on non-native USB ports
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // start I2S at 8 kHz with 32-bits per sample
  if (!I2S.begin(ADC_DAC_MODE, 8000, 16)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }
}

void loop() {
  // read a sample
  int sample = I2S.read();
  Serial.println(sample);
}
