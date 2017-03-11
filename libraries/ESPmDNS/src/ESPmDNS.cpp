/*

ESP8266 Multicast DNS (port of CC3000 Multicast DNS library)
Version 1.1
Copyright (c) 2013 Tony DiCola (tony@tonydicola.com)
ESP8266 port (c) 2015 Ivan Grokhotkov (ivan@esp8266.com)
MDNS-SD Suport 2015 Hristo Gochkov (hristo@espressif.com)
Extended MDNS-SD support 2016 Lars Englund (lars.englund@gmail.com)


License (MIT license):
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

 */

// Important RFC's for reference:
// - DNS request and response: http://www.ietf.org/rfc/rfc1035.txt
// - Multicast DNS: http://www.ietf.org/rfc/rfc6762.txt
// - MDNS-SD: https://tools.ietf.org/html/rfc6763

#ifndef LWIP_OPEN_SRC
#define LWIP_OPEN_SRC
#endif

#include "ESPmDNS.h"
#include <functional>
#include "esp_wifi.h"

MDNSResponder::MDNSResponder() : mdns(NULL), _if(TCPIP_ADAPTER_IF_STA) {}
MDNSResponder::~MDNSResponder() {
    end();
}

bool MDNSResponder::begin(const char* hostName, tcpip_adapter_if_t tcpip_if, uint32_t ttl){
    _if = tcpip_if;
    if(!mdns && mdns_init(_if, &mdns)){
        log_e("Failed starting MDNS");
        return false;
    }
    _hostname = hostName;
    if(mdns_set_hostname(mdns, hostName)) {
        log_e("Failed setting MDNS hostname");
        return false;
    }
    return true;
}

void MDNSResponder::end() {
    if(!mdns){
        return;
    }
    mdns_free(mdns);
    mdns = NULL;
}

void MDNSResponder::setInstanceName(String name) {
    if (name.length() > 63) return;
    if(mdns_set_instance(mdns, name.c_str())){
        log_e("Failed setting MDNS instance");
        return;
    }
}

void MDNSResponder::enableArduino(uint16_t port, bool auth){
    const char * arduTxtData[4] = {
            "board=" ARDUINO_BOARD,
            "tcp_check=no",
            "ssh_upload=no",
            "auth_upload=no"
    };
    if(auth){
        arduTxtData[3] = "auth_upload=yes";
    }

    if(mdns_service_add(mdns, "_arduino", "_tcp", port)) {
        log_e("Failed adding Arduino service");
    } else if(mdns_service_txt_set(mdns, "_arduino", "_tcp", 4, arduTxtData)) {
        log_e("Failed setting Arduino service TXT");
    }
}

void MDNSResponder::disableArduino(){
    if(mdns_service_remove(mdns, "_arduino", "_tcp")) {
        log_w("Failed removing Arduino service");
    }
}

void MDNSResponder::enableWorkstation(){
    char winstance[21+_hostname.length()];
    uint8_t mac[6];
    esp_wifi_get_mac((wifi_interface_t)_if, mac);
    sprintf(winstance, "%s [%02x:%02x:%02x:%02x:%02x:%02x]", _hostname.c_str(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if(mdns_service_add(mdns, "_workstation", "_tcp", 9)) {
        log_e("Failed adding Workstation service");
    } else if(mdns_service_instance_set(mdns, "_workstation", "_tcp", winstance)) {
        log_e("Failed setting Workstation service instance name");
    }
}

void MDNSResponder::disableWorkstation(){
    if(mdns_service_remove(mdns, "_workstation", "_tcp")) {
        log_w("Failed removing Workstation service");
    }
}

void MDNSResponder::addService(char *name, char *proto, uint16_t port){
    if(mdns_service_add(mdns, name, proto, port)) {
        log_e("Failed adding service %s.%s.\n", name, proto);
    }
}

bool MDNSResponder::addServiceTxt(char *name, char *proto, char *key, char *value){
    //ToDo: implement it in IDF. This will set the TXT to one record currently
    String txt = String(key) + "=" + String(value);
    const char * txt_chr[1] = {txt.c_str()};
    if(mdns_service_txt_set(mdns, name, proto, 1, txt_chr)) {
        log_e("Failed setting service TXT");
        return false;
    }
    return true;
}

int MDNSResponder::queryService(char *service, char *proto) {
    mdns_result_free(mdns);
    if(proto){
        char srv[strlen(service)+2];
        char prt[strlen(proto)+2];
        sprintf(srv, "_%s", service);
        sprintf(prt, "_%s", proto);
        return mdns_query(mdns, srv, prt, 2000);
    }
    return mdns_query(mdns, service, NULL, 2000);
}

IPAddress MDNSResponder::queryHost(char *host){
    mdns_result_free(mdns);
    if(!mdns_query(mdns, host, NULL, 2000)){
        return IPAddress();
    }
    return IP(0);
}

String MDNSResponder::hostname(int idx) {
    const mdns_result_t * result = mdns_result_get(mdns, idx);
    if(!result){
        log_e("Result %d not found", idx);
        return String();
    }
    return String(result->host);
}

IPAddress MDNSResponder::IP(int idx) {
    const mdns_result_t * result = mdns_result_get(mdns, idx);
    if(!result){
        log_e("Result %d not found", idx);
        return IPAddress();
    }
    return IPAddress(result->addr.addr);
}

uint16_t MDNSResponder::port(int idx) {
    const mdns_result_t * result = mdns_result_get(mdns, idx);
    if(!result){
        log_e("Result %d not found", idx);
        return 0;
    }
    return result->port;
}

MDNSResponder MDNS;
