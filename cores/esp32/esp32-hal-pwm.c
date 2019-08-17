// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp32-hal-pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "math.h"

static uint16_t __freq = 1000;//1000Hz
static uint8_t __resolution = 8;//8 bits

void __analogWriteResolution(uint8_t bits){
__resolution=bits;	
	
}

void __analogWriteFrequency(uint16_t feq)
{
	__freq=feq;
	
}

void __analogWrite(uint8_t pin,uint16_t val)    //hall sensor without LNA
{
    pinMode(pin,OUTPUT);
	if (val == 0)
	{
		digitalWrite(pin, LOW);
	}
	else if (val >=pow(2,__resolution))
	{
		digitalWrite(pin, HIGH);
	}
	else
	{
   
	int Channel = pwmChannel(pin);
	ledcAttachPin(pin,  Channel);
	ledcSetup( Channel, __freq, __resolution);
	if( 0 <= val || 100 >= val )
   {
	ledcWrite(pwmChannel(pin), val);
   } 
	}
	
	
}

int  pwmChannel(int pin)
{
  int ledChannel = 0;
  
  switch(pin)
  {
    case 16:
    ledChannel = 0;
    break;
    case 17:
    ledChannel = 1;
    break;
    case 25:
    ledChannel = 2;
    break;
    case 32:
    ledChannel = 3;
    break;
    case 33:
    ledChannel = 4;
    break;
    case 18:
    ledChannel = 5;
    break;
    case 21:
    ledChannel = 6;
    break; 
    case 22:
    ledChannel = 7;
    break; 
    case 26:
    ledChannel = 8;
    break;    
    case 27:
    ledChannel = 9;
    break;
    case 1:
    ledChannel = 10;
    break;
    case 2:
    ledChannel = 11;
    break;
    case 12:
    ledChannel = 12;
    break;
    case 13:
    ledChannel = 13;
    break;
    case 14:
    ledChannel = 14;
    break;
    case 15:
    ledChannel = 15;
    break;                
    }
    return ledChannel;
  }


extern void analogWrite(uint8_t pin,uint16_t val) __attribute__ ((weak, alias("__analogWrite")));
extern void analogWriteResolution(uint8_t bits) __attribute__ ((weak, alias("__analogWriteResolution")));
extern void analogWriteFrequency(uint16_t feq) __attribute__ ((weak, alias("__analogWriteFrequency")));