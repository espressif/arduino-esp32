#include "ESP_Network_Events.h"
#include "ESP_Network_Manager.h"
#include "esp_task.h"
#include "esp32-hal.h"

typedef struct NetworkEventCbList {
    static network_event_handle_t current_id;
    network_event_handle_t id;
    NetworkEventCb cb;
    NetworkEventFuncCb fcb;
    NetworkEventSysCb scb;
    arduino_event_id_t event;

    NetworkEventCbList() : id(current_id++), cb(NULL), fcb(NULL), scb(NULL), event(ARDUINO_EVENT_NONE) {}
} NetworkEventCbList_t;
network_event_handle_t NetworkEventCbList::current_id = 1;

// arduino dont like std::vectors move static here
static std::vector<NetworkEventCbList_t> cbEventList;

static void _network_event_task(void * arg){
    for (;;) {
        ((ESP_Network_Events*)arg)->checkForEvent();
    }
    vTaskDelete(NULL);
}

ESP_Network_Events::ESP_Network_Events()
	: _arduino_event_group(NULL)
	, _arduino_event_group_h(NULL)
	, _arduino_event_queue(NULL)
	, _arduino_event_task_handle(NULL)
{}

ESP_Network_Events::~ESP_Network_Events(){
	if(_arduino_event_task_handle != NULL){
		vTaskDelete(_arduino_event_task_handle);
		_arduino_event_task_handle = NULL;
	}
	if(_arduino_event_group != NULL){
		vEventGroupDelete(_arduino_event_group);
		_arduino_event_group = NULL;
	}
	if(_arduino_event_group_h != NULL){
		vEventGroupDelete(_arduino_event_group_h);
		_arduino_event_group_h = NULL;
	}
	if(_arduino_event_queue != NULL){
		arduino_event_t *event = NULL;
		while(xQueueReceive(_arduino_event_queue, &event, 0) == pdTRUE){
			free(event);
		}
		vQueueDelete(_arduino_event_queue);
		_arduino_event_queue = NULL;
	}
}

bool ESP_Network_Events::initNetworkEvents(){
    if(!_arduino_event_group){
        _arduino_event_group = xEventGroupCreate();
        if(!_arduino_event_group){
            log_e("Network Event Group Create Failed!");
            return false;
        }
        xEventGroupSetBits(_arduino_event_group, WIFI_DNS_IDLE_BIT);
    }
    if(!_arduino_event_group_h){
        _arduino_event_group_h = xEventGroupCreate();
        if(!_arduino_event_group_h){
            log_e("Network Event Group 2 Create Failed!");
            return false;
        }
    }

    if(!_arduino_event_queue){
    	_arduino_event_queue = xQueueCreate(32, sizeof(arduino_event_t*));
        if(!_arduino_event_queue){
            log_e("Network Event Queue Create Failed!");
            return false;
        }
    }

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    	log_e("esp_event_loop_create_default failed!");
        return err;
    }

    if(!_arduino_event_task_handle){
        xTaskCreateUniversal(_network_event_task, "arduino_events", 4096, this, ESP_TASKD_EVENT_PRIO - 1, &_arduino_event_task_handle, ARDUINO_EVENT_RUNNING_CORE);
        if(!_arduino_event_task_handle){
            log_e("Network Event Task Start Failed!");
            return false;
        }
    }

    return true;
}

bool ESP_Network_Events::postEvent(arduino_event_t *data)
{
	if(data == NULL || _arduino_event_queue == NULL){
        return false;
	}
	arduino_event_t * event = (arduino_event_t*)malloc(sizeof(arduino_event_t));
	if(event == NULL){
        log_e("Arduino Event Malloc Failed!");
        return false;
	}
	memcpy(event, data, sizeof(arduino_event_t));
    if (xQueueSend(_arduino_event_queue, &event, portMAX_DELAY) != pdPASS) {
        log_e("Arduino Event Send Failed!");
        return false;
    }
    return true;
}

void ESP_Network_Events::checkForEvent()
{
	arduino_event_t *event = NULL;
	if(_arduino_event_queue == NULL){
		return;
	}
	if(xQueueReceive(_arduino_event_queue, &event, portMAX_DELAY) != pdTRUE){
		return;
	}
    if(event == NULL){
    	return;
    }
    log_v("Network Event: %d - %s", event->event_id, eventName(event->event_id));
    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        NetworkEventCbList_t entry = cbEventList[i];
        if(entry.cb || entry.fcb || entry.scb) {
            if(entry.event == (arduino_event_id_t) event->event_id || entry.event == ARDUINO_EVENT_MAX) {
                if(entry.cb) {
                    entry.cb((arduino_event_id_t) event->event_id);
                } else if(entry.fcb) {
                    entry.fcb((arduino_event_id_t) event->event_id, (arduino_event_info_t) event->event_info);
                } else {
                    entry.scb(event);
                }
            }
        }
    }
    free(event);
}

network_event_handle_t ESP_Network_Events::onEvent(NetworkEventCb cbEvent, arduino_event_id_t event)
{
    if(!cbEvent) {
        return 0;
    }
    NetworkEventCbList_t newEventHandler;
    newEventHandler.cb = cbEvent;
    newEventHandler.fcb = NULL;
    newEventHandler.scb = NULL;
    newEventHandler.event = event;
    cbEventList.push_back(newEventHandler);
    return newEventHandler.id;
}

network_event_handle_t ESP_Network_Events::onEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event)
{
    if(!cbEvent) {
        return 0;
    }
    NetworkEventCbList_t newEventHandler;
    newEventHandler.cb = NULL;
    newEventHandler.fcb = cbEvent;
    newEventHandler.scb = NULL;
    newEventHandler.event = event;
    cbEventList.push_back(newEventHandler);
    return newEventHandler.id;
}

network_event_handle_t ESP_Network_Events::onEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event)
{
    if(!cbEvent) {
        return 0;
    }
    NetworkEventCbList_t newEventHandler;
    newEventHandler.cb = NULL;
    newEventHandler.fcb = NULL;
    newEventHandler.scb = cbEvent;
    newEventHandler.event = event;
    cbEventList.push_back(newEventHandler);
    return newEventHandler.id;
}

void ESP_Network_Events::removeEvent(NetworkEventCb cbEvent, arduino_event_id_t event)
{
    if(!cbEvent) {
        return;
    }

    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        NetworkEventCbList_t entry = cbEventList[i];
        if(entry.cb == cbEvent && entry.event == event) {
            cbEventList.erase(cbEventList.begin() + i);
        }
    }
}

void ESP_Network_Events::removeEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event)
{
    if(!cbEvent) {
        return;
    }

    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        NetworkEventCbList_t entry = cbEventList[i];
        if(entry.scb == cbEvent && entry.event == event) {
            cbEventList.erase(cbEventList.begin() + i);
        }
    }
}

void ESP_Network_Events::removeEvent(network_event_handle_t id)
{
    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        NetworkEventCbList_t entry = cbEventList[i];
        if(entry.id == id) {
            cbEventList.erase(cbEventList.begin() + i);
        }
    }
}

int ESP_Network_Events::setStatusBits(int bits){
    if(!_arduino_event_group){
        return 0;
    }
    int l=bits & 0x00FFFFFF, h=(bits & 0xFF000000) >> 24;
    if(l){
    	l = xEventGroupSetBits(_arduino_event_group, l);
    }
    if(h){
    	h = xEventGroupSetBits(_arduino_event_group_h, h);
    }
    return l | (h << 24);
}

int ESP_Network_Events::clearStatusBits(int bits){
    if(!_arduino_event_group){
        return 0;
    }
    int l=bits & 0x00FFFFFF, h=(bits & 0xFF000000) >> 24;
    if(l){
    	l = xEventGroupClearBits(_arduino_event_group, l);
    }
    if(h){
    	h = xEventGroupClearBits(_arduino_event_group_h, h);
    }
    return l | (h << 24);
}

int ESP_Network_Events::getStatusBits(){
    if(!_arduino_event_group){
        return 0;
    }
    return xEventGroupGetBits(_arduino_event_group) | (xEventGroupGetBits(_arduino_event_group_h) << 24);
}

int ESP_Network_Events::waitStatusBits(int bits, uint32_t timeout_ms){
    if(!_arduino_event_group){
        return 0;
    }
    int l=bits & 0x00FFFFFF, h=(bits & 0xFF000000) >> 24;
    if(l){
	    l = xEventGroupWaitBits(
	        _arduino_event_group,    // The event group being tested.
	        l,  // The bits within the event group to wait for.
	        pdFALSE,         // bits should be cleared before returning.
	        pdTRUE,        // Don't wait for all bits, any bit will do.
	        timeout_ms / portTICK_PERIOD_MS ) & l; // Wait a maximum of timeout_ms for any bit to be set.
    }
    if(h){
    	h = xEventGroupWaitBits(
	        _arduino_event_group,    // The event group being tested.
	        h,  // The bits within the event group to wait for.
	        pdFALSE,         // bits should be cleared before returning.
	        pdTRUE,        // Don't wait for all bits, any bit will do.
	        timeout_ms / portTICK_PERIOD_MS ) & h; // Wait a maximum of timeout_ms for any bit to be set.
    }
    return l | (h << 24);
}

const char * ESP_Network_Events::eventName(arduino_event_id_t id) {
    switch(id) {
        case ARDUINO_EVENT_ETH_START: return "ETH_START";
        case ARDUINO_EVENT_ETH_STOP: return "ETH_STOP";
        case ARDUINO_EVENT_ETH_CONNECTED: return "ETH_CONNECTED";
        case ARDUINO_EVENT_ETH_DISCONNECTED: return "ETH_DISCONNECTED";
        case ARDUINO_EVENT_ETH_GOT_IP: return "ETH_GOT_IP";
        case ARDUINO_EVENT_ETH_LOST_IP: return "ETH_LOST_IP";
        case ARDUINO_EVENT_ETH_GOT_IP6: return "ETH_GOT_IP6";

        case ARDUINO_EVENT_PPP_START: return "PPP_START";
        case ARDUINO_EVENT_PPP_STOP: return "PPP_STOP";
        case ARDUINO_EVENT_PPP_CONNECTED: return "PPP_CONNECTED";
        case ARDUINO_EVENT_PPP_DISCONNECTED: return "PPP_DISCONNECTED";
        case ARDUINO_EVENT_PPP_GOT_IP: return "PPP_GOT_IP";
        case ARDUINO_EVENT_PPP_LOST_IP: return "PPP_LOST_IP";
        case ARDUINO_EVENT_PPP_GOT_IP6: return "PPP_GOT_IP6";
#if SOC_WIFI_SUPPORTED
        case ARDUINO_EVENT_WIFI_READY: return "WIFI_READY";
        case ARDUINO_EVENT_WIFI_SCAN_DONE: return "SCAN_DONE";
        case ARDUINO_EVENT_WIFI_STA_START: return "STA_START";
        case ARDUINO_EVENT_WIFI_STA_STOP: return "STA_STOP";
        case ARDUINO_EVENT_WIFI_STA_CONNECTED: return "STA_CONNECTED";
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: return "STA_DISCONNECTED";
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: return "STA_AUTHMODE_CHANGE";
        case ARDUINO_EVENT_WIFI_STA_GOT_IP: return "STA_GOT_IP";
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6: return "STA_GOT_IP6";
        case ARDUINO_EVENT_WIFI_STA_LOST_IP: return "STA_LOST_IP";
        case ARDUINO_EVENT_WIFI_AP_START: return "AP_START";
        case ARDUINO_EVENT_WIFI_AP_STOP: return "AP_STOP";
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED: return "AP_STACONNECTED";
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: return "AP_STADISCONNECTED";
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED: return "AP_STAIPASSIGNED";
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED: return "AP_PROBEREQRECVED";
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6: return "AP_GOT_IP6";
        case ARDUINO_EVENT_WIFI_FTM_REPORT: return "FTM_REPORT";
        case ARDUINO_EVENT_WPS_ER_SUCCESS: return "WPS_ER_SUCCESS";
        case ARDUINO_EVENT_WPS_ER_FAILED: return "WPS_ER_FAILED";
        case ARDUINO_EVENT_WPS_ER_TIMEOUT: return "WPS_ER_TIMEOUT";
        case ARDUINO_EVENT_WPS_ER_PIN: return "WPS_ER_PIN";
        case ARDUINO_EVENT_WPS_ER_PBC_OVERLAP: return "WPS_ER_PBC_OVERLAP";
        case ARDUINO_EVENT_SC_SCAN_DONE: return "SC_SCAN_DONE";
        case ARDUINO_EVENT_SC_FOUND_CHANNEL: return "SC_FOUND_CHANNEL";
        case ARDUINO_EVENT_SC_GOT_SSID_PSWD: return "SC_GOT_SSID_PSWD";
        case ARDUINO_EVENT_SC_SEND_ACK_DONE: return "SC_SEND_ACK_DONE";
        case ARDUINO_EVENT_PROV_INIT: return "PROV_INIT";
        case ARDUINO_EVENT_PROV_DEINIT: return "PROV_DEINIT";
        case ARDUINO_EVENT_PROV_START: return "PROV_START";
        case ARDUINO_EVENT_PROV_END: return "PROV_END";
        case ARDUINO_EVENT_PROV_CRED_RECV: return "PROV_CRED_RECV";
        case ARDUINO_EVENT_PROV_CRED_FAIL: return "PROV_CRED_FAIL";
        case ARDUINO_EVENT_PROV_CRED_SUCCESS: return "PROV_CRED_SUCCESS";
#endif
        default: return "";
    }
}
