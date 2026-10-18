// SPDX-License-Identifier: Apache-2.0
// Execute the actual NetworkManager::hostByName method with protected-DNS
// and esp_netif dispatch doubles. This is not a hardware/lwIP runtime test.
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
constexpr int ERR_OK=0,ESP_OK=0,AF_UNSPEC=0,AF_INET6=10,SOCK_STREAM=1,IPv6=6,ESP_NETIF_ID_MAX=2;
using err_t=int;using esp_err_t=int;using Network_Interface_ID=int;
struct IPAddress {
  uint32_t value=0;IPAddress()=default;IPAddress(uint32_t v):value(v){}IPAddress(int,const uint8_t*){}
  IPAddress&operator=(uint32_t v){value=v;return *this;}
  bool fromString(const char *s){if(std::string(s)=="192.0.2.1"){value=1;return true;}return false;}
  std::string toString()const{return "192.0.2.1";}
  IPAddress&operator=(const struct in_addr&);
};
struct NetworkInterface {bool ipv4=false,ipv6=false;bool hasIP(){return ipv4;}bool hasGlobalIPv6(){return ipv6;}};
NetworkInterface testInterface;
NetworkInterface *getNetifByID(Network_Interface_ID id){return id?nullptr:&testInterface;}
struct in_addr{uint32_t s_addr=1;};struct in6_addr{uint8_t s6_addr[16]{};};
struct sockaddr_in{in_addr sin_addr;};struct sockaddr_in6{in6_addr sin6_addr;};
struct addrinfo{int ai_family,ai_socktype;void *ai_addr;};
struct NetworkManager {int hostByName(const char*,IPAddress&);};
bool tcpipContext=false,nested=false,failDispatch=false;
unsigned dispatches=0,clears=0,udpCallbacks=0,resolutions=0,nestedCalls=0,clearLogs=0,errorLogs=0;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
esp_err_t esp_netif_tcpip_exec(esp_err_t(*callback)(void*),void *ctx){
  // IDF5.5.5 esp_netif_lwip_ipc_call_msg: a current core owner executes
  // directly; an ordinary caller uses synchronous dispatch/core protection.
  if(tcpipContext)return callback(ctx);
  ++dispatches;if(failDispatch){failDispatch=false;return 0x101;}
  tcpipContext=true;const auto result=callback(ctx);tcpipContext=false;return result;
}
void dns_clear_cache(){
  check(tcpipContext,"dns_clear_cache reached a pending UDP callback outside TCP/IP context");
  ++clears;++udpCallbacks; // pending SNTP DNS callback may allocate a UDP PCB.
  if(nested){nested=false;esp_netif_tcpip_exec([](void*){
    check(tcpipContext,"nested callback lost ownership");++nestedCalls;
    NetworkManager network;IPAddress result;
    check(network.hostByName("nested.invalid",result)==1,"nested lookup failed");return ESP_OK;
  },nullptr);}
}
int lwip_getaddrinfo(const char*,const char*,const addrinfo *hints,addrinfo **result){
  ++resolutions;static addrinfo answer;static sockaddr_in v4;static sockaddr_in6 v6;
  answer.ai_family=hints->ai_family==AF_INET6?AF_INET6:AF_UNSPEC;
  answer.ai_addr=answer.ai_family==AF_INET6?static_cast<void*>(&v6):static_cast<void*>(&v4);*result=&answer;return ERR_OK;
}
void lwip_freeaddrinfo(addrinfo*){}
void debugLog(const char *text,...){if(std::string(text)=="Clearing DNS cache")++clearLogs;}
void errorLog(const char*,...){++errorLogs;}
#define log_d(...) debugLog(__VA_ARGS__)
#define log_e(...) errorLog(__VA_ARGS__)
// ACTUAL_IMPLEMENTATION
int main(){try{
  NetworkManager network;IPAddress result;
  check(network.hostByName("192.0.2.1",result)==1&&!clears&&!resolutions,"literal address unnecessarily touched DNS");
  testInterface.ipv4=true;nested=true;
  check(network.hostByName("fixture.invalid",result)==1,"IPv4 lookup result changed");
  check(clears==1&&dispatches==1&&udpCallbacks==1&&nestedCalls==1&&!tcpipContext,"clear/nested callback owner not preserved");
  check(network.hostByName("fixture.invalid",result)==1&&clears==1,"unchanged address state unnecessarily cleared cache");
  testInterface.ipv6=true;
  check(network.hostByName("fixture.invalid",result)==1&&clears==2&&dispatches==2,"IPv6 address transition was not protected");
  testInterface.ipv4=testInterface.ipv6=false;
  check(network.hostByName("fixture.invalid",result)==1&&clears==3&&dispatches==3,"link-loss cache transition was not protected");
  check(udpCallbacks==clears&&!tcpipContext,"cache callback leaked ownership");
  const auto beforeLookups=resolutions;testInterface.ipv4=true;failDispatch=true;
  check(network.hostByName("fixture.invalid",result)==0,"failed dispatch reported successful lookup");
  check(clears==3&&clearLogs==3&&errorLogs==1&&resolutions==beforeLookups,"failed dispatch falsely logged clear or continued DNS");
  check(network.hostByName("fixture.invalid",result)==1&&clears==4&&clearLogs==4&&dispatches==5,"failed dispatch poisoned refresh tracking; same-state retry skipped clear");
  std::cout<<"Actual hostByName: literal/no-change/IPv4/IPv6/link-loss, pending/nested context and failed-dispatch tracking restoration pass\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
