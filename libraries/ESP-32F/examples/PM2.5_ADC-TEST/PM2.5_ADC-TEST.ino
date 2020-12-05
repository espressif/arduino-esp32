/*-------------------------------------------------------------------------
                              
                              果云科技
                           ESP-32F 开发板
                              
                        ADC采集 粉尘传感器 实验
-------------------------------------------------------------------------*/

#include "Arduino.h"
#define  dustPin  35  //定义adc采集引脚
#define  ledPower 21  //定义led发射引脚
int value;
float dustVal=0;
 

int delayTime=280;
int delayTime2=40;
float offTime=9680;
 
void setup() {
  // put your setup code here, to run once:
   pinMode(ledPower,OUTPUT);
   adcAttachPin(dustPin);
   adcStart(dustPin);
   Serial.begin(115200);
     
     
}

void loop() {
  // put your main code here, to run repeatedly:
      digitalWrite(ledPower,LOW); 
      delayMicroseconds(delayTime);
      dustVal=analogRead(dustPin);
     // Serial.write(value>>8); Serial.write(value); 
      delayMicroseconds(delayTime2);
      digitalWrite(ledPower,HIGH); 
      delayMicroseconds(offTime);
       
      delay(1000);
      if (dustVal>36.455)
      Serial.println((float(dustVal/4096)-0.0356)*120000*0.035);
}
