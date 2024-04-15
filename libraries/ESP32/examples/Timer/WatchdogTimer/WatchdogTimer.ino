#include "esp_system.h"
#include "rom/ets_sys.h"

const int button = 0;         //gpio to use to trigger delay
const int wdtTimeout = 3000;  //time in ms to trigger the watchdog
hw_timer_t* timer = NULL;

void ARDUINO_ISR_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("running setup");

  pinMode(button, INPUT_PULLUP);                   //init control pin
  timer = timerBegin(1000000);                     //timer 1Mhz resolution
  timerAttachInterrupt(timer, &resetModule);       //attach callback
  timerAlarm(timer, wdtTimeout * 1000, false, 0);  //set time in us
}

void loop() {
  Serial.println("running main loop");

  timerWrite(timer, 0);  //reset timer (feed watchdog)
  long loopTime = millis();
  //while button is pressed, delay up to 3 seconds to trigger the timer
  while (!digitalRead(button)) {
    Serial.println("button pressed");
    delay(500);
  }
  delay(1000);  //simulate work
  loopTime = millis() - loopTime;

  Serial.print("loop time is = ");
  Serial.println(loopTime);  //should be under 3000
}
