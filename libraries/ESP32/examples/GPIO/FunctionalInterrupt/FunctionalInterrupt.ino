#include <Arduino.h>
#include "FunctionalInterrupts.h"

#if defined(ESP32)
#define BUTTON1 16
#define BUTTON2 17
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
#define BUTTON1 D4
#define BUTTON2 D3
#else
#define BUTTON1 2
#define BUTTON2 0
#endif

class Button {
  public:
    Button(uint8_t reqPin) : PIN(reqPin) {
      pinMode(PIN, INPUT_PULLUP);
      attachInterrupt(PIN, std::bind(&Button::isr, this), FALLING);
    };
    ~Button() {
      detachFunctionalInterrupt(PIN);
    }

#if defined(ESP8266)
    void ICACHE_RAM_ATTR isr()
#elif defined(ESP32)
    void IRAM_ATTR isr()
#endif
    {
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
