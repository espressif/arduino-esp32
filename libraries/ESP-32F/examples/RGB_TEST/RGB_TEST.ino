/*--------------------------------
          果云科技
       ESP-32F 开发板

          RGB 实验
---------------------------------*/
#include<stdio.h>
#include<stdlib.h>
void setup() {
  // put your setup code here, to run once:
    pinMode(32,OUTPUT); 
    pinMode(33,OUTPUT);
    pinMode(27,OUTPUT);//三个引脚设置为输出模式
}

void loop() {
  // put your main code here, to run repeatedly:
   digitalWrite(32,rand()%2); //三个LED随机点亮，组成不同的颜色
   digitalWrite(33,rand()%2);
   digitalWrite(27,rand()%2);
   delay(1000);
   
}
