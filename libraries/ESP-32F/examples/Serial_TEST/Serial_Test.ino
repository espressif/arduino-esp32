/*------------------------------------------------------------------------------------------------------------------------------
                              果云科技
                           ESP-32F 开发板

                             Serial 实验
------------------------------------------------------------------------------------------------------------------------------*/
String buf;
char temp;
void setup() {
  // put your setup code here, to run once:
   Serial.begin(115200);
   Serial.setTimeout(10);//设置串口接收超时时间10ms （如果不明白可以改成1000 ，用串口助手看效果）
   Serial.println("Goouuu HelloWorld!");
   
}

void loop() {
  // put your main code here, to run repeatedly:
    if(Serial.available()!=0)//如果接收缓冲区非空 
    {
     buf=Serial.readString();//将收到的数据以字符串形式存到buf
     Serial.print(buf);//串口打印buf
        
    }
  
}
