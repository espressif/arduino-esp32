/*
**********************************************************************************************
@file         Deneyap_Servo.h
@mainpage     Servo Arduino library header file for ESP dev boards
@maintainer   RFtek Electronics <techsupport@rftek.com.tr>
@version      v1.0.0
@brief        This file contains all function prototypes and macros for Servo Arduino library
**********************************************************************************************
*/
#ifndef _DENEYAPSERVO_H
#define _DENEYAPSERVO_H

#include <Arduino.h>

#define PWMFREQ 50
#define PWMCHANNEL 0
#define PWMRESOLUTION 12
#define FIRSTDUTY 0

#define SERVOMIN 0
#define SERVOMAX 180
#define DUTYCYLEMIN 100
#define DUTYCYLEMAX 500

class Servo {
public:
  void attach(int pin, int channel = PWMCHANNEL, int freq = PWMFREQ, int resolution = PWMRESOLUTION);
  void write(int value);
private:
  int _channel;
};

#endif /* _DENEYAPSERVO_H_ */