#include "user_dht.h"
#include "Arduino.h"

DHT11 :: DHT11(uint8  p)
{
    pin=p;
}
DHT11::~DHT11()
{
    
} 
void DHT11:: PortIN()
{
   pinMode(pin,INPUT);
}

void DHT11:: PortOUT()
{
   pinMode(pin,OUTPUT);
}

uint8  DHT11:: Start()
{
   uint8  result = 0;
   PortOUT();
   DHT11_Pin_Low;
   delay(20);
   DHT11_Pin_Hig;
   delayMicroseconds(45);
   PortIN();
   delayMicroseconds(5);
   while(DHT11_Pin_In==0)
   {
   result++;
   delayMicroseconds(10);
   }
   while(DHT11_Pin_In&&result);
   if(result>3)
   return true;
   else
     return false;
}

uint8  DHT11::ReadByte()
{
  uint8  reader;
  uint8  bitsum;
  reader = 0; 
  PortIN();
  for(bitsum=0;bitsum<8;bitsum++)
  {
    reader = reader << 1;
    while(DHT11_Pin_In==0);
    delayMicroseconds(50);
    if(DHT11_Pin_In)
       reader |= 0x01;
    while(DHT11_Pin_In);
  } 
  return reader;

  
}
uint8  DHT11::Read_Value(uint8  *dht)
{
  uint8  sum; 
  uint8  checksum=0;
  if(Start())
  {   
    dht[0]   = ReadByte();
    dht[1]   = ReadByte();
    dht[2]   = ReadByte();
    dht[3]   = ReadByte();
    checksum = ReadByte();
    sum = (dht[0]+dht[1]+dht[2]+dht[3]);
    if(checksum==sum)
      return true;
    else
      return false;
  };
  return false;
}
void DHT11::NumToString(uint8  dht,uint8  *str)
{
  str[0] = (dht%100)/10 + '0';
  str[1] = dht%10 + '0';
  str[2] = '\0';
}

void DHT11::Get_DHT11_Value()  //DHT11 数据获取
{
      uint8 dhtData[5];
      uint8 outstr[10];
     if(Read_Value(dhtData))
    { Serial.print("temperature: "); 
      NumToString(dhtData[2],outstr);Serial.write(outstr,3);
      NumToString(dhtData[3],outstr);Serial.write(outstr,3);
      delay(10);
      Serial.print("\r\n");
      Serial.print("humidity: "); 
      NumToString(dhtData[0],outstr);Serial.write(outstr,3);
      NumToString(dhtData[1],outstr);Serial.write(outstr,3);
      delay(10);
      Serial.print("\r\n"); 
      //Serial.write(dhtData,4);
      
    

    } 

}


