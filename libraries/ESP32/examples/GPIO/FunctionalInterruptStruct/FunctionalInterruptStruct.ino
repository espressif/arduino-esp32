#include <Arduino.h>

#define BUTTON1 16
#define BUTTON2 17

struct Button {
  uint8_t PIN;
  volatile uint32_t numberKeyPresses;
  volatile int pressed;
};

void isr(void* param) {
  struct Button* button = (struct Button*)param;
  button->numberKeyPresses = button->numberKeyPresses + 1;
  button->pressed = 1;
}

void checkPressed(struct Button* button) {
  if (button->pressed) {
    Serial.printf("Button on pin %u has been pressed %lu times\n", button->PIN, button->numberKeyPresses);
    button->pressed = 0;
  }
}

struct Button button1 = { BUTTON1, 0, 0 };
struct Button button2 = { BUTTON2, 0, 0 };

void setup() {
  Serial.begin(115200);
  pinMode(button1.PIN, INPUT_PULLUP);
  pinMode(button2.PIN, INPUT_PULLUP);
  attachInterruptArg(button1.PIN, isr, (void*)&button1, FALLING);
  attachInterruptArg(button2.PIN, isr, (void*)&button2, FALLING);
}

void loop() {
  checkPressed(&button1);
  checkPressed(&button2);
}
