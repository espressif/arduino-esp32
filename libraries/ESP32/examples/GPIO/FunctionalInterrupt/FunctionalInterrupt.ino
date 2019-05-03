#include <Arduino.h>
#include <Schedule.h>
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
      // Arduino C API:
      attachInterruptArg(PIN, [](void* self) { static_cast<Button*>(self)->isr(); }, this, FALLING); // works on ESP32; fails on ESP8266: "ISR not in IRAM"
      //attachInterruptArg(PIN, reinterpret_cast<void(*)(void*)>(&isr_static), this, FALLING); // works on ESP32; works on ESP8266
      // FunctionalInterrupts API:
      //attachInterrupt(PIN, [this]() { isr(); }, FALLING); // works on ESP32; works on ESP8266
      //attachScheduledInterrupt(PIN, [this](InterruptInfo ii) { Serial.print("Pin "); Serial.println(ii.pin); isr(); }, FALLING); // works on ESP32; works on ESP8266
    };
    ~Button() {
      detachInterrupt(PIN);
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

#if defined(ESP8266)
    static void ICACHE_RAM_ATTR isr_static(Button* const self)
#elif defined(ESP32)
    static void IRAM_ATTR isr_static(Button* const self)
#endif
    {
        self->isr();
    }

    void checkPressed() {
      if (pressed) {
        Serial.printf("Button on pin %u has been pressed %u times\n", PIN, numberKeyPresses);
        pressed = false;
      }
    }

  private:
    const uint8_t PIN;
    volatile uint32_t numberKeyPresses = 0;
    volatile bool pressed = false;
};

Button* button1;
Button* button2;


void setup() {
  Serial.begin(115200);
  schedule_function([]() { Serial.println("Scheduled function"); });
  Serial.println("FunctionalInterrupt test/example");

  button1 = new Button(BUTTON1);
  button2 = new Button(BUTTON2);

  Serial.println("setup() complete");
}

void loop() {
  button1->checkPressed();
  button2->checkPressed();
}
