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

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

const char * _spp_server_name = "ESP32_SPP_SERVER";

#define QUEUE_SIZE 256
static uint32_t _spp_client = 0;
static xQueueHandle _spp_queue = NULL;

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        log_i("ESP_SPP_INIT_EVT");
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, _spp_server_name);
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT://discovery complete
        log_i("ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT://Client connection open
        log_i("ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT://Client connection closed
        _spp_client = 0;
        log_i("ESP_SPP_CLOSE_EVT");
        break;
    case ESP_SPP_START_EVT://server started
        log_i("ESP_SPP_START_EVT");
        break;
    case ESP_SPP_CL_INIT_EVT://client initiated a connection
        log_i("ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT://connection received data
        log_v("ESP_SPP_DATA_IND_EVT len=%d handle=%d", param->data_ind.len, param->data_ind.handle);
        //esp_log_buffer_hex("",param->data_ind.data,param->data_ind.len); //for low level debug

        if (_spp_queue != NULL){
            for (int i = 0; i < param->data_ind.len; i++)
                xQueueSend(_spp_queue, param->data_ind.data + i, (TickType_t)0);
        } else {
            log_e("SerialQueueBT ERROR");
        }
        break;
    case ESP_SPP_CONG_EVT://connection congestion status changed
        log_i("ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT://write operation completed
        log_v("ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT://Server connection open
        _spp_client = param->open.handle;
        log_i("ESP_SPP_SRV_OPEN_EVT");
        break;
    default:
        break;
    }
}

static bool _init_bt(const char *deviceName)
{
    if (!btStarted() && !btStart()){
        log_e("%s initialize controller failed\n", __func__);
        return false;
    }
    
    esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
    if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED){
        if (esp_bluedroid_init()) {
            log_e("%s initialize bluedroid failed\n", __func__);
            return false;
        }
    }
    
    if (bt_state != ESP_BLUEDROID_STATUS_ENABLED){
        if (esp_bluedroid_enable()) {
            log_e("%s enable bluedroid failed\n", __func__);
            return false;
        }
    }

    if (esp_spp_register_callback(esp_spp_cb) != ESP_OK){
        log_e("%s spp register failed\n", __func__);
        return false;
    }

    if (esp_spp_init(ESP_SPP_MODE_CB) != ESP_OK){
        log_e("%s spp init failed\n", __func__);
        return false;
    }

    _spp_queue = xQueueCreate(QUEUE_SIZE, sizeof(uint8_t)); //initialize the queue
    if (_spp_queue == NULL){
        log_e("%s Queue creation error\n", __func__);
        return false;
    }
    esp_bt_dev_set_device_name(deviceName);

    // the default BTA_DM_COD_LOUDSPEAKER does not work with the macOS BT stack
    esp_bt_cod_t cod;
    cod.major = 0b00001;
    cod.minor = 0b000100;
    cod.service = 0b00000010110;
    if (esp_bt_gap_set_cod(cod, ESP_BT_INIT_COD) != ESP_OK) {
        log_e("%s set cod failed\n", __func__);
        return false;
    }

    return true;
}

static bool _stop_bt()
{
    if (btStarted()){
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        btStop();
    }
    return true;
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

bool BluetoothSerial::begin(String localName)
{
    if (localName.length()){
        local_name = localName;
    }
    return _init_bt(local_name.c_str());
}

int BluetoothSerial::available(void)
{
    if (!_spp_client || _spp_queue == NULL){
        return 0;
    }
    return uxQueueMessagesWaiting(_spp_queue);
}

int BluetoothSerial::peek(void)
{
    if (available()){
        if (!_spp_client || _spp_queue == NULL){
            return 0;
        }

        uint8_t c;
        if (xQueuePeek(_spp_queue, &c, 0)){
            return c;
        }
    }
    return -1;
}

bool BluetoothSerial::hasClient(void)
{
    if (_spp_client)
        return true;
	
    return false;
}

int BluetoothSerial::read(void)
{
    if (available()){
        if (!_spp_client || _spp_queue == NULL){
            return 0;
        }

        uint8_t c;
        if (xQueueReceive(_spp_queue, &c, 0)){
            return c;
        }
    }
    return 0;
}

size_t BluetoothSerial::write(uint8_t c)
{
    if (!_spp_client){
        return 0;
    }

    uint8_t buffer[1];
    buffer[0] = c;
    esp_err_t err = esp_spp_write(_spp_client, 1, buffer);
    return (err == ESP_OK) ? 1 : 0;
}

size_t BluetoothSerial::write(const uint8_t *buffer, size_t size)
{
    if (!_spp_client){
        return 0;
    }

    esp_err_t err = esp_spp_write(_spp_client, size, (uint8_t *)buffer);
    return (err == ESP_OK) ? size : 0;
}

void BluetoothSerial::flush()
{
    if (_spp_client){
        int qsize = available();
        uint8_t buffer[qsize];
        esp_spp_write(_spp_client, qsize, buffer);
    }
}

void BluetoothSerial::end()
{
    _stop_bt();
}

#endif
