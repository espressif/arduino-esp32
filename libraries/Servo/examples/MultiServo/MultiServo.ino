/*  MultiServo.ino
 *  3 servo motor shaft rotates. 1.servo:30 degree, 2.servo:60 degree, 3.servo:90 degree
 * 
 *  WARNING: 
 *    - This example code is designed for the SG90 servo. If you are using another servo motor, you should change the sample code.
 *    - If you are using multiple servo motor, you must select another channel.
 *  created 08/11/2022 by zeynepdicle
*/
#include <Deneyap_Servo.h>
 
Servo servo1;
Servo servo2;
Servo servo3;

void setup() {  
  servo1.attach(D3);    /* default: attach(pin, channel=0, freq=50, resolution=12) */
  servo2.attach(D4,1);
  servo3.attach(D5,2);
}

void loop() { 
  servo1.write(30);
  servo2.write(60);
  servo3.write(90);
}