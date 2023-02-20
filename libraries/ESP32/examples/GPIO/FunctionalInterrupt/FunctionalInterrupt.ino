#include <Arduino.h>
#include <FunctionalInterrupt.h>

#define BUTTON1 16
#define BUTTON2 17

class Button
{
public:
  Button(uint8_t reqPin) : PIN(reqPin){
    pinMode(PIN, INPUT_PULLUP);
    attachInterrupt(PIN, std::bind(&Button::isr,this), FALLING);
  };
  ~Button() {
    detachInterrupt(PIN);
  }

  void ARDUINO_ISR_ATTR isr() {
    numberKeyPresses += 1;
    pressed = true;
  }

  void checkPressed() {
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
}

void loop() {
  button1.checkPressed();
  button2.checkPressed();
}
