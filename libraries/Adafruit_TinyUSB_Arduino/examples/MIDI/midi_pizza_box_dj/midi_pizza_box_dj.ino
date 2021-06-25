//Circuit Playground PZ-1 Pizza Box DJ
// by John Park
// a.k.a. DJ Sternocleidomastoid
// for Adafruit Industries
// MIT License
///////////////////////////////////////////////////////////////////////////////
// MIDI controller for Traktor, Mixxx and other DJ software
// Uses TinyUSB to send MIDI over USB
// 
// MIDI controller command mapping must be set up in your DJ software, check
// below for MIDI signal outputs
//
// For detail tutorial https://learn.adafruit.com/circuit-playground-pizza-box-dj-controller
///////////////////////////////////////////////////////////////////////////////

#include <Adafruit_CircuitPlayground.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, usbMIDI);

///////////////////////////////////////////////////////////////////////////////
//MIDI CC (change control) assignments
const int CHANNEL         = 14; //MIDI channel 
const int LEFT_BUTTON_CC  =  4; //Play/Pause Deck A
const int RIGHT_BUTTON_CC = 19; //MIDI CC for left button: FX 1 On
const int CAP1_CONTROL    = 1;  //Select/Set+Store Hotcue 2 (1 is load point)
const int CAP0_CONTROL    = 0;  //Left fader mixes Deck B volume down
const int CAP2_CONTROL    = 2;  //Delete hotcue 2 point       
const int CAP3_CONTROL    = 3;  //Remix Trigger 6-1 or Loop Size /2  
const int CAP12_CONTROL   = 12; //Right fader mixes Deck A volume down
const int CAP6_CONTROL    = 6;  //Scratch (jog turn) Deck B     
const int CAP9_CONTROL    = 9;  //Play/Pause Deck B
const int CAP10_CONTROL   = 10; //Remix Trigger 6-4 or Loop Set 4 bar

int leftButtonState = 0;
int rightButtonState = 0;
int leftButtonLastState = 0;
int rightButtonLastState = 0;
int cap0State = 0;
int cap0LastState = 0;
int cap1State = 0;
int cap1LastState = 0;
int cap2State = 0;
int cap2LastState = 0;
int cap3State = 0;
int cap3LastState = 0;
int cap12State = 0;
int cap12LastState = 0;
int cap6State = 0;
int cap6LastState = 0;
int cap9State = 0;
int cap9LastState = 0;
int cap10State = 0;
int cap10LastState = 0;
bool cap1ON = false; //prevents off command from being spewed
bool cap12ON = false; //prevents off command from being spewed
bool cap6ON = false; //prevents off command from being spewed

const int CAPMIN = 25; //lowest value that's considered an intentional touch to minimize crosstalk
const int CAPSLOP = 0; //value of capacitive pad variance that's considered noise


///////////////////////////////////////////////////////////////////////////////
//mic_meter code
// To keep the display 'lively,' an upper and lower range of volume
// levels are dynamically adjusted based on recent audio history, and
// the graph is fit into this range.
#define  FRAMES 8
uint16_t lvl[FRAMES],           // Audio level for the prior #FRAMES frames
         avgLo  = 6,            // Audio volume lower end of range
         avgHi  = 512,          // Audio volume upper end of range
         sum    = 256 * FRAMES; // Sum of lvl[] array
uint8_t  lvlIdx = 0;            // Counter into lvl[] array
int16_t  peak   = 0;            // Falling dot shows recent max
int8_t   peakV  = 0;            // Velocity of peak dot

///////////////////////////////////////////////////////////////////////////////
void setup() {

  usbMIDI.begin();
  Serial.begin(115200);

  CircuitPlayground.begin();
  CircuitPlayground.setBrightness(30); //make brighter for performance, up to 255
  CircuitPlayground.clearPixels();
  for(uint8_t i=0; i<FRAMES; i++) lvl[i] = 256;

  // Wait until device is enumerated properly before sending MIDI message
  while( !USBDevice.mounted() ) delay(1);
}

///////////////////////////////////////////////////////////////////////////////
// COLOR TABLES for animation ----------------------------------------------

const uint8_t PROGMEM
  reds[]   = { 0x9A, 0x75, 0x00, 0x00, 0x00, 0x65, 0x84, 0x9A, 0xAD, 0xAD },
  greens[] = { 0x00, 0x00, 0x00, 0x87, 0xB1, 0x9E, 0x87, 0x66, 0x00, 0x00 },
  blues[]  = { 0x95, 0xD5, 0xFF, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  gamma8[] = { // Gamma correction improves the appearance of midrange colors
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06,
    0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0E,
    0x0E, 0x0F, 0x0F, 0x10, 0x10, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13, 0x14,
    0x14, 0x15, 0x15, 0x16, 0x16, 0x17, 0x18, 0x18, 0x19, 0x19, 0x1A, 0x1B,
    0x1B, 0x1C, 0x1D, 0x1D, 0x1E, 0x1F, 0x1F, 0x20, 0x21, 0x22, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2A, 0x2B, 0x2C, 0x2D,
    0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x44, 0x45, 0x46,
    0x47, 0x48, 0x49, 0x4B, 0x4C, 0x4D, 0x4E, 0x50, 0x51, 0x52, 0x54, 0x55,
    0x56, 0x58, 0x59, 0x5A, 0x5C, 0x5D, 0x5E, 0x60, 0x61, 0x63, 0x64, 0x66,
    0x67, 0x69, 0x6A, 0x6C, 0x6D, 0x6F, 0x70, 0x72, 0x73, 0x75, 0x77, 0x78,
    0x7A, 0x7C, 0x7D, 0x7F, 0x81, 0x82, 0x84, 0x86, 0x88, 0x89, 0x8B, 0x8D,
    0x8F, 0x91, 0x92, 0x94, 0x96, 0x98, 0x9A, 0x9C, 0x9E, 0xA0, 0xA2, 0xA4,
    0xA6, 0xA8, 0xAA, 0xAC, 0xAE, 0xB0, 0xB2, 0xB4, 0xB6, 0xB8, 0xBA, 0xBC,
    0xBF, 0xC1, 0xC3, 0xC5, 0xC7, 0xCA, 0xCC, 0xCE, 0xD1, 0xD3, 0xD5, 0xD7,
    0xDA, 0xDC, 0xDF, 0xE1, 0xE3, 0xE6, 0xE8, 0xEB, 0xED, 0xF0, 0xF2, 0xF5,
    0xF7, 0xFA, 0xFC, 0xFF };
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
void loop() {

  //Sound activated lights
  uint8_t  i, r, g, b;
  uint16_t minLvl, maxLvl, a, scaled;

  a           = CircuitPlayground.mic.peak(10); // 10 ms of audio
  sum        -= lvl[lvlIdx];
  lvl[lvlIdx] = a;
  sum        += a;                              // Sum of lvl[] array
  minLvl = maxLvl = lvl[0];                     // Calc min, max of lvl[]...
  for(i=1; i<FRAMES; i++) {
    if(lvl[i] < minLvl)      minLvl = lvl[i];
    else if(lvl[i] > maxLvl) maxLvl = lvl[i];
  }

  // Keep some minimum distance between min & max levels,
  // else the display gets "jumpy."
  if((maxLvl - minLvl) < 40) {
    maxLvl = (minLvl < (512-40)) ? minLvl + 40 : 512;
  }
  avgLo = (avgLo * 7 + minLvl + 2) / 8; // Dampen min/max levels
  avgHi = (maxLvl >= avgHi) ?           // (fake rolling averages)
    (avgHi *  3 + maxLvl + 1) /  4 :    // Fast rise
    (avgHi * 31 + maxLvl + 8) / 32;     // Slow decay

  a = sum / FRAMES; // Average of lvl[] array
  if(a <= avgLo) {  // Below min?
    scaled = 0;     // Bargraph = zero
  } else {          // Else scale to fixed-point coordspace 0-2560
    scaled = 2560L * (a - avgLo) / (avgHi - avgLo);
    if(scaled > 2560) scaled = 2560;
  }
  if(scaled >= peak) {            // New peak
    peakV = (scaled - peak) / 4;  // Give it an upward nudge
    peak  = scaled;
  }

  uint8_t  whole  = scaled / 256,    // Full-brightness pixels (0-10)
           frac   = scaled & 255;    // Brightness of fractional pixel
  int      whole2 = peak / 256,      // Index of peak pixel
           frac2  = peak & 255;      // Between-pixels position of peak
  uint16_t a1, a2;                   // Scaling factors for color blending

  for(i=0; i<10; i++) {              // For each NeoPixel...
    if(i <= whole) {                 // In currently-lit area?
      r = pgm_read_byte(&reds[i]),   // Look up pixel color
      g = pgm_read_byte(&greens[i]),
      b = pgm_read_byte(&blues[i]);
      if(i == whole) {               // Fraction pixel at top of range?
        a1 = (uint16_t)frac + 1;     // Fade toward black
        r  = (r * a1) >> 8;
        g  = (g * a1) >> 8;
        b  = (b * a1) >> 8;
      }
    } else {
      r = g = b = 0;                 // In unlit area
    }
    // Composite the peak pixel atop whatever else is happening...
    if(i == whole2) {                // Peak pixel?
      a1 = 256 - frac2;              // Existing pixel blend factor 1-256
      a2 = frac2 + 1;                // Peak pixel blend factor 1-256
      r  = ((r * a1) + (0x84 * a2)) >> 8; // Will
      g  = ((g * a1) + (0x87 * a2)) >> 8; // it
      b  = ((b * a1) + (0xC3 * a2)) >> 8; // blend?
    } else if(i == (whole2-1)) {     // Just below peak pixel
      a1 = frac2 + 1;                // Opposite blend ratios to above,
      a2 = 256 - frac2;              // but same idea
      r  = ((r * a1) + (0x84 * a2)) >> 8;
      g  = ((g * a1) + (0x87 * a2)) >> 8;
      b  = ((b * a1) + (0xC3 * a2)) >> 8;
    }
    CircuitPlayground.strip.setPixelColor(i,
      pgm_read_byte(&gamma8[r]),
      pgm_read_byte(&gamma8[g]),
      pgm_read_byte(&gamma8[b]));
  }
  CircuitPlayground.strip.show();

  peak += peakV;
  if(peak <= 0) {
    peak  = 0;
    peakV = 0;
  } else if(peakV >= -126) {
    peakV -= 2;
  }

  if(++lvlIdx >= FRAMES) lvlIdx = 0;

///////////////////////////////////////////////////////////////////////////////
//BUTTONS
//
//read the buttons
  leftButtonState =  (CircuitPlayground.leftButton());  
  //not pressed = 0, pressed = 1
  rightButtonState = (CircuitPlayground.rightButton());
  
//Left Button
//compare current button states to previous button states
  if(leftButtonState != leftButtonLastState){           //the state has changed
    if(leftButtonState == 1){                           //went from off to on, it's pressed
      usbMIDI.sendControlChange(LEFT_BUTTON_CC, 1, CHANNEL); //send MIDI note ON command
      CircuitPlayground.redLED(HIGH); //turn on the red LED
    }
    else{                   //the button went from on to off, it isn't pressed
      usbMIDI.sendControlChange(LEFT_BUTTON_CC, 0, CHANNEL);  //send MIDI note OFF 
      CircuitPlayground.redLED(LOW);                    //turn off the red LED
    }
    leftButtonLastState = leftButtonState;       //toggle
  }


//Right Button
  if(rightButtonState != rightButtonLastState){           
    if(rightButtonState == 1){                           
      usbMIDI.sendControlChange(RIGHT_BUTTON_CC, 1, CHANNEL); 
      CircuitPlayground.redLED(HIGH); 
    }
    else{                                              
      usbMIDI.sendControlChange(RIGHT_BUTTON_CC, 0, CHANNEL);  
      CircuitPlayground.redLED(LOW);                    
    }
    rightButtonLastState = rightButtonState;              
  }
///////////////////////////////////////////////////////////////////////////////
//Cap sense pads
//
//read the capacative pads
  int cap1  = CircuitPlayground.readCap(1);
  int cap0  = CircuitPlayground.readCap(0);
  int cap2  = CircuitPlayground.readCap(2);
  int cap3  = CircuitPlayground.readCap(3);
  int cap12 = CircuitPlayground.readCap(12);
  int cap6  = CircuitPlayground.readCap(6);
  int cap9  = CircuitPlayground.readCap(9);
  int cap10 = CircuitPlayground.readCap(10);

  //cap1 Crossfader Deck B
  
  if(cap1 > CAPMIN){
    cap1State=1;
  }
  else{
    cap1State=0;
  }
  if(cap1State != cap1LastState){
    if(cap1State==1){
      usbMIDI.sendControlChange(CAP1_CONTROL, 127, CHANNEL);
      CircuitPlayground.redLED(HIGH);
    }
    else {
      usbMIDI.sendControlChange(CAP1_CONTROL, 0, CHANNEL);
      CircuitPlayground.redLED(LOW);
    }
    cap1LastState = cap1State;
  }

  //cap0 
  if(cap0 > CAPMIN){
    cap0State=1;
  }
  else{
    cap0State=0;
  }
  if(cap0State != cap0LastState){
    if(cap0State==1){
      usbMIDI.sendControlChange(CAP0_CONTROL, 1, CHANNEL);
      CircuitPlayground.redLED(HIGH);
    }
    else {
      usbMIDI.sendControlChange(CAP0_CONTROL, 0, CHANNEL);
      CircuitPlayground.redLED(LOW);
    }
    cap0LastState = cap0State;
  }

//cap2
  if(cap2 > CAPMIN){
    cap2State=1;
  }
  else{
    cap2State=0;
  }
  if(cap2State != cap2LastState){
    if(cap2State==1){
      usbMIDI.sendControlChange(CAP2_CONTROL, 1, CHANNEL);
      CircuitPlayground.redLED(HIGH);
    }
    else {
      usbMIDI.sendControlChange(CAP2_CONTROL, 0, CHANNEL);
      CircuitPlayground.redLED(LOW);
    }
    cap2LastState = cap2State;
  }

  //cap3
  if(cap3 > CAPMIN){
    cap3State=1;
  }
  else{
    cap3State=0;
  }
  if(cap3State != cap3LastState){
    if(cap3State==1){
      usbMIDI.sendControlChange(CAP3_CONTROL, 1, CHANNEL);
      CircuitPlayground.redLED(HIGH);
    }
    else {
      usbMIDI.sendControlChange(CAP3_CONTROL, 0, CHANNEL);
      CircuitPlayground.redLED(LOW);
    }
    cap3LastState = cap3State;
  }

  //cap12 Crossfader Deck B

  if(cap12 > CAPMIN){
    cap12State=1;
  }
  else{
    cap12State=0;
  }
  if(cap12State != cap12LastState){
    if(cap12State==1){
      usbMIDI.sendControlChange(CAP12_CONTROL, 127, CHANNEL);
      CircuitPlayground.redLED(HIGH);
    }
    else {
      usbMIDI.sendControlChange(CAP12_CONTROL, 0, CHANNEL);
      CircuitPlayground.redLED(LOW);
    }
    cap12LastState = cap12State;
  }

  //cap6
  if (cap6>CAPMIN){
    int cap6NewVal = map(CircuitPlayground.readCap(6),0,200,0,127);
    if (abs(cap6 - cap6NewVal>CAPSLOP)) {
      cap6 = cap6NewVal;
      usbMIDI.sendControlChange(CAP6_CONTROL, cap6, CHANNEL);
      CircuitPlayground.redLED(HIGH);
      cap6ON = true;
    }
    else{
      if (cap6ON==true){
        usbMIDI.sendControlChange(CAP6_CONTROL, 0, CHANNEL); //send a 0
        CircuitPlayground.redLED(LOW);
        cap6ON=false;
      }
    }
  }

  //cap9
  if(cap9 > CAPMIN){
    cap9State=1;
  }
  else{
    cap9State=0;
  }
  if(cap9State != cap9LastState){
    if(cap9State==1){
      usbMIDI.sendControlChange(CAP9_CONTROL, 1, CHANNEL);
      CircuitPlayground.redLED(HIGH);
    }
    else {
      usbMIDI.sendControlChange(CAP9_CONTROL, 0, CHANNEL);
      CircuitPlayground.redLED(LOW);
    }
    cap9LastState = cap9State;
  }

  //cap10
  if(cap10 > CAPMIN){
    cap10State=1;
  }
  else{
    cap10State=0;
  }
  if(cap10State != cap10LastState){
    if(cap10State==1){
      usbMIDI.sendControlChange(CAP10_CONTROL, 1, CHANNEL);
      CircuitPlayground.redLED(HIGH);
    }
    else {
      usbMIDI.sendControlChange(CAP10_CONTROL, 0, CHANNEL);
      CircuitPlayground.redLED(LOW);
    }
    cap10LastState = cap10State;
  }

  // MIDI Controllers should discard incoming MIDI messages.
  while (usbMIDI.read()) {
  }
}
