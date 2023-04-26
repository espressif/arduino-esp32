/*
  ledcWrite_RGB.ino
  Runs through the full 255 color spectrum for an rgb led 
  Demonstrate ledcWrite functionality for driving leds with PWM on ESP32
 
  This example code is in the public domain.
  
  Some basic modifications were made by vseven, mostly commenting.
 */
 
// Set up the rgb led names
uint8_t ledR = 0;
uint8_t ledG = 2;
uint8_t ledB = 4; 

const boolean invert = true; // set true if common anode, false if common cathode

uint8_t color = 0;          // a value from 0 to 255 representing the hue
uint32_t R, G, B;           // the Red Green and Blue color components
uint8_t brightness = 255;  // 255 is maximum brightness, but can be changed.  Might need 256 for common anode to fully turn off.

// the setup routine runs once when you press reset:
void setup() 
{            
  Serial.begin(115200);
  delay(10); 
  
  // Initialize pins as LEDC channels 
  // resolution 1-16 bits, freq limits depend on resolution
  ledcAttach(ledR, 12000, 8); // 12 kHz PWM, 8-bit resolution
  ledcAttach(ledG, 12000, 8);
  ledcAttach(ledB, 12000, 8);
}

// void loop runs over and over again
void loop() 
{
  Serial.println("Send all LEDs a 255 and wait 2 seconds.");
  // If your RGB LED turns off instead of on here you should check if the LED is common anode or cathode.
  // If it doesn't fully turn off and is common anode try using 256.
  ledcWrite(ledR, 255);
  ledcWrite(ledG, 255);
  ledcWrite(ledB, 255);
  delay(2000);
  Serial.println("Send all LEDs a 0 and wait 2 seconds.");
  ledcWrite(ledR, 0);
  ledcWrite(ledG, 0);
  ledcWrite(ledB, 0);
  delay(2000);
 
  Serial.println("Starting color fade loop.");
  
 for (color = 0; color < 255; color++) { // Slew through the color spectrum

  hueToRGB(color, brightness);  // call function to convert hue to RGB

  // write the RGB values to the pins
  ledcWrite(ledR, R); // write red component to channel 1, etc.
  ledcWrite(ledG, G);   
  ledcWrite(ledB, B); 
 
  delay(100); // full cycle of rgb over 256 colors takes 26 seconds
 }
 
}

// Courtesy http://www.instructables.com/id/How-to-Use-an-RGB-LED/?ALLSTEPS
// function to convert a color to its Red, Green, and Blue components.

void hueToRGB(uint8_t hue, uint8_t brightness)
{
    uint16_t scaledHue = (hue * 6);
    uint8_t segment = scaledHue / 256; // segment 0 to 5 around the
                                            // color wheel
    uint16_t segmentOffset =
      scaledHue - (segment * 256); // position within the segment

    uint8_t complement = 0;
    uint16_t prev = (brightness * ( 255 -  segmentOffset)) / 256;
    uint16_t next = (brightness *  segmentOffset) / 256;

    if(invert)
    {
      brightness = 255 - brightness;
      complement = 255;
      prev = 255 - prev;
      next = 255 - next;
    }

    switch(segment ) {
    case 0:      // red
        R = brightness;
        G = next;
        B = complement;
    break;
    case 1:     // yellow
        R = prev;
        G = brightness;
        B = complement;
    break;
    case 2:     // green
        R = complement;
        G = brightness;
        B = next;
    break;
    case 3:    // cyan
        R = complement;
        G = prev;
        B = brightness;
    break;
    case 4:    // blue
        R = next;
        G = complement;
        B = brightness;
    break;
   case 5:      // magenta
    default:
        R = brightness;
        G = complement;
        B = prev;
    break;
    }
}
