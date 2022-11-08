/*  Knob.ino
 *  According to the potentiometer value, the servo motor shaft position connected to the D9 pin rotates.
 * 
 *  WARNING: 
 *    - This example code is designed for the SG90 servo. If you are using another servo motor, you should change the sample code.
 *    - If you are using multiple servo motor, you must select another channel.
 *  created 08/11/2022 by zeynepdicle
*/
#include <Deneyap_Servo.h>
   
Servo myservo;

int potpin = A0;
int val; 

void setup() {  
  Servo.attach(D9);     /*dafault: attach(pin, channel=0, freq=50, resolution=12)*/
  pinMode(A0, INPUT);
}

void loop() { 
  val = analogRead(potpin);
  val = map(val, 0, 4095, 0, 180);
  Servo.write(val);
  delay(15);
}