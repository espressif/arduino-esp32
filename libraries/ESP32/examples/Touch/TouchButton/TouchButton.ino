/*

This is an example how to use Touch Intrrerupts
The sketh will tell when it is touched and then relesased as like a push-button

This method based on touchInterruptSetThresholdDirection() is only available for ESP32
*/

#include "Arduino.h"

int threshold = 40;
bool touchActive = false;
bool lastTouchActive = false;
bool testingLower = true;

void gotTouchEvent(){
  if (lastTouchActive != testingLower) {
    touchActive = !touchActive;
    testingLower = !testingLower;
    // Touch ISR will be inverted: Lower <--> Higher than the Threshold after ISR event is noticed
    touchInterruptSetThresholdDirection(testingLower);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // give me time to bring up serial monitor
  Serial.println("ESP32 Touch Interrupt Test");
  touchAttachInterrupt(T2, gotTouchEvent, threshold);

  // Touch ISR will be activated when touchRead is lower than the Threshold
  touchInterruptSetThresholdDirection(testingLower);
}

void loop(){
  if(lastTouchActive != touchActive){
    lastTouchActive = touchActive;
    if (touchActive) {
      Serial.println("  ---- Touch was Pressed");
    } else {
      Serial.println("  ---- Touch was Released");
    }
  }
  Serial.printf("T2 pin2 = %d \n", touchRead(T2));
  delay(125);
}
