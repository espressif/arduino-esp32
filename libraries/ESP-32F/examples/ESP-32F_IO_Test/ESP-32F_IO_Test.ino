/*--------------------------------
          果云科技
       ESP-32F 开发板

          RGB 实验
---------------------------------*/
#include<stdio.h>
#include<stdlib.h>
#include "WiFi.h"
#define STA_SSID "your-ssid"
#define STA_PASS "your-pass"
#define AP_SSID  "ESP"

 using namespace std;
char IO_Num[40]={32,33,25,26,27,14,12,13,15,2,4,0,17,16,5,18,19,21,22,23};
int num;
String a;
char *b;
uint64_t chipid;  
char c[20];
void setup() {
  // put your setup code here, to run once:
   Serial.begin(115200);
   chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
   chipid=(uint32_t)chipid;
   itoa(chipid, c, 10);  
  
   for(num=0;num<20;num++)
   { 
   pinMode(IO_Num[num],OUTPUT); 
   digitalWrite(IO_Num[num],1); 
   }
   //pinMode(33,OUTPUT);
   //pinMode(27,OUTPUT);//三个引脚设置为输出模式
    
    
    WiFi.mode(WIFI_AP);   
    a.concat(AP_SSID);
    a.concat(c);
    a.toCharArray(c,10);
    WiFi.softAP(c,"",1,0,4);
    Serial.println(a);\
}

void loop() {
   // put your main code here, to run repeatedly:
   //digitalWrite(32,rand()%2); //三个LED随机点亮，组成不同的颜色
   ///digitalWrite(33,rand()%2);
   //digitalWrite(27,rand()%2);
   digitalWrite(IO_Num[num],0);  
   delay(100);
   digitalWrite(IO_Num[num],1); 
   delay(100);
   num++;
   if(num>=20)
   num=0;
   
}
