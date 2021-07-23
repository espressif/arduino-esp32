/*
 This example is only for ESP devices.

 This example demonstrates usage of callback functions and duplex operation.
 Callback functions allow you to perform audio events pseudo parallel to your main loop.
 This way you no longer need to poll for reading or writing data from/to I2S module.

 Unlike Arduino, ESP allows you to operate input and output simultaneously on separate pins.

Hardware:
   1. ESP board with at least 4 GPIOs available
   2. I2S mirophone (for example this one https://www.adafruit.com/product/3421)
   3. I2S decoder (for example this one https://www.adafruit.com/product/3678)
   4. Headphones, or speaker(s) to be connected into the decoder
   5. Some connecting wires, USB cable and optionally a breadboard

 Wiring:
  Note: If you need to use other than default pins you can change the default definition below this comment block
  1. Connect pins of both the microphone and decoder to common SCK and FS pins on ESP
    1.a SCK (Source Clock, or Bit Clock) connects by default to pin 5
    1.b FS (Frame Select, or Left-Right Select) connects by default to pin 25
  2. Connect data pin of your microphone to pin 35
  3. Connect data pin of your decoder to pin 26
  4. Connect power pins and other remaining pins of your modules according to their specific instructions
  5. Connect headphones/speaker(s) to decoder
  6. Connect ESP board to your computer via USB cable

 Steps to run:
 1. Select target board:
   Tools -> Board -> ESP32 Arduino -> your board
 2. Upload sketch
   Press upload button (arrow in top left corner)
   When you see in console line like this: "Connecting........_____.....__"
     If loading doesn't start automatically, you may need to press and hold Boot
       button and press EN button shortly. Now you can release both buttons.
     You should see lines like this: "Writing at 0x00010000... (12 %)" with rising percentage on each line.
     If this fails, try the board buttons right after pressing upload button, or reconnect the USB cable.
 3. Open plotter
   Tools -> Serial Plotter
 4. Enjoy
  Listen to generated square wave signal, while observing the wave signal recorded by your microphone
    both at the same time thanks to ESP duplex ability.

 Tip:
   Install ArduinoSound library and discover extended functionality of I2S
    you can try ESP WiFi telephone, Record on SD card and Play back from it...

Created by Tomas Pilny
on 23rd June 2021
*/

// If you need to change any of the default pins, simply uncomment chosen line and change the pin number
/*
#define PIN_I2S_SCK      5
#define PIN_I2S_FS       25
#define PIN_I2S_SD       35
#define PIN_I2S_SD_OUT   26
*/

#include <I2S.h>
const int sampleRate = 16000; // sample rate in Hz
const int bitsPerSample = 16;

// code from SimpleTone example
const int frequency = 1250; // frequency of square wave in Hz
const int amplitude = 32767; // amplitude of square wave

const int halfWavelength = (sampleRate / frequency); // half wavelength of square wave

short out_sample = amplitude; // current sample value
int count = 0;

void outputCallback(){

  if (count % halfWavelength == 0) {
    // invert the sample every half wavelength count multiple to generate square wave
    out_sample = -1 * out_sample;
  }

  Serial.printf("write %d\n", out_sample); // only for debug

  // write the same sample twice, once for left and once for the right channel
  I2S.write(out_sample);
  I2S.write(out_sample);

  // increment the counter for the next sample
  count++;
}

// code from InputSerialPlotter example
void inputCallback(){
  // read a sample
  int in_sample = I2S.read();

  if (in_sample) {
    // if it's non-zero print value to serial
    Serial.println(in_sample);
  }
}

void setup() {
  // Open serial communications and wait for port to open:
  // A baud rate of 115200 is used instead of 9600 for a faster data rate
  // on non-native USB ports
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // start I2S at 8 kHz with 32-bits per sample
  if (!I2S.begin(I2S_PHILIPS_MODE, sampleRate, bitsPerSample)) {
    Serial.println("Failed to initialize I2S!");
    while (1) delay(100); // do nothing
  }
  I2S.setAllPins(PIN_I2S_SCK, PIN_I2S_FS, PIN_I2S_SD, PIN_I2S_SD_OUT);

  // Register our callback functions
  I2S.onTransmit(outputCallback); // Function outputCallback will be called each time I2S finishes transmit operation (audio output)
  I2S.onReceive(inputCallback); // Function inputCallback will be called each time I2S finishes receive operation (audio input)
  Serial.println("Callbacks example setup done.");
}

void loop() {
  // loop task remains free for other work
  delay(10); // Let the FreeRTOS reset the watchDogTimer
}
