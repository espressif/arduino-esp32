#include <WiFi.h> //Wifi library
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks
#define EAP_IDENTITY "login" //if connecting from another corporation, use identity@organisation.domain in Eduroam 
#define EAP_PASSWORD "password" //your Eduroam password
const char* ssid = "eduroam"; // Eduroam SSID
const char* host = "arduino.php5.sk"; //external server domain for HTTP connection after authentification
int counter = 0;
void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to network: ");
  Serial.println(ssid);
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA); //init wifi mode
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username --> identity and username is same
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT(); //set config settings to default
  esp_wifi_sta_wpa2_ent_enable(&config); //set config settings to enable function
  WiFi.begin(ssid); //connect to wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    if(counter>=60){ //after 30 seconds timeout - reset board
      ESP.restart();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: "); 
  Serial.println(WiFi.localIP()); //print LAN IP
}
void loop() {
  if (WiFi.status() == WL_CONNECTED) { //if we are connected to Eduroam network
    counter = 0; //reset counter
    Serial.println("Wifi is still connected with IP: "); 
    Serial.println(WiFi.localIP());   //inform user about his IP address
  }else if (WiFi.status() != WL_CONNECTED) { //if we lost connection, retry
    WiFi.begin(ssid);      
  }
  while (WiFi.status() != WL_CONNECTED) { //during lost connection, print dots
    delay(500);
    Serial.print(".");
    counter++;
    if(counter>=60){ //30 seconds timeout - reset board
    ESP.restart();
    }
  }
  Serial.print("Connecting to website: ");
  Serial.println(host);
  WiFiClient client;
  if (client.connect(host, 80)) {
    String url = "/rele/rele1.txt";
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "User-Agent: ESP32\r\n" + "Connection: close\r\n\r\n");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }
    String line = client.readStringUntil('\n');
   Serial.println(line);
  }else{
      Serial.println("Connection unsucessful");
    }  
}
