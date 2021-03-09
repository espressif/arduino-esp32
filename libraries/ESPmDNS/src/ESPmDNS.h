// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ESP32MDNS_H
#define ESP32MDNS_H

#include "Arduino.h"
#include "IPv6Address.h"
#include "mdns.h"

//this should be defined at build time
#ifndef ARDUINO_VARIANT
#define ARDUINO_VARIANT "esp32"
#endif

class MDNSResponder {
public:
  MDNSResponder();
  ~MDNSResponder();
  bool begin(const char* hostName);
  void end();

  void setInstanceName(String name);
  void setInstanceName(const char * name){
    setInstanceName(String(name));
  }
  void setInstanceName(char * name){
    setInstanceName(String(name));
  }

  bool addService(char *service, char *proto, uint16_t port);
  bool addService(const char *service, const char *proto, uint16_t port){
    return addService((char *)service, (char *)proto, port);
  }
  bool addService(String service, String proto, uint16_t port){
    return addService(service.c_str(), proto.c_str(), port);
  }
  
  bool addServiceTxt(char *name, char *proto, char * key, char * value);
  void addServiceTxt(const char *name, const char *proto, const char *key,const char * value){
    addServiceTxt((char *)name, (char *)proto, (char *)key, (char *)value);
  }
  void addServiceTxt(String name, String proto, String key, String value){
    addServiceTxt(name.c_str(), proto.c_str(), key.c_str(), value.c_str());
  }

  void enableArduino(uint16_t port=3232, bool auth=false);
  void disableArduino();

  void enableWorkstation(wifi_interface_t interface=WIFI_IF_STA);
  void disableWorkstation();

  IPAddress queryHost(char *host, uint32_t timeout=2000);
  IPAddress queryHost(const char *host, uint32_t timeout=2000){
    return queryHost((char *)host, timeout);
  }
  IPAddress queryHost(String host, uint32_t timeout=2000){
    return queryHost(host.c_str(), timeout);
  }
  
  int queryService(char *service, char *proto);
  int queryService(const char *service, const char *proto){
    return queryService((char *)service, (char *)proto);
  }
  int queryService(String service, String proto){
    return queryService(service.c_str(), proto.c_str());
  }

  String hostname(int idx);
  IPAddress IP(int idx);
  IPv6Address IPv6(int idx);
  uint16_t port(int idx);
  int numTxt(int idx);
  bool hasTxt(int idx, const char * key);
  String txt(int idx, const char * key);
  String txt(int idx, int txtIdx);
  
private:
  String _hostname;
  mdns_result_t * results;
  mdns_result_t * _getResult(int idx);
};

extern MDNSResponder MDNS;

#endif //ESP32MDNS_H
