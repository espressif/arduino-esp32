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
#include "WiFi.h"
#include <functional>
#include "esp_wifi.h"

// Add quotes around defined value
#ifdef __IN_ECLIPSE__
#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)
#else
#define STR(tok) tok
#endif

// static void _on_sys_event(arduino_event_t *event){
//     mdns_handle_system_event(NULL, event);
// }

MDNSResponder::MDNSResponder() :results(NULL) {}
MDNSResponder::~MDNSResponder() {
    end();
}

bool MDNSResponder::begin(const String& hostName){
    if(mdns_init()){
        log_e("Failed starting MDNS");
        return false;
    }
    //WiFi.onEvent(_on_sys_event);
    _hostname = hostName;
	_hostname.toLowerCase();
    if(mdns_hostname_set(hostName.c_str())) {
        log_e("Failed setting MDNS hostname");
        return false;
    }
    return true;
}

void MDNSResponder::end() {
    mdns_free();
}

void MDNSResponder::setInstanceName(String name) {
    if (name.length() > 63) return;
    if(mdns_instance_name_set(name.c_str())){
        log_e("Failed setting MDNS instance");
        return;
    }
}


void MDNSResponder::enableArduino(uint16_t port, bool auth){
    mdns_txt_item_t arduTxtData[4] = {
        {(char*)"board"         ,(char*)STR(ARDUINO_VARIANT)},
        {(char*)"tcp_check"     ,(char*)"no"},
        {(char*)"ssh_upload"    ,(char*)"no"},
        {(char*)"auth_upload"   ,(char*)"no"}
    };

    if(mdns_service_add(NULL, "_arduino", "_tcp", port, arduTxtData, 4)) {
        log_e("Failed adding Arduino service");
    }

    if(auth && mdns_service_txt_item_set("_arduino", "_tcp", "auth_upload", "yes")){
        log_e("Failed setting Arduino txt item");
    }
}

void MDNSResponder::disableArduino(){
    if(mdns_service_remove("_arduino", "_tcp")) {
        log_w("Failed removing Arduino service");
    }
}

void MDNSResponder::enableWorkstation(esp_interface_t interface){
    char winstance[21+_hostname.length()];
    uint8_t mac[6];
    esp_wifi_get_mac((wifi_interface_t)interface, mac);
    sprintf(winstance, "%s [%02x:%02x:%02x:%02x:%02x:%02x]", _hostname.c_str(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if(mdns_service_add(NULL, "_workstation", "_tcp", 9, NULL, 0)) {
        log_e("Failed adding Workstation service");
    } else if(mdns_service_instance_name_set("_workstation", "_tcp", winstance)) {
        log_e("Failed setting Workstation service instance name");
    }
}

void MDNSResponder::disableWorkstation(){
    if(mdns_service_remove("_workstation", "_tcp")) {
        log_w("Failed removing Workstation service");
    }
}

bool MDNSResponder::addService(char *name, char *proto, uint16_t port){
    char _name[strlen(name)+2];
    char _proto[strlen(proto)+2];
	if (name[0] == '_') {
		sprintf(_name, "%s", name);
	} else {
		sprintf(_name, "_%s", name);
	}
	if (proto[0] == '_') {
		sprintf(_proto, "%s", proto);
	} else {
		sprintf(_proto, "_%s", proto);
	}

    if(mdns_service_add(NULL, _name, _proto, port, NULL, 0)) {
        log_e("Failed adding service %s.%s.\n", name, proto);
	return false;
    }
    return true;
}

bool MDNSResponder::addServiceTxt(char *name, char *proto, char *key, char *value){
    char _name[strlen(name)+2];
    char _proto[strlen(proto)+2];
	if (name[0] == '_') {
		sprintf(_name, "%s", name);
	} else {
		sprintf(_name, "_%s", name);
	}
	if (proto[0] == '_') {
		sprintf(_proto, "%s", proto);
	} else {
		sprintf(_proto, "_%s", proto);
	}

    if(mdns_service_txt_item_set(_name, _proto, key, value)) {
        log_e("Failed setting service TXT");
        return false;
    }
    return true;
}

IPAddress MDNSResponder::queryHost(char *host, uint32_t timeout){
    esp_ip4_addr_t addr;
    addr.addr = 0;

    esp_err_t err = mdns_query_a(host, timeout,  &addr);
    if(err){
        if(err == ESP_ERR_NOT_FOUND){
            log_w("Host was not found!");
            return IPAddress();
        }
        log_e("Query Failed");
        return IPAddress();
    }
    return IPAddress(addr.addr);
}


int MDNSResponder::queryService(char *service, char *proto) {
    if(!service || !service[0] || !proto || !proto[0]){
        log_e("Bad Parameters");
        return 0;
    }

    if(results){
        mdns_query_results_free(results);
        results = NULL;
    }

    char srv[strlen(service)+2];
    char prt[strlen(proto)+2];
	if (service[0] == '_') {
		sprintf(srv, "%s", service);
	} else {
		sprintf(srv, "_%s", service);
	}
	if (proto[0] == '_') {
		sprintf(prt, "%s", proto);
	} else {
		sprintf(prt, "_%s", proto);
	}

    esp_err_t err = mdns_query_ptr(srv, prt, 3000, 20,  &results);
    if(err){
        log_e("Query Failed");
        return 0;
    }
    if(!results){
        log_w("No results found!");
        return 0;
    }

    mdns_result_t * r = results;
    int i = 0;
    while(r){
        i++;
        r = r->next;
    }
    return i;
}

mdns_result_t * MDNSResponder::_getResult(int idx){
    mdns_result_t * result = results;
    int i = 0;
    while(result){
        if(i == idx){
            break;
        }
        i++;
        result = result->next;
    }
    return result;
}

mdns_txt_item_t * MDNSResponder::_getResultTxt(int idx, int txtIdx){
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return NULL;
    }
    if (txtIdx >= result->txt_count) return NULL;
    return &result->txt[txtIdx];
}

String MDNSResponder::hostname(int idx) {
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return String();
    }
    return String(result->hostname);
}

IPAddress MDNSResponder::IP(int idx) {
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return IPAddress();
    }
    mdns_ip_addr_t * addr = result->addr;
    while(addr){
        if(addr->addr.type == MDNS_IP_PROTOCOL_V4){
            return IPAddress(addr->addr.u_addr.ip4.addr);
        }
        addr = addr->next;
    }
    return IPAddress();
}

IPv6Address MDNSResponder::IPv6(int idx) {
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return IPv6Address();
    }
    mdns_ip_addr_t * addr = result->addr;
    while(addr){
        if(addr->addr.type == MDNS_IP_PROTOCOL_V6){
            return IPv6Address(addr->addr.u_addr.ip6.addr);
        }
        addr = addr->next;
    }
    return IPv6Address();
}

uint16_t MDNSResponder::port(int idx) {
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return 0;
    }
    return result->port;
}

int MDNSResponder::numTxt(int idx) {
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return 0;
    }
    return result->txt_count;
}

bool MDNSResponder::hasTxt(int idx, const char * key) {
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return false;
    }
    int i = 0;
    while(i < result->txt_count) {
        if (strcmp(result->txt[i].key, key) == 0) return true;
        i++;
    }
    return false;
}

String MDNSResponder::txt(int idx, const char * key) {
    mdns_result_t * result = _getResult(idx);
    if(!result){
        log_e("Result %d not found", idx);
        return "";
    }
    int i = 0;
    while(i < result->txt_count) {
        if (strcmp(result->txt[i].key, key) == 0) return result->txt[i].value;
        i++;
    }
    return "";
}

String MDNSResponder::txt(int idx, int txtIdx) {
    mdns_txt_item_t * resultTxt = _getResultTxt(idx, txtIdx);
    return !resultTxt
        ? ""
        : resultTxt->value;
}

String MDNSResponder::txtKey(int idx, int txtIdx) {
    mdns_txt_item_t * resultTxt = _getResultTxt(idx, txtIdx);
    return !resultTxt
        ? ""
        : resultTxt->key;
}

MDNSResponder MDNS;
