/*

This is an example how to use Touch Intrrerupts
The sketch will tell when it is touched and then released as like a push-button

This method based on touchInterruptGetLastStatus() is only available for ESP32 S2 and S3
*/

#include "Arduino.h"

int threshold = 1500;  // ESP32S2
bool touch1detected = false;
bool touch2detected = false;

void gotTouch1() {
  touch1detected = true;
}

void gotTouch2() {
  touch2detected = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // give me time to bring up serial monitor

  Serial.println("\n ESP32 Touch Interrupt Test\n");
  touchAttachInterrupt(T1, gotTouch1, threshold);
  touchAttachInterrupt(T2, gotTouch2, threshold);
}

void loop() {
  if (touch1detected) {
    touch1detected = false;
    if (touchInterruptGetLastStatus(T1)) {
      Serial.println(" --- T1 Touched");
    } else {
      Serial.println(" --- T1 Released");
    }
  }
  if (touch2detected) {
    touch2detected = false;
    if (touchInterruptGetLastStatus(T2)) {
      Serial.println(" --- T2 Touched");
    } else {
      Serial.println(" --- T2 Released");
    }
  }

  delay(80);
}
