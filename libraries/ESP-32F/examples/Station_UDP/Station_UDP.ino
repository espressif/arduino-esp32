#include<WiFi.h>
#include<WiFiUDP.h>
#include<WiFiMulti.h>
WiFiMulti WiFiMulti;
WiFiUDP UDP; 
#define WIFISSID "yq"
#define Password "goouuu8266"
uint8_t test[6]={0x12,0x34,0x56,0x78,0x0c,0x0b};//6个字节的测试数据
uint8_t RxBuf[10];
const uint16_t port = 8266;//主机端口
const char *host = "39.106.163.29"; // ip or dns 远程主机ip
int Buf_Size=0;
void Connect_Router(void);//连接到你的路由器  
void Connect_Host(void);//连接到远程服务器


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Connect_Router();//连接到你的路由器  
  
  
  
}

void loop() {
  // put your main code here, to run repeatedly:

    Connect_Host();//连接到远程服务器
   
}


void Connect_Router(void)
{
    // We start by connecting to a WiFi network
    WiFiMulti.addAP(WIFISSID,Password);

    Serial.println();
    Serial.println();
    Serial.print("Wait for WiFi... ");

    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    delay(500);
}

void Connect_Host(void)
{    
     UDP.begin(WiFi.localIP(),port);
     UDP.beginPacket(host,port);
     UDP.write(test,6);
     
     
          Buf_Size=UDP.parsePacket();
          UDP.read(RxBuf,Buf_Size);
          Serial.write(RxBuf,Buf_Size);
      
     UDP.endPacket();
     delay(500);
}
 


 


