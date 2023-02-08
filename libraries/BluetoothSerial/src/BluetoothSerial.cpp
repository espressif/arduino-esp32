// Copyright 2018 Evandro Luis Copercini
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sdkconfig.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

#include "BluetoothSerial.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include <esp_log.h>

#include "esp32-hal-log.h"

const char * _spp_server_name = "ESP32SPP";

#define RX_QUEUE_SIZE 512
#define TX_QUEUE_SIZE 32
#define SPP_TX_QUEUE_TIMEOUT 1000
#define SPP_TX_DONE_TIMEOUT 1000
#define SPP_CONGESTED_TIMEOUT 1000

static uint32_t _spp_client = 0;
static xQueueHandle _spp_rx_queue = NULL;
static xQueueHandle _spp_tx_queue = NULL;
static SemaphoreHandle_t _spp_tx_done = NULL;
static TaskHandle_t _spp_task_handle = NULL;
static EventGroupHandle_t _spp_event_group = NULL;
static EventGroupHandle_t _bt_event_group = NULL;
static boolean secondConnectionAttempt;
static esp_spp_cb_t custom_spp_callback = NULL;
static BluetoothSerialDataCb custom_data_callback = NULL;
static esp_bd_addr_t current_bd_addr;
static ConfirmRequestCb confirm_request_callback = NULL;
static AuthCompleteCb auth_complete_callback = NULL;

#define INQ_LEN 0x10
#define INQ_NUM_RSPS 20
#define READY_TIMEOUT (10 * 1000)
#define SCAN_TIMEOUT (INQ_LEN * 2 * 1000)
static esp_bd_addr_t _peer_bd_addr;
static char _remote_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
static bool _isRemoteAddressSet;
static bool _isMaster;
static esp_bt_pin_code_t _pin_code;
static int _pin_len;
static bool _isPinSet;
static bool _enableSSP;
static esp_spp_sec_t _sec_mask;
static esp_spp_role_t _role;
// start connect on ESP_SPP_DISCOVERY_COMP_EVT or save entry for getChannels
static bool _doConnect;
static std::map <int, std::string> sdpRecords;

static BTScanResultsSet scanResults;
static BTAdvertisedDeviceCb advertisedDeviceCb = nullptr;

// _spp_event_group
#define SPP_RUNNING     0x01
#define SPP_CONNECTED   0x02
#define SPP_CONGESTED   0x04
// true until OPEN successful, changes to false on CLOSE
#define SPP_DISCONNECTED 0x08
// true until connect(), changes to true on CLOSE
#define SPP_CLOSED      0x10

// _bt_event_group
#define BT_DISCOVERY_RUNNING    0x01
#define BT_DISCOVERY_COMPLETED  0x02

#define BT_SDP_RUNNING          0x04
#define BT_SDP_COMPLETED        0x08

typedef struct {
        size_t len;
        uint8_t data[];
} spp_packet_t;

#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
  if (bda == NULL || str == NULL || size < 18) {
    return NULL;
  }

  uint8_t *p = bda;
  snprintf(str, size, "%02x:%02x:%02x:%02x:%02x:%02x",
          p[0], p[1], p[2], p[3], p[4], p[5]);
  return str;
}
#endif

static bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len)
{
    if (!eir || !bdname || !bdname_len) {
        return false;
    }

    uint8_t *rmt_bdname, rmt_bdname_len;
    *bdname = *bdname_len = rmt_bdname_len = 0;

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }
    if (rmt_bdname) {
        rmt_bdname_len = rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN ? ESP_BT_GAP_MAX_BDNAME_LEN : rmt_bdname_len;
        memcpy(bdname, rmt_bdname, rmt_bdname_len);
        bdname[rmt_bdname_len] = 0;
        *bdname_len = rmt_bdname_len;
        return true;
    }
    return false;
}

static bool btSetPin() {
    esp_bt_pin_type_t pin_type;
    if (_isPinSet) {
        if (_pin_len) {
            log_i("pin set");
            pin_type = ESP_BT_PIN_TYPE_FIXED;
        } else {
            _isPinSet = false;
            log_i("pin reset");
            pin_type = ESP_BT_PIN_TYPE_VARIABLE; // pin_code would be ignored (default)
        }
        return (esp_bt_gap_set_pin(pin_type, _pin_len, _pin_code) == ESP_OK);        
    }
    return false;
}

static esp_err_t _spp_queue_packet(uint8_t *data, size_t len){
    if(!data || !len){
        log_w("No data provided");
        return ESP_OK;
    }
    spp_packet_t * packet = (spp_packet_t*)malloc(sizeof(spp_packet_t) + len);
    if(!packet){
        log_e("SPP TX Packet Malloc Failed!");
        return ESP_FAIL;
    }
    packet->len = len;
    memcpy(packet->data, data, len);
    if (!_spp_tx_queue || xQueueSend(_spp_tx_queue, &packet, SPP_TX_QUEUE_TIMEOUT) != pdPASS) {
        log_e("SPP TX Queue Send Failed!");
        free(packet);
        return ESP_FAIL;
    }
    return ESP_OK;
}

const uint16_t SPP_TX_MAX = 330;
static uint8_t _spp_tx_buffer[SPP_TX_MAX];
static uint16_t _spp_tx_buffer_len = 0;

static bool _spp_send_buffer(){
    if((xEventGroupWaitBits(_spp_event_group, SPP_CONGESTED, pdFALSE, pdTRUE, SPP_CONGESTED_TIMEOUT) & SPP_CONGESTED) != 0){
        if(!_spp_client){
            log_v("SPP Client Gone!");
            return false;
        }
        log_v("SPP Write %u", _spp_tx_buffer_len);
        esp_err_t err = esp_spp_write(_spp_client, _spp_tx_buffer_len, _spp_tx_buffer);
        if(err != ESP_OK){
            log_e("SPP Write Failed! [0x%X]", err);
            return false;
        }
        _spp_tx_buffer_len = 0;
        if(xSemaphoreTake(_spp_tx_done, SPP_TX_DONE_TIMEOUT) != pdTRUE){
            log_e("SPP Ack Failed!");
            return false;
        }
        return true;
    }
    log_e("SPP Write Congested!");
    return false;
}

static void _spp_tx_task(void * arg){
    spp_packet_t *packet = NULL;
    size_t len = 0, to_send = 0;
    uint8_t * data = NULL;
    for (;;) {
        if(_spp_tx_queue && xQueueReceive(_spp_tx_queue, &packet, portMAX_DELAY) == pdTRUE && packet){
            if(packet->len <= (SPP_TX_MAX - _spp_tx_buffer_len)){
                memcpy(_spp_tx_buffer+_spp_tx_buffer_len, packet->data, packet->len);
                _spp_tx_buffer_len+=packet->len;
                free(packet);
                packet = NULL;
                if(SPP_TX_MAX == _spp_tx_buffer_len || uxQueueMessagesWaiting(_spp_tx_queue) == 0){
                    _spp_send_buffer();
                }
            } else {
                len = packet->len;
                data = packet->data;
                to_send = SPP_TX_MAX - _spp_tx_buffer_len;
                memcpy(_spp_tx_buffer+_spp_tx_buffer_len, data, to_send);
                _spp_tx_buffer_len = SPP_TX_MAX;
                data += to_send;
                len -= to_send;
                if(!_spp_send_buffer()){
                    len = 0;
                }
                while(len >= SPP_TX_MAX){
                    memcpy(_spp_tx_buffer, data, SPP_TX_MAX);
                    _spp_tx_buffer_len = SPP_TX_MAX;
                    data += SPP_TX_MAX;
                    len -= SPP_TX_MAX;
                    if(!_spp_send_buffer()){
                        len = 0;
                        break;
                    }
                }
                if(len){
                    memcpy(_spp_tx_buffer, data, len);
                    _spp_tx_buffer_len += len;
                    if(uxQueueMessagesWaiting(_spp_tx_queue) == 0){
                        _spp_send_buffer();
                    }
                }
                free(packet);
                packet = NULL;
            }
        } else {
            log_e("Something went horribly wrong");
        }
    }
    vTaskDelete(NULL);
    _spp_task_handle = NULL;
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        log_i("ESP_SPP_INIT_EVT");
#ifdef ESP_IDF_VERSION_MAJOR
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif
        if (!_isMaster) {
            log_i("ESP_SPP_INIT_EVT: slave: start");
            esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, _spp_server_name);
        }
        xEventGroupSetBits(_spp_event_group, SPP_RUNNING);
        break;

    case ESP_SPP_SRV_OPEN_EVT://Server connection open
        if (param->srv_open.status == ESP_SPP_SUCCESS) {
            log_i("ESP_SPP_SRV_OPEN_EVT: %u", _spp_client);
            if (!_spp_client){
                _spp_client = param->srv_open.handle;
                _spp_tx_buffer_len = 0;
            } else {
                secondConnectionAttempt = true;
                esp_spp_disconnect(param->srv_open.handle);
            }
            xEventGroupClearBits(_spp_event_group, SPP_DISCONNECTED);
            xEventGroupSetBits(_spp_event_group, SPP_CONNECTED);
        } else {
            log_e("ESP_SPP_SRV_OPEN_EVT Failed!, status:%d", param->srv_open.status);
        }
        break;

    case ESP_SPP_CLOSE_EVT://Client connection closed
        if ((param->close.async == false && param->close.status == ESP_SPP_SUCCESS) || param->close.async) {
            log_i("ESP_SPP_CLOSE_EVT status:%d handle:%d close_by_remote:%d attempt %u", param->close.status,
                 param->close.handle, param->close.async, secondConnectionAttempt);
            if(secondConnectionAttempt) {
                secondConnectionAttempt = false;
            } else {
                _spp_client = 0;
                xEventGroupSetBits(_spp_event_group, SPP_DISCONNECTED);
                xEventGroupSetBits(_spp_event_group, SPP_CONGESTED);
                xEventGroupSetBits(_spp_event_group, SPP_CLOSED);
                xEventGroupClearBits(_spp_event_group, SPP_CONNECTED);
            }        
        } else {
            log_e("ESP_SPP_CLOSE_EVT failed!, status:%d", param->close.status);
        }
        break;

    case ESP_SPP_CONG_EVT://connection congestion status changed
        if(param->cong.cong){
            xEventGroupClearBits(_spp_event_group, SPP_CONGESTED);
        } else {
            xEventGroupSetBits(_spp_event_group, SPP_CONGESTED);
        }
        log_v("ESP_SPP_CONG_EVT: %s", param->cong.cong?"CONGESTED":"FREE");
        break;

    case ESP_SPP_WRITE_EVT://write operation completed
        if (param->write.status == ESP_SPP_SUCCESS) {
            if(param->write.cong){
                xEventGroupClearBits(_spp_event_group, SPP_CONGESTED);
            }
            log_v("ESP_SPP_WRITE_EVT: %u %s", param->write.len, param->write.cong?"CONGESTED":"");
        } else {
            log_e("ESP_SPP_WRITE_EVT failed!, status:%d", param->write.status);
        }
        xSemaphoreGive(_spp_tx_done);//we can try to send another packet
        break;

    case ESP_SPP_DATA_IND_EVT://connection received data
        log_v("ESP_SPP_DATA_IND_EVT len=%d handle=%d", param->data_ind.len, param->data_ind.handle);
        //esp_log_buffer_hex("",param->data_ind.data,param->data_ind.len); //for low level debug
        //ets_printf("r:%u\n", param->data_ind.len);

        if(custom_data_callback){
            custom_data_callback(param->data_ind.data, param->data_ind.len);
        } else if (_spp_rx_queue != NULL){
            for (int i = 0; i < param->data_ind.len; i++){
                if(xQueueSend(_spp_rx_queue, param->data_ind.data + i, (TickType_t)0) != pdTRUE){
                    log_e("RX Full! Discarding %u bytes", param->data_ind.len - i);
                    break;
                }
            }
        }
        break;

    case ESP_SPP_DISCOVERY_COMP_EVT://discovery complete
        log_i("ESP_SPP_DISCOVERY_COMP_EVT num=%d", param->disc_comp.scn_num);
        if (param->disc_comp.status == ESP_SPP_SUCCESS) {
            for(int i=0; i < param->disc_comp.scn_num; i++) {
                log_d("ESP_SPP_DISCOVERY_COMP_EVT: spp [%d] channel: %d service name:%s", i, param->disc_comp.scn[i], param->disc_comp.service_name[0]);
            }
            if(_doConnect) {
                if(param->disc_comp.scn_num > 0) {
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
                    char bda_str[18];
                    log_i("ESP_SPP_DISCOVERY_COMP_EVT: spp connect to remote %s channel %d",
                        bda2str(_peer_bd_addr, bda_str, sizeof(bda_str)),
                        param->disc_comp.scn[0]);
#endif
                    xEventGroupClearBits(_spp_event_group, SPP_CLOSED);
                    if(esp_spp_connect(_sec_mask, _role, param->disc_comp.scn[0], _peer_bd_addr) != ESP_OK) {
                        log_e("ESP_SPP_DISCOVERY_COMP_EVT connect failed");
                        xEventGroupSetBits(_spp_event_group, SPP_CLOSED);
                    }
                } else {
                    log_e("ESP_SPP_DISCOVERY_COMP_EVT remote doesn't offer an SPP channel");
                    xEventGroupSetBits(_spp_event_group, SPP_CLOSED);
                }
            } else {
                for(int i=0; i < param->disc_comp.scn_num; i++) {
                    sdpRecords[param->disc_comp.scn[i]] = param->disc_comp.service_name[0];
                }
            }
        } else {
            log_e("ESP_SPP_DISCOVERY_COMP_EVT failed!, status:%d", param->disc_comp.status);
        }
        xEventGroupSetBits(_bt_event_group, BT_SDP_COMPLETED);
        break;

    case ESP_SPP_OPEN_EVT://Client connection open
        log_i("ESP_SPP_OPEN_EVT");
        if (!_spp_client){
                _spp_client = param->open.handle;
        } else {
            secondConnectionAttempt = true;
            esp_spp_disconnect(param->open.handle);
        }
        xEventGroupClearBits(_spp_event_group, SPP_DISCONNECTED);
        xEventGroupSetBits(_spp_event_group, SPP_CONNECTED);
        xEventGroupSetBits(_spp_event_group, SPP_CONGESTED);
        break;

    case ESP_SPP_START_EVT://server started
        log_i("ESP_SPP_START_EVT");
        break;

    case ESP_SPP_CL_INIT_EVT://client initiated a connection
        if (param->cl_init.status == ESP_SPP_SUCCESS) {
            log_i("ESP_SPP_CL_INIT_EVT handle:%d sec_id:%d", param->cl_init.handle, param->cl_init.sec_id);
        } else {
            log_i("ESP_SPP_CL_INIT_EVT status:%d", param->cl_init.status);
        }
        break;

    default:
        log_i("ESP_SPP_* event unhandled %d", event);
        break;
    }
    if(custom_spp_callback)(*custom_spp_callback)(event, param);
}

void BluetoothSerial::onData(BluetoothSerialDataCb cb){
    custom_data_callback = cb;
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch(event){
        case ESP_BT_GAP_DISC_RES_EVT: {
            log_i("ESP_BT_GAP_DISC_RES_EVT properties=%d", param->disc_res.num_prop);
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
            char bda_str[18];
            log_i("Scanned device: %s", bda2str(param->disc_res.bda, bda_str, 18));
#endif
            BTAdvertisedDeviceSet advertisedDevice;
            uint8_t peer_bdname_len = 0;
            char peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
            for (int i = 0; i < param->disc_res.num_prop; i++) {
                switch(param->disc_res.prop[i].type) {
                    case ESP_BT_GAP_DEV_PROP_EIR:  
                        if (get_name_from_eir((uint8_t*)param->disc_res.prop[i].val, peer_bdname, &peer_bdname_len)) {
                            log_i("ESP_BT_GAP_DISC_RES_EVT : EIR : %s : %d", peer_bdname, peer_bdname_len);
                            if (strlen(_remote_name) == peer_bdname_len
                                && strncmp(peer_bdname, _remote_name, peer_bdname_len) == 0) {
                                log_v("ESP_BT_GAP_DISC_RES_EVT : SPP_START_DISCOVERY_EIR : %s", peer_bdname, peer_bdname_len);
                                _isRemoteAddressSet = true;
                                memcpy(_peer_bd_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
                                esp_bt_gap_cancel_discovery();
                                esp_spp_start_discovery(_peer_bd_addr);
                            }
                        }
                        break;

                    case ESP_BT_GAP_DEV_PROP_BDNAME:
                        peer_bdname_len = param->disc_res.prop[i].len;
                        memcpy(peer_bdname, param->disc_res.prop[i].val, peer_bdname_len);
                        peer_bdname_len--; // len includes 0 terminator
                        log_v("ESP_BT_GAP_DISC_RES_EVT : BDNAME :  %s : %d", peer_bdname, peer_bdname_len);
                        if (strlen(_remote_name) == peer_bdname_len
                            && strncmp(peer_bdname, _remote_name, peer_bdname_len) == 0) {
                            log_i("ESP_BT_GAP_DISC_RES_EVT : SPP_START_DISCOVERY_BDNAME : %s", peer_bdname);
                            _isRemoteAddressSet = true;
                            memcpy(_peer_bd_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
                            esp_bt_gap_cancel_discovery();
                            esp_spp_start_discovery(_peer_bd_addr);
                        } 
                        break;

                    case ESP_BT_GAP_DEV_PROP_COD:
                        if (param->disc_res.prop[i].len <= sizeof(int)) {
                            uint32_t cod = 0;
                            memcpy(&cod, param->disc_res.prop[i].val, param->disc_res.prop[i].len);
                            advertisedDevice.setCOD(cod);
                            log_d("ESP_BT_GAP_DEV_PROP_COD 0x%x", cod);
                        } else {
                            log_d("ESP_BT_GAP_DEV_PROP_COD invalid COD: Value size larger than integer");
                        }
                        break;

                    case ESP_BT_GAP_DEV_PROP_RSSI:
                        if (param->disc_res.prop[i].len <= sizeof(int)) {
                            uint8_t rssi = 0;
                            memcpy(&rssi, param->disc_res.prop[i].val, param->disc_res.prop[i].len);
                            log_d("ESP_BT_GAP_DEV_PROP_RSSI %d", rssi);
                            advertisedDevice.setRSSI(rssi);
                        } else {
                            log_d("ESP_BT_GAP_DEV_PROP_RSSI invalid RSSI: Value size larger than integer");
                        }
                        break;
                        
                    default:
                        log_i("ESP_BT_GAP_DISC_RES_EVT unknown property [%d]:type:%d", i, param->disc_res.prop[i].type);
                        break;
                }
                if (_isRemoteAddressSet)
                    break;
            }
            if (peer_bdname_len)
                advertisedDevice.setName(peer_bdname);
            esp_bd_addr_t addr;
            memcpy(addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
            advertisedDevice.setAddress(BTAddress(addr));
            if (scanResults.add(advertisedDevice) && advertisedDeviceCb)
                advertisedDeviceCb(&advertisedDevice);
        }
        break;

        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                log_i("ESP_BT_GAP_DISC_STATE_CHANGED_EVT stopped");
                xEventGroupClearBits(_bt_event_group, BT_DISCOVERY_RUNNING);
                xEventGroupSetBits(_bt_event_group, BT_DISCOVERY_COMPLETED);
            } else { // ESP_BT_GAP_DISCOVERY_STARTED
                log_i("ESP_BT_GAP_DISC_STATE_CHANGED_EVT started");
                xEventGroupClearBits(_bt_event_group, BT_DISCOVERY_COMPLETED);
                xEventGroupSetBits(_bt_event_group, BT_DISCOVERY_RUNNING);
            }
            break;

        case ESP_BT_GAP_RMT_SRVCS_EVT:
            log_i( "ESP_BT_GAP_RMT_SRVCS_EVT: status = %d, num_uuids = %d", param->rmt_srvcs.stat, param->rmt_srvcs.num_uuids);
            break;

        case ESP_BT_GAP_RMT_SRVC_REC_EVT:
            log_i("ESP_BT_GAP_RMT_SRVC_REC_EVT: status = %d", param->rmt_srvc_rec.stat);
            break;

        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                log_v("authentication success: %s", param->auth_cmpl.device_name);
                if (auth_complete_callback) {
                    auth_complete_callback(true);
                }
            } else {
                log_e("authentication failed, status:%d", param->auth_cmpl.stat);
                if (auth_complete_callback) {
                    auth_complete_callback(false);
                }
            }
            break;

        case ESP_BT_GAP_PIN_REQ_EVT:
            // default pairing pins
            log_i("ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
            if (param->pin_req.min_16_digit) {
                log_i("Input pin code: 0000 0000 0000 0000");
                esp_bt_pin_code_t pin_code;
                memset(pin_code, '0', ESP_BT_PIN_CODE_LEN);
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
            } else {
                log_i("Input pin code: 1234");
                esp_bt_pin_code_t pin_code;
                memcpy(pin_code, "1234", 4);
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            }
            break;
       
        case ESP_BT_GAP_CFM_REQ_EVT:
            log_i("ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
            if (confirm_request_callback) {
                memcpy(current_bd_addr, param->cfm_req.bda, sizeof(esp_bd_addr_t));
                confirm_request_callback(param->cfm_req.num_val);
            }
            else {
                esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            }
            break;

        case ESP_BT_GAP_KEY_NOTIF_EVT:
            log_i("ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
            break;

        case ESP_BT_GAP_KEY_REQ_EVT:
            log_i("ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
            break;

        case ESP_BT_GAP_CONFIG_EIR_DATA_EVT:
            log_i("ESP_BT_GAP_CONFIG_EIR_DATA_EVT: stat:%d num:%d", param->config_eir_data.stat, param->config_eir_data.eir_type_num);
            break;

        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS ) {
                log_i("ESP_BT_GAP_READ_REMOTE_NAME_EVT: %s", param->read_rmt_name.rmt_name);
            } else {
                log_i("ESP_BT_GAP_READ_REMOTE_NAME_EVT: no success stat:%d", param->read_rmt_name.stat);
            }
            break;

        case ESP_BT_GAP_MODE_CHG_EVT:
            log_i("ESP_BT_GAP_MODE_CHG_EVT: mode: %d", param->mode_chg.mode);
            break;

        default:
            log_i("ESP-BT_GAP_* unknown message: %d", event);
            break;
    }
}

static bool _init_bt(const char *deviceName)
{
    if(!_bt_event_group){
        _bt_event_group = xEventGroupCreate();
        if(!_bt_event_group){
            log_e("BT Event Group Create Failed!");
            return false;
        }
        xEventGroupClearBits(_bt_event_group, 0xFFFFFF);
    }
    if(!_spp_event_group){
        _spp_event_group = xEventGroupCreate();
        if(!_spp_event_group){
            log_e("SPP Event Group Create Failed!");
            return false;
        }
        xEventGroupClearBits(_spp_event_group, 0xFFFFFF);
        xEventGroupSetBits(_spp_event_group, SPP_CONGESTED);
        xEventGroupSetBits(_spp_event_group, SPP_DISCONNECTED);
        xEventGroupSetBits(_spp_event_group, SPP_CLOSED);
    }
    if (_spp_rx_queue == NULL){
        _spp_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(uint8_t)); //initialize the queue
        if (_spp_rx_queue == NULL){
            log_e("RX Queue Create Failed");
            return false;
        }
    }
    if (_spp_tx_queue == NULL){
        _spp_tx_queue = xQueueCreate(TX_QUEUE_SIZE, sizeof(spp_packet_t*)); //initialize the queue
        if (_spp_tx_queue == NULL){
            log_e("TX Queue Create Failed");
            return false;
        }
    }
    if(_spp_tx_done == NULL){
        _spp_tx_done = xSemaphoreCreateBinary();
        if (_spp_tx_done == NULL){
            log_e("TX Semaphore Create Failed");
            return false;
        }
        xSemaphoreTake(_spp_tx_done, 0);
    }

    if(!_spp_task_handle){
        xTaskCreatePinnedToCore(_spp_tx_task, "spp_tx", 4096, NULL, 10, &_spp_task_handle, 0);
        if(!_spp_task_handle){
            log_e("Network Event Task Start Failed!");
            return false;
        }
    }

    if (!btStarted() && !btStart()){
        log_e("initialize controller failed");
        return false;
    }
    
    esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
    if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED){
        if (esp_bluedroid_init()) {
            log_e("initialize bluedroid failed");
            return false;
        }
    }
    
    if (bt_state != ESP_BLUEDROID_STATUS_ENABLED){
        if (esp_bluedroid_enable()) {
            log_e("enable bluedroid failed");
            return false;
        }
    }

    if (esp_bt_gap_register_callback(esp_bt_gap_cb) != ESP_OK) {
        log_e("gap register failed");
        return false;
    }

    if (esp_spp_register_callback(esp_spp_cb) != ESP_OK){
        log_e("spp register failed");
        return false;
    }

    if (esp_spp_init(ESP_SPP_MODE_CB) != ESP_OK){
        log_e("spp init failed");
        return false;
    }

    // if (esp_bt_sleep_disable() != ESP_OK){
    //     log_e("esp_bt_sleep_disable failed");
    // }

    log_i("device name set");
    esp_bt_dev_set_device_name(deviceName);

    if (_isPinSet) {
        log_i("pin set");
        btSetPin();
    }

    if (_enableSSP) {
        log_i("Simple Secure Pairing");
        esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    }

    // the default BTA_DM_COD_LOUDSPEAKER does not work with the macOS BT stack
    esp_bt_cod_t cod;
    cod.major = 0b00001;
    cod.minor = 0b000100;
    cod.service = 0b00000010110;
    if (esp_bt_gap_set_cod(cod, ESP_BT_INIT_COD) != ESP_OK) {
        log_e("set cod failed");
        return false;
    }
    return true;
}

static bool _stop_bt()
{
    if (btStarted()){
        if(_spp_client)
            esp_spp_disconnect(_spp_client);
        esp_spp_deinit();
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        btStop();
    }
    _spp_client = 0;
    if(_spp_task_handle){
        vTaskDelete(_spp_task_handle);
        _spp_task_handle = NULL;
    }
    if(_spp_event_group){
        vEventGroupDelete(_spp_event_group);
        _spp_event_group = NULL;
    }
    if(_spp_rx_queue){
        vQueueDelete(_spp_rx_queue);
        //ToDo: clear RX queue when in packet mode
        _spp_rx_queue = NULL;
    }
    if(_spp_tx_queue){
        spp_packet_t *packet = NULL;
        while(xQueueReceive(_spp_tx_queue, &packet, 0) == pdTRUE){
            free(packet);
        }
        vQueueDelete(_spp_tx_queue);
        _spp_tx_queue = NULL;
    }
    if (_spp_tx_done) {
        vSemaphoreDelete(_spp_tx_done);
        _spp_tx_done = NULL;
    }
    if (_bt_event_group) {
        vEventGroupDelete(_bt_event_group);
        _bt_event_group = NULL;
    }
    return true;
}

static bool waitForConnect(int timeout) {
    TickType_t xTicksToWait = timeout / portTICK_PERIOD_MS;
    // wait for connected or closed
    EventBits_t rc = xEventGroupWaitBits(_spp_event_group, SPP_CONNECTED | SPP_CLOSED, pdFALSE, pdFALSE, xTicksToWait);
    if((rc & SPP_CONNECTED) != 0)
        return true;
    else if((rc & SPP_CLOSED) != 0) {
        log_d("connection closed!");
        return false;
    }
    log_d("timeout");
    return false;
}

static bool waitForDiscovered(int timeout) {
    TickType_t xTicksToWait = timeout / portTICK_PERIOD_MS;
    return (xEventGroupWaitBits(_spp_event_group, BT_DISCOVERY_COMPLETED, pdFALSE, pdTRUE, xTicksToWait) & BT_DISCOVERY_COMPLETED) != 0;
}

static bool waitForSDPRecord(int timeout) {
    TickType_t xTicksToWait = timeout / portTICK_PERIOD_MS;
    return (xEventGroupWaitBits(_bt_event_group, BT_SDP_COMPLETED, pdFALSE, pdTRUE, xTicksToWait) & BT_SDP_COMPLETED) != 0;
}

/*
 * Serial Bluetooth Arduino
 *
 * */

BluetoothSerial::BluetoothSerial()
{
    local_name = "ESP32"; //default bluetooth name
}

BluetoothSerial::~BluetoothSerial(void)
{
    _stop_bt();
}

/**
 * @Param isMaster set to true if you want to connect to an other device
 */
bool BluetoothSerial::begin(String localName, bool isMaster)
{
    _isMaster = isMaster;
    if (localName.length()){
        local_name = localName;
    }
    return _init_bt(local_name.c_str());
}

int BluetoothSerial::available(void)
{
    if (_spp_rx_queue == NULL){
        return 0;
    }
    return uxQueueMessagesWaiting(_spp_rx_queue);
}

int BluetoothSerial::peek(void)
{
    uint8_t c;
    if (_spp_rx_queue && xQueuePeek(_spp_rx_queue, &c, this->timeoutTicks)){
        return c;
    }
    return -1;
}

bool BluetoothSerial::hasClient(void)
{
    return _spp_client > 0;
}

int BluetoothSerial::read()
{

    uint8_t c = 0;
    if (_spp_rx_queue && xQueueReceive(_spp_rx_queue, &c, this->timeoutTicks)){
        return c;
    }
    return -1;
}

/**
 * Set timeout for read / peek
 */
void BluetoothSerial::setTimeout(int timeoutMS)
{
    this->timeoutTicks=timeoutMS / portTICK_PERIOD_MS;
}

size_t BluetoothSerial::write(uint8_t c)
{
    return write(&c, 1);
}

size_t BluetoothSerial::write(const uint8_t *buffer, size_t size)
{
    if (!_spp_client){
        return 0;
    }
    return (_spp_queue_packet((uint8_t *)buffer, size) == ESP_OK) ? size : 0;
}

void BluetoothSerial::flush()
{
    if (_spp_tx_queue != NULL){
        while(uxQueueMessagesWaiting(_spp_tx_queue) > 0){
           delay(100);
        }
    }
}

void BluetoothSerial::end()
{
    _stop_bt();
}

void BluetoothSerial::onConfirmRequest(ConfirmRequestCb cb)
{
    confirm_request_callback = cb;
}

void BluetoothSerial::onAuthComplete(AuthCompleteCb cb)
{
    auth_complete_callback = cb;
}

void BluetoothSerial::confirmReply(boolean confirm)
{
    esp_bt_gap_ssp_confirm_reply(current_bd_addr, confirm);  
}


esp_err_t BluetoothSerial::register_callback(esp_spp_cb_t callback)
{
    custom_spp_callback = callback;
    return ESP_OK;
}

//Simple Secure Pairing
void BluetoothSerial::enableSSP() {
    _enableSSP = true;
}
/*
     * Set default parameters for Legacy Pairing
     * Use fixed pin code
*/
bool BluetoothSerial::setPin(const char *pin) {
    log_i("pin: %s", pin);
    bool isEmpty =  !(pin  && *pin);
    if (isEmpty && !_isPinSet) {
        return true; // nothing to do
    } else if (!isEmpty){
        _pin_len = strlen(pin);
        memcpy(_pin_code, pin, _pin_len);
    } else {
        _pin_len = 0; // resetting pin to none (default)
    }
    _pin_code[_pin_len] = 0;
    _isPinSet = true;
    if (isReady(false, READY_TIMEOUT)) {
        btSetPin();
    }
    return true;
}

bool BluetoothSerial::connect(String remoteName)
{
    bool retval = false;

    if (!isReady(true, READY_TIMEOUT)) return false;
    if (remoteName && remoteName.length() < 1) {
        log_e("No remote name is provided");
        return false; 
    }
    disconnect();
    _doConnect = true;
    _isRemoteAddressSet = false;
	_sec_mask = ESP_SPP_SEC_ENCRYPT|ESP_SPP_SEC_AUTHENTICATE;
	_role = ESP_SPP_ROLE_MASTER;
    strncpy(_remote_name, remoteName.c_str(), ESP_BT_GAP_MAX_BDNAME_LEN);
    _remote_name[ESP_BT_GAP_MAX_BDNAME_LEN] = 0;
    log_i("master : remoteName");
    // will first resolve name to address
#ifdef ESP_IDF_VERSION_MAJOR
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif
    xEventGroupClearBits(_spp_event_group, SPP_CLOSED);
    if (esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, INQ_LEN, INQ_NUM_RSPS) == ESP_OK) {
        retval = waitForConnect(SCAN_TIMEOUT);
    }
    if (retval == false) {
      _isRemoteAddressSet = false;
    }
    return retval;
}

/**
 * @Param channel: specify channel or 0 for auto-detect
 * @Param sec_mask:
 *           ESP_SPP_SEC_ENCRYPT|ESP_SPP_SEC_AUTHENTICATE
 *           ESP_SPP_SEC_NONE
 * @Param role:
 *           ESP_SPP_ROLE_MASTER   master can handle up to 7 connections to slaves
 *           ESP_SPP_ROLE_SLAVE    can only have one connection to a master
 */
bool BluetoothSerial::connect(uint8_t remoteAddress[], int channel, esp_spp_sec_t sec_mask, esp_spp_role_t role)
{
    bool retval = false;
    if (!isReady(true, READY_TIMEOUT)) return false;
    if (!remoteAddress) {
        log_e("No remote address is provided");
        return false; 
    }
    disconnect();
    _doConnect = true;
    _remote_name[0] = 0;
    _isRemoteAddressSet = true;
	_sec_mask = sec_mask;
	_role = role;
    memcpy(_peer_bd_addr, remoteAddress, ESP_BD_ADDR_LEN);
    log_i("master : remoteAddress");
    xEventGroupClearBits(_spp_event_group, SPP_CLOSED);
    if (channel > 0) {
#if (ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO)
        char bda_str[18];
        log_i("spp connect to remote %s channel %d",
            bda2str(_peer_bd_addr, bda_str, sizeof(bda_str)),
            channel);
#endif
        if(esp_spp_connect(sec_mask, role, channel, _peer_bd_addr) != ESP_OK ) {
			log_e("spp connect failed");
      retval = false;
    } else {
      retval = waitForConnect(READY_TIMEOUT);
      if(retval) {
            log_i("connected");
        } else {
            if(this->isClosed()) {
                log_e("connect failed");
            } else {
                log_e("connect timed out after %dms", READY_TIMEOUT);
            }
        }
    }
  } else if (esp_spp_start_discovery(_peer_bd_addr) == ESP_OK) {
    retval = waitForConnect(READY_TIMEOUT);
  }

  if (!retval) {
    _isRemoteAddressSet = false;
    }
    return retval;
}

bool BluetoothSerial::connect()
{
    if (!isReady(true, READY_TIMEOUT)) return false;
    _doConnect = true;
    if (_isRemoteAddressSet){
        disconnect();
        // use resolved or set address first
        log_i("master : remoteAddress");
        if (esp_spp_start_discovery(_peer_bd_addr) == ESP_OK) {
            return waitForConnect(READY_TIMEOUT);
        }
        return false;
    } else if (_remote_name[0]) {
        disconnect();
        log_i("master : remoteName");
        // will resolve name to address first - it may take a while
#ifdef ESP_IDF_VERSION_MAJOR
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif
        if (esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, INQ_LEN, INQ_NUM_RSPS) == ESP_OK) {
            return waitForConnect(SCAN_TIMEOUT);
        }
        return false;
    }
    log_e("Neither Remote name nor address was provided");
    return false;
}

bool BluetoothSerial::disconnect() {
    if (_spp_client) {
        flush();
        log_i("disconnecting");
        if (esp_spp_disconnect(_spp_client) == ESP_OK) {
            TickType_t xTicksToWait = READY_TIMEOUT / portTICK_PERIOD_MS;
            return (xEventGroupWaitBits(_spp_event_group, SPP_DISCONNECTED, pdFALSE, pdTRUE, xTicksToWait) & SPP_DISCONNECTED) != 0;
        }
    }
    return false;
}

bool BluetoothSerial::unpairDevice(uint8_t remoteAddress[]) {
    if (isReady(false, READY_TIMEOUT)) {
        log_i("removing bonded device");
        return (esp_bt_gap_remove_bond_device(remoteAddress) == ESP_OK);
    }
    return false;
}

bool BluetoothSerial::connected(int timeout) {
    return waitForConnect(timeout);
}

/**
 * true if a connection terminated or a connection attempt failed
 */
bool BluetoothSerial::isClosed() {
    return xEventGroupGetBits(_spp_event_group) & SPP_CLOSED;
}

bool BluetoothSerial::isReady(bool checkMaster, int timeout) {
    if (checkMaster && !_isMaster) {
        log_e("Master mode is not active. Call begin(localName, true) to enable Master mode");
        return false;
    }
    if (!btStarted()) {
        log_e("BT is not initialized. Call begin() first");
        return false;
    }
    TickType_t xTicksToWait = timeout / portTICK_PERIOD_MS;
    return (xEventGroupWaitBits(_spp_event_group, SPP_RUNNING, pdFALSE, pdTRUE, xTicksToWait) & SPP_RUNNING) != 0;
}


/**
 * @brief           RemoteName or address are not allowed to be set during discovery
 *                  (otherwhise it might connect automatically and stop discovery)
 * @param[in]       timeoutMs can range from MIN_INQ_TIME to MAX_INQ_TIME
 * @return          in case of Error immediately Empty ScanResults.
 */
BTScanResults* BluetoothSerial::discover(int timeoutMs) {
    scanResults.clear();
    if (timeoutMs < MIN_INQ_TIME || timeoutMs > MAX_INQ_TIME || strlen(_remote_name) || _isRemoteAddressSet)
        return nullptr;
    int timeout = timeoutMs / INQ_TIME;
    log_i("discover::disconnect");
    disconnect();
    log_i("discovering");
    // will resolve name to address first - it may take a while
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    if (esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, timeout, 0) == ESP_OK) {
        waitForDiscovered(timeoutMs);
        log_i("gap_cancel_discovery()");
        esp_bt_gap_cancel_discovery();
    }
    return &scanResults;
}

/**
 * @brief           RemoteName or address are not allowed to be set during discovery
 *                  (otherwhise it might connect automatically and stop discovery)
 * @param[in]       cb called when a [b]new[/b] device has been discovered
 * @param[in]       timeoutMs can be 0 or range from MIN_INQ_TIME to MAX_INQ_TIME
 *
 * @return          Wheter start was successfull or problems with params
 */
bool BluetoothSerial::discoverAsync(BTAdvertisedDeviceCb cb, int timeoutMs) {
    scanResults.clear();
    if (strlen(_remote_name) || _isRemoteAddressSet)
        return false;
    int timeout = timeoutMs / INQ_TIME;
    disconnect();
    advertisedDeviceCb = cb;
    log_i("discovering");
    // will resolve name to address first - it may take a while
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    if (timeout > 0)
        return esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, timeout, 0) == ESP_OK;
    else return esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, ESP_BT_GAP_MAX_INQ_LEN, 0) == ESP_OK;
}

/** @brief      Stops the asynchronous discovery and clears the callback */
void BluetoothSerial::discoverAsyncStop() {
    esp_bt_gap_cancel_discovery();
    advertisedDeviceCb = nullptr;
}

/** @brief      Clears scanresult entries */
void BluetoothSerial::discoverClear() {
    scanResults.clear();
}

/** @brief      Can be used while discovering asynchronously
 *              Will be returned also on synchronous discovery.
 *
 * @return      BTScanResults contains several information of found devices
 */
BTScanResults* BluetoothSerial::getScanResults() {
    return &scanResults;
}

BluetoothSerial::operator bool() const
{
    return true;
}

/**
 * SDP scan address
 * esp_spp_start_discovery doesn't tell us the btAddress in the callback, so we have to wait until it's finished
 */
std::map<int, std::string> BluetoothSerial::getChannels(const BTAddress &remoteAddress) {
    if(xEventGroupGetBits(_bt_event_group) & BT_SDP_RUNNING) {
        log_e("getChannels failed - already running");
    }
    xEventGroupSetBits(_bt_event_group, BT_SDP_RUNNING);
    xEventGroupClearBits(_bt_event_group, BT_SDP_COMPLETED);
    _doConnect = false;
    sdpRecords.clear();
    log_d("esp_spp_start_discovery");
    if (esp_spp_start_discovery(*remoteAddress.getNative()) != ESP_OK) {
        log_e("esp_spp_start_discovery failed");
    } else {
        if(! waitForSDPRecord(READY_TIMEOUT)) {
            log_e("getChannels failed timeout");
        }
        log_d("esp_spp_start_discovery wait for BT_SDP_COMPLETED done (%dms)", READY_TIMEOUT);
    }
    log_d("esp_spp_start_discovery done, found %d services", sdpRecords.size());
    xEventGroupClearBits(_bt_event_group, BT_SDP_RUNNING);
    return sdpRecords;
}

/**
 * @brief      Gets the MAC address of local BT device in byte array.
 *
 * @param      mac [out]  The mac
 */
void BluetoothSerial::getBtAddress(uint8_t *mac) {
    const uint8_t *dev_mac = esp_bt_dev_get_address();
    memcpy(mac, dev_mac, ESP_BD_ADDR_LEN);
}
/**
 * @brief      Gets the MAC address of local BT device as BTAddress object.
 *
 * @return     The BTAddress object.
 */
BTAddress BluetoothSerial::getBtAddressObject() {
    uint8_t mac_arr[ESP_BD_ADDR_LEN];
    getBtAddress(mac_arr);
    return BTAddress(mac_arr);
}
/**
 * @brief      Gets the MAC address of local BT device as string.
 *
 * @return     The BT MAC address string.
 */
String BluetoothSerial::getBtAddressString() {
    return getBtAddressObject().toString(true);
}
#endif
