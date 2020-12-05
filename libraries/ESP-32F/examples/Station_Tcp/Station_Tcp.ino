#include<WiFi.h>
#include<WiFiMulti.h>
WiFiMulti WiFiMulti;
WiFiClient client;
#define WIFISSID "yq"
#define Password "goouuu8266"
uint8_t test[6]={0x12,0x34,0x56,0x78,0x0c,0x0b};//6个字节的测试数据
const uint16_t port = 8266;//主机端口
const char * host = "39.106.163.29"; // ip or dns 远程主机ip

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
    // This will send the request to the server
    client.write(test,6);//发送6个字节数据
    client.print("Hello_Goouuu!\r\n");//发送字符串
    //read back one line from server
    String line = client.readStringUntil('\r');读取服务器发来的数据
    Serial.println(line);//打印服务器发来的数据
   
    client.stop();//关闭端口
    Serial.println("closing connection");
    Serial.println("wait 5 sec...");
    delay(5000);
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
    if (!client.connect(host, port)) {
        Serial.println("connection failed");
        Serial.println("wait 5 sec...");
        delay(5000);
        return;
    }
    
}
 


 


