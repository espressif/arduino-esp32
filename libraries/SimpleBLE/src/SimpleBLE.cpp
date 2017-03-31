// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#include "SimpleBLE.h"
#include "esp32-hal-log.h"

/*  HCI Command opcode group field(OGF) */
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS    (0x03 << 10)            /* 0x0C00 */
#define HCI_GRP_BLE_CMDS                   (0x08 << 10)

/*  HCI Command opcode command field(OCF) */
#define HCI_RESET                          (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_BLE_WRITE_ADV_ENABLE           (0x000A | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_PARAMS           (0x0006 | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_DATA             (0x0008 | HCI_GRP_BLE_CMDS)

#define HCI_H4_CMD_PREAMBLE_SIZE                (4)
#define HCIC_PARAM_SIZE_WRITE_ADV_ENABLE        (1)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS    (15)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA      (31)

/* EIR/AD data type definitions */
#define BT_DATA_FLAGS               0x01 /* AD flags */
#define BT_DATA_UUID16_SOME         0x02 /* 16-bit UUID, more available */
#define BT_DATA_UUID16_ALL          0x03 /* 16-bit UUID, all listed */
#define BT_DATA_UUID32_SOME         0x04 /* 32-bit UUID, more available */
#define BT_DATA_UUID32_ALL          0x05 /* 32-bit UUID, all listed */
#define BT_DATA_UUID128_SOME        0x06 /* 128-bit UUID, more available */
#define BT_DATA_UUID128_ALL         0x07 /* 128-bit UUID, all listed */
#define BT_DATA_NAME_SHORTENED      0x08 /* Shortened name */
#define BT_DATA_NAME_COMPLETE       0x09 /* Complete name */
#define BT_DATA_TX_POWER            0x0a /* Tx Power */
#define BT_DATA_SOLICIT16           0x14 /* Solicit UUIDs, 16-bit */
#define BT_DATA_SOLICIT128          0x15 /* Solicit UUIDs, 128-bit */
#define BT_DATA_SVC_DATA16          0x16 /* Service data, 16-bit UUID */
#define BT_DATA_GAP_APPEARANCE      0x19 /* GAP appearance */
#define BT_DATA_SOLICIT32           0x1f /* Solicit UUIDs, 32-bit */
#define BT_DATA_SVC_DATA32          0x20 /* Service data, 32-bit UUID */
#define BT_DATA_SVC_DATA128         0x21 /* Service data, 128-bit UUID */
#define BT_DATA_MANUFACTURER_DATA   0xff /* Manufacturer Specific Data */


/* Advertising types */
#define     BLE_GAP_ADV_TYPE_ADV_IND         0x00
#define     BLE_GAP_ADV_TYPE_ADV_DIRECT_IND  0x01
#define     BLE_GAP_ADV_TYPE_ADV_SCAN_IND    0x02
#define     BLE_GAP_ADV_TYPE_ADV_NONCONN_IND 0x03


/* Advertising Discovery Flags */
#define     BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE       (0x01)
#define     BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE       (0x02)
#define     BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED       (0x04)
#define     BLE_GAP_ADV_FLAG_LE_BR_EDR_CONTROLLER       (0x08)
#define     BLE_GAP_ADV_FLAG_LE_BR_EDR_HOST             (0x10)
#define     BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE (BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)
#define     BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE (BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)


/* Advertising Filter Policies */
#define     BLE_GAP_ADV_FP_ANY              0x00
#define     BLE_GAP_ADV_FP_FILTER_SCANREQ   0x01
#define     BLE_GAP_ADV_FP_FILTER_CONNREQ   0x02
#define     BLE_GAP_ADV_FP_FILTER_BOTH      0x03


/* Advertising Device Address Types */
#define     BLE_GAP_ADDR_TYPE_PUBLIC                          0x00
#define     BLE_GAP_ADDR_TYPE_RANDOM_STATIC                   0x01
#define     BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE       0x02
#define     BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE   0x03


/* GAP Advertising Channel Maps */
#define     GAP_ADVCHAN_37          0x01
#define     GAP_ADVCHAN_38          0x02
#define     GAP_ADVCHAN_39          0x04
#define     GAP_ADVCHAN_ALL         GAP_ADVCHAN_37 | GAP_ADVCHAN_38 | GAP_ADVCHAN_39


/* GAP Filter Policies */
#define     BLE_GAP_ADV_FP_ANY              0x00
#define     BLE_GAP_ADV_FP_FILTER_SCANREQ   0x01
#define     BLE_GAP_ADV_FP_FILTER_CONNREQ   0x02
#define     BLE_GAP_ADV_FP_FILTER_BOTH      0x03

#define BD_ADDR_LEN     (6)                     /* Device address length */


/*
 * BLE System
 *
 * */

/* HCI H4 message type definitions */
enum {
    H4_TYPE_COMMAND = 1,
    H4_TYPE_ACL     = 2,
    H4_TYPE_SCO     = 3,
    H4_TYPE_EVENT   = 4
};

volatile bool _vhci_host_send_available = false;
volatile bool _vhci_host_command_running = false;
static uint16_t _vhci_host_command = 0x0000;
static uint8_t _vhci_host_command_result = 0x00;

//controller is ready to receive command
static void _on_tx_ready(void)
{
    _vhci_host_send_available = true;
}
/*
static void _dump_buf(const char * txt, uint8_t *data, uint16_t len){
    log_printf("%s[%u]:", txt, len);
    for (uint16_t i=0; i<len; i++)
        log_printf(" %02x", data[i]);
    log_printf("\n");
}
*/
//controller has a packet
static int _on_rx_data(uint8_t *data, uint16_t len)
{
    if(len == 7 && *data == 0x04){
        //baseband response
        uint16_t cmd = (((uint16_t)data[5] << 8) | data[4]);
        uint8_t res = data[6];
        if(_vhci_host_command_running && _vhci_host_command == cmd){
            //_dump_buf("BLE: res", data, len);
            _vhci_host_command_result = res;
            _vhci_host_command_running = false;
            return 0;
        } else if(cmd == 0){
            log_e("error %u", res);
        }
    }

    //_dump_buf("BLE: rx", data, len);
    return 0;
}




static esp_vhci_host_callback_t vhci_host_cb = {
        _on_tx_ready,
        _on_rx_data
};

static bool _esp_ble_start()
{
    if(btStart()){
        esp_vhci_host_register_callback(&vhci_host_cb);
        uint8_t i = 0;
        while(!esp_vhci_host_check_send_available() && i++ < 100){
            delay(10);
        }
        if(i >= 100){
            log_e("esp_vhci_host_check_send_available failed");
            return false;
        }
        _vhci_host_send_available = true;
    } else 
        log_e("BT Failed");
    return true;
}

static bool _esp_ble_stop()
{
    if(btStarted()){
        _vhci_host_send_available = false;
        btStop();
        esp_vhci_host_register_callback(NULL);
    }
    return true;
}

//public

static uint8_t ble_send_cmd(uint16_t cmd, uint8_t * data, uint8_t len){
    static uint8_t buf[36];
    if(len > 32){
        //too much data
        return 2;
    }
    uint16_t i = 0;
    while(!_vhci_host_send_available && i++ < 1000){
        delay(1);
    }
    if(i >= 1000){
        log_e("_vhci_host_send_available failed");
        return 1;
    }
    uint8_t outlen = len + HCI_H4_CMD_PREAMBLE_SIZE;
    buf[0] = H4_TYPE_COMMAND;
    buf[1] = (uint8_t)(cmd & 0xFF);
    buf[2] = (uint8_t)(cmd >> 8);
    buf[3] = len;
    if(len){
        memcpy(buf+4, data, len);
    }
    _vhci_host_send_available = false;
    _vhci_host_command_running = true;
    _vhci_host_command = cmd;

    //log_printf("BLE: cmd: 0x%04X, data[%u]:", cmd, len);
    //for (uint16_t i=0; i<len; i++) log_printf(" %02x", buf[i+4]);
    //log_printf("\n");
    
    esp_vhci_host_send_packet(buf, outlen);
    while(_vhci_host_command_running);
    int res = _vhci_host_command_result;
    //log_printf("BLE: cmd: 0x%04X, res: %u\n", cmd, res);
    return res;
}


/*
 * BLE Arduino
 *
 * */

enum {
    UNIT_0_625_MS = 625,  /* Number of microseconds in 0.625 milliseconds. */
    UNIT_1_25_MS = 1250,  /* Number of microseconds in 1.25 milliseconds. */
    UNIT_10_MS = 10000  /* Number of microseconds in 10 milliseconds. */
};

/* BLE Advertising parameters struct */
typedef struct ble_gap_adv_params_s {
        uint8_t type;
        uint8_t own_addr_type;
        uint8_t addr_type;
        uint8_t addr[BD_ADDR_LEN];
        uint8_t fp;  // filter policy
        uint16_t interval_min;  // minimum advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20ms to 10.24s)
        uint16_t interval_max;
        uint8_t chn_map;
} ble_adv_params_t;

#define MSEC_TO_UNITS(TIME, RESOLUTION)  (((TIME) * 1000) / (RESOLUTION))
#define UINT16_TO_STREAM(p, u16)         {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)           {*(p)++ = (uint8_t)(u8);}
#define BDADDR_TO_STREAM(p, a)           {int i; for (i = 0; i < BD_ADDR_LEN;  i++) *(p)++ = (uint8_t) a[BD_ADDR_LEN - 1 - i];}
#define ARRAY_TO_STREAM(p, a, len)       {int i; for (i = 0; i < len;          i++) *(p)++ = (uint8_t) a[i];}

SimpleBLE::SimpleBLE()
{
    uint8_t peerAddr[BD_ADDR_LEN] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85};
    _ble_adv_param = (ble_adv_params_t*)malloc(sizeof(ble_adv_params_t));
    memset(_ble_adv_param, 0x00, sizeof(ble_adv_params_t));
    _ble_adv_param->type = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;//not connectable
    _ble_adv_param->chn_map = GAP_ADVCHAN_ALL; // 37, 38, 39 channels
    _ble_adv_param->fp = 0;//any
    _ble_adv_param->interval_min = 512;
    _ble_adv_param->interval_max = 1024;
    _ble_adv_param->addr_type = 0;//public
    memcpy(_ble_adv_param->addr, peerAddr, BD_ADDR_LEN);
    local_name = "esp32";
}

SimpleBLE::~SimpleBLE(void)
{
    free(_ble_adv_param);
    _esp_ble_stop();
}

bool SimpleBLE::begin(String localName)
{
    if(!_esp_ble_start()){
        return false;
    }
    ble_send_cmd(HCI_RESET, NULL, 0);
    if(localName.length()){
        local_name = localName;
    }
    _ble_send_adv_param();
    _ble_send_adv_data();

    uint8_t adv_enable = 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_ENABLE, &adv_enable, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
    return true;
}

void SimpleBLE::end()
{
    uint8_t adv_enable = 0;
    ble_send_cmd(HCI_BLE_WRITE_ADV_ENABLE, &adv_enable, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
    ble_send_cmd(HCI_RESET, NULL, 0);
    _esp_ble_stop();
}

void SimpleBLE::_ble_send_adv_param(void)
{
    uint8_t dbuf[HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS];
    uint8_t *buf = dbuf;
    UINT16_TO_STREAM (buf, _ble_adv_param->interval_min);
    UINT16_TO_STREAM (buf, _ble_adv_param->interval_max);
    UINT8_TO_STREAM (buf, _ble_adv_param->type);
    UINT8_TO_STREAM (buf, _ble_adv_param->own_addr_type);
    UINT8_TO_STREAM (buf, _ble_adv_param->addr_type);
    ARRAY_TO_STREAM (buf, _ble_adv_param->addr, BD_ADDR_LEN);
    UINT8_TO_STREAM (buf, _ble_adv_param->chn_map);
    UINT8_TO_STREAM (buf, _ble_adv_param->fp);
    ble_send_cmd(HCI_BLE_WRITE_ADV_PARAMS, dbuf, HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS);
}

void SimpleBLE::_ble_send_adv_data(void)
{
    uint8_t adv_data[HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1] = {
            0x03, 0x02, BT_DATA_FLAGS, BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE
    };
    //zerofill the buffer
    memset(adv_data+4, 0x00, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA-4);
    uint8_t adv_data_len = 4;

    // Advertising data device local name
    uint8_t name_len = (uint8_t) local_name.length();
    adv_data[adv_data_len++] = name_len + 1;
    adv_data[adv_data_len++] = BT_DATA_NAME_COMPLETE;
    for (int i=0; i<name_len; i++) {
        adv_data[adv_data_len++] = (uint8_t) local_name.charAt(i);
    }
    //send data
    adv_data[0] = adv_data_len - 1;
    ble_send_cmd(HCI_BLE_WRITE_ADV_DATA, (uint8_t *)adv_data, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1);
}
