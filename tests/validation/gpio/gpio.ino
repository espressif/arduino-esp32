#include <Arduino.h>
#include <unity.h>

#define BTN 0

void test_button()
{
  Serial.println("Button test");
  static int count = 0;
  static int lastState = HIGH;
  while(count < 3)
  {
    int state = digitalRead(BTN);
    if (state != lastState)
    {
        if (state == LOW)
        {
        count++;
        Serial.print("Button pressed ");
        Serial.print(count);
        Serial.println(" times");
        }
        lastState = state;
    }
    delay(10);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(BTN, INPUT_PULLUP);
  test_button();
}

void loop()
{
}