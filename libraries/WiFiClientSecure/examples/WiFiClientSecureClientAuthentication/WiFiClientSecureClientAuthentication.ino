/*
  Secure connection example for ESP32 <----> Mosquitto broker (used for MQTT) communcation
  with possible client authentication

  Prerequisite:
  PubSubClient library for Arduino - https://github.com/knolleary/pubsubclient/
  OpenSSL - https://www.openssl.org/
  Mosquitto broker - https://mosquitto.org/
  
  1. step - Generating the certificates
  For generating self-signed certificates please run the following commands:
      
  openssl req -new -x509 -days 365 -extensions v3_ca -keyout ca.key -out ca.crt -subj '/CN=TrustedCA.net'  #If you generating self-signed certificates the CN can be anything

  openssl genrsa -out mosquitto.key 2048
  openssl req -out mosquitto.csr -key mosquitto.key -new -subj '/CN=Mosquitto_borker_adress'    #Its necessary to set the CN to the adress of which the client calls your Mosquitto server (eg. yourserver.com)!!!
  openssl x509 -req -in mosquitto.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out mosquitto.crt -days 365 

  #This is only needed if the mosquitto broker requires a client autentithication (require_certificate is set to true in mosquitto config)
  openssl genrsa -out esp.key 2048
  openssl req -out esp.csr -key esp.key -new -subj '/CN=localhost'
  openssl x509 -req -in esp.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out esp.crt -days 365

  2. Open ca.crt, esp.crt and esp.key with text viewer and copy the values to this WiFiClientSecureClientAuthentication.ino source file into 
  corresponding const char CA_cert[], const char ESP_CA_cert[] and const char ESP_RSA_key[] with escape characters.

  (1-2.) Alternatively you can use the libraries/WiFiClientSecure/examples/WiFiClientSecureClientAuthentication/certificates/certificate_generator.sh script 
  for generating and formatting the certificates.

  3. step - Install and setup you Mosquitto broker
  Follow the instructions from https://mosquitto.org/ and check the manual for the coniguration.
  For the Mosquito broker you need ca.crt, mosquitto.key and mosquitto.crt files generated in previous step.
  Recommended to put they in /etc/mosquitto/ca_certificates/ and /etc/mosquitto/certs/
  You need to config Mosquitto broker to use these files (usually /etc/mosquitto/conf.d/default.conf):
  
  listener 8883
  cafile path/to/ca.crt
  keyfile path/to/mosquitto.key
  certfile path/to/mosquitto.crt
  require_certificate true or false #If you need client authentication set it to true
  log_type all  #for logging in /var/log/mosquitto/

  4.Restart the Mosquitto service or start the broker:
  sudo service mosquitto restart
  or
  mosquitto -c /etc/mosquitto/conf.d/default.conf
  
  2021 - Norbert Gal - Apache 2.0 License.
*/

#include <PubSubClient.h>
#include "WiFiClientSecure.h"

const char* CA_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"################################################################\n" \
"################################################################\n" \
"################################################################\n" \
"-----END CERTIFICATE-----";

const char* ESP_CA_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"################################################################\n" \
"################################################################\n" \
"################################################################\n" \
"-----END CERTIFICATE-----";

const char* ESP_RSA_key= \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"################################################################\n" \
"################################################################\n" \
"################################################################\n" \
"-----END RSA PRIVATE KEY-----";


const char* ssid     = "wifi";        // your network SSID (name of wifi network)
const char* password = "wifi_pass";   // your network password

const char* server = "Mosquitto_borker_adress";
const char* mqtt_user = "user";           //If it is configured, see Mosquitto config
const char* mqtt_pass = "user_password";  //If it is configured, see Mosquitto config

WiFiClientSecure client;
PubSubClient mqtt_client(client); 

void setup() {
  Serial.begin(115200);
  delay(100); 

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.print("Connected to ");
  Serial.println(ssid);

  client.setCACert(CA_cert);
  client.setCertificate(ESP_CA_cert); // for client verification if the require_certificate is set to true  in the mosquitto broker config
  client.setPrivateKey(ESP_RSA_key);  // for client verification if the require_certificate is set to true  in the mosquitto broker config
  mqtt_client.setServer(server, 8883);
  
}

void loop() {
  
  Serial.println("\nStarting connection to server...");
  //if you use password for Mosquitto broker
  //if (mqtt_client.connect("ESP32", mqtt_user , mqtt_pass)) {
  //if you dont use password for Mosquitto broker
  if (mqtt_client.connect("ESP32")) {                       
    Serial.print("Connected, mqtt_client state: ");
    Serial.println(mqtt_client.state());
    mqtt_client.publish("LIvingRoom/TEMPERATURE", "25");
  }
  else {
    Serial.println("Connected failed!  mqtt_client state:");
    Serial.print(mqtt_client.state());
    Serial.println("WiFiClientSecure client state:");
    char lastError[100];
    client.lastError(lastError,100);  //Get the last error for WiFiClientSecure
    Serial.print(lastError);
  }
  delay(10000);
}
