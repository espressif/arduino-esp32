/*  ServoBasic.ino
 *  The servo motor shaft position rotates 60 degree.
 *  
 *  WARNING: 
 *    - This example code is designed for the SG90 servo. If you are using another servo motor, you should change the sample code.
 *    - If you are using multiple servo motor, you must select another channel.
 *  created 08/11/2022 by zeynepdicle
*/
#include <Deneyap_Servo.h>
 
Servo myservo;

void setup() {  
  myservo.attach(D9); /*attach(pin, channel=0, freq=50, resolution=12) */
}

void loop() { 
  myservo.write(60);
}