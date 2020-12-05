/*--------------------------------
          果云科技
       ESP-32F 开发板

      DHT11 传感器 实验
---------------------------------*/
//String temp;
#include "user_dht.h" 
uint8 value[5];
DHT11 dht11(21); 
void setup() {
  // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("Hello Goouuu");
     
  
   
}

void loop() {
  // put your main code here, to run repeatedly:
    //dht11.Read_Value(value);
    //Serial.write(value,4);
    dht11.Get_DHT11_Value();
    delay(1000);
}
