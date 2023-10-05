/*
 * This example demonstrates usage of interrupt by detecting a button press.
 *
 * Setup: Connect first button between pin defined in BUTTON1 and GND
 *        Similarly connect second button between pin defined in BUTTON2 and GND.
 *        If you do not have a button simply connect a wire to those buttons
 *        - touching GND pin with other end of the wire will behave same as pressing the connected button.
 *        Wen using the bare wire be careful not to touch any other pin by accident.
 *
 * Note: There is no de-bounce implemented and the physical connection will normally
 *       trigger many more button presses than actually happened.
 *       This is completely normal and is not to be considered a fault.
 */


#include <Arduino.h>
#include <FunctionalInterrupt.h>

#define BUTTON1 16
#define BUTTON2 17

class Button{
public:
  Button(uint8_t reqPin) : PIN(reqPin){
    pinMode(PIN, INPUT_PULLUP);
  };

  void begin(){
    attachInterrupt(PIN, std::bind(&Button::isr,this), FALLING);
    Serial.printf("Started button interrupt on pin %d\n", PIN);
  }

  ~Button(){
    detachInterrupt(PIN);
  }

  void ARDUINO_ISR_ATTR isr(){
    numberKeyPresses += 1;
    pressed = true;
  }

  void checkPressed(){
    if (pressed) {
      Serial.printf("Button on pin %u has been pressed %u times\n", PIN, numberKeyPresses);
      pressed = false;
    }
  }

private:
    const uint8_t PIN;
    volatile uint32_t numberKeyPresses;
    volatile bool pressed;
};

Button button1(BUTTON1);
Button button2(BUTTON2);

void setup() {
    Serial.begin(115200);
    while(!Serial) delay(10);
    Serial.println("Starting Functional Interrupt example.");
    button1.begin();
    button2.begin();
    Serial.println("Setup done.");
}

void loop() {
  button1.checkPressed();
  button2.checkPressed();
}
