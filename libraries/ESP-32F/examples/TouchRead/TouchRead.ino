/*------------------------------------------------------------------------------------------------------------------------------
                              果云科技
                           ESP-32F 开发板

                             触摸按键 实验
------------------------------------------------------------------------------------------------------------------------------*/
// Just test touch pin - Touch0 is T0 which is on GPIO 2.
int T0_Cul=0;
int T2_Cul=0;
void LED_Init(void)//LED��ʼ��
{
   pinMode(33,OUTPUT);
   pinMode(27,OUTPUT);  
}

void setup()
{
  LED_Init();
  Serial.begin(115200);
  Serial.println("ESP32 Touch Test");
}



void Touch_Pad_Check(void)
{
  Serial.print("T0_Value:");  
  Serial.println(touchRead(T0));  // get value using T0
  T0_Cul=touchRead(T0);
  Serial.print("T2_Value:");  // get value using T2
  Serial.println(touchRead(T2));  // 
  T2_Cul=touchRead(T2);

    if(T0_Cul<=15)//通道采集数值低于15 被认为按键按下
    {
      digitalWrite(33,0);//点亮LEDA
    }
    else if(T0_Cul>20)//通道采集数值高于20 被认为按键被释放
    {
     digitalWrite(33,1);//熄灭LEDA
    }

     if(T2_Cul<=15)
   {
   digitalWrite(27,0);
   }
   else if(T2_Cul>20)
   {
    digitalWrite(27,1);
   }
}

void loop()
{
  Touch_Pad_Check();
  
}
