/**********************************************************
      Make in goouuu.com
	  Maker:MDC
***********************************************************/
#ifndef USER_DHT11_H
#define USER_DHT11_H
#define uint8  unsigned char


class DHT11
{
  private:
          uint8  pin;   
  public:  //公共方法
     DHT11(uint8  p);//构造函数
     ~DHT11();//析构函数         
     void PortIN();//DHT11 引脚设置为输入模式
     void PortOUT();//DHT11 引脚设置为输出模式
     uint8  Start();//开始读取数据
     uint8  ReadByte();//读取一个字节的数据
     uint8  Read_Value(uint8  *dht);//读取5个字节，读取一帧温湿度数据
     void NumToString(uint8  dht,uint8  *str);
     void Get_DHT11_Value();//获取一帧数据并且打印
};
#define  DHT11_Pin_In   digitalRead(pin)
#define  DHT11_Pin_Low  digitalWrite(pin,0)
#define  DHT11_Pin_Hig  digitalWrite(pin,1)
 

#endif
