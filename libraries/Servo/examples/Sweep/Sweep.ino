/*  Sweep.ino
 *  The servo motor shaft rotates back and forth with 15 degree steps in the range of 0-180 degrees. 
 * 
 *  WARNING: 
 *    - This example code is designed for the SG90 servo. If you are using another servo motor, you should change the sample code.
 *    - If you are using multiple servo motor, you must select another channel.
 *  created 08/11/2022 by zeynepdicle
*/
#include <Deneyap_Servo.h>
 
Servo myservo;

int pos = 0; 

void setup() {  
  myservo.attach(D9);   /*dafault: attach(pin, channel=0, freq=50, resolution=12)*/
}

void loop() {
  for (pos = 0; pos <= 180; pos += 15) {
    myservo.write(pos);
    delay(1000);
  }
  for (pos = 180; pos >= 0; pos -= 15) {  
    myservo.write(pos);
    delay(1000);
  } 
}