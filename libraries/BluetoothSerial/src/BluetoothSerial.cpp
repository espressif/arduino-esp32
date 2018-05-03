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

#define SPP_SERVER_NAME "ESP32_SPP_SERVER"
#define SPP_TAG "BluetoothSerial"

#define QUEUE_SIZE 256
uint32_t client;
xQueueHandle SerialQueueBT;

static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_NONE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        client = 0;
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
        break;
    case ESP_SPP_START_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:
        ESP_LOGV(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d", param->data_ind.len, param->data_ind.handle);
        //esp_log_buffer_hex("",param->data_ind.data,param->data_ind.len); //for low level debug

        if (SerialQueueBT != 0){
            for (int i = 0; i < param->data_ind.len; i++)
                xQueueSend(SerialQueueBT, param->data_ind.data + i, (TickType_t)0);
        }
        else {
            ESP_LOGE(SPP_TAG, "SerialQueueBT ERROR");
        }
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGV(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        client = param->open.handle;
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
        break;
    default:
        break;
    }
}

static bool _init_bt(const char *deviceName)
{
    if (!btStarted() && !btStart()){
        ESP_LOGE(SPP_TAG, "%s initialize controller failed\n", __func__);
        return false;
    }
    
    esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
    if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED){
        if (esp_bluedroid_init()) {
            ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed\n", __func__);
            return false;
        }
    }
    
    if (bt_state != ESP_BLUEDROID_STATUS_ENABLED){
        if (esp_bluedroid_enable()) {
            ESP_LOGE(SPP_TAG, "%s enable bluedroid failed\n", __func__);
            return false;
        }
    }

    if (esp_spp_register_callback(esp_spp_cb) != ESP_OK){
        ESP_LOGE(SPP_TAG, "%s spp register failed\n", __func__);
        return false;
    }

    if (esp_spp_init(esp_spp_mode) != ESP_OK){
        ESP_LOGE(SPP_TAG, "%s spp init failed\n", __func__);
        return false;
    }

    SerialQueueBT = xQueueCreate(QUEUE_SIZE, sizeof(uint8_t)); //initialize the queue
    if (SerialQueueBT == NULL){
        ESP_LOGE(SPP_TAG, "%s Queue creation error\n", __func__);
        return false;
    }
    esp_bt_dev_set_device_name(deviceName);

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
    if (!client || SerialQueueBT == NULL){
        return 0;
    }
    return uxQueueMessagesWaiting(SerialQueueBT);
}

int BluetoothSerial::peek(void)
{
    if (available()){
        if (!client || SerialQueueBT == NULL){
            return 0;
        }

        uint8_t c;
        if (xQueuePeek(SerialQueueBT, &c, 0)){
            return c;
        }
    }
    return -1;
}

bool BluetoothSerial::hasClient(void)
{
    if (client)
        return true;
	
    return false;
}

int BluetoothSerial::read(void)
{
    if (available()){
        if (!client || SerialQueueBT == NULL){
            return 0;
        }

        uint8_t c;
        if (xQueueReceive(SerialQueueBT, &c, 0)){
            return c;
        }
    }
    return 0;
}

size_t BluetoothSerial::write(uint8_t c)
{
    if (client){
        uint8_t buffer[1];
        buffer[0] = c;
        esp_spp_write(client, 1, buffer);
        return 1;
    }
    return -1;
}

size_t BluetoothSerial::write(const uint8_t *buffer, size_t size)
{
    if (client){
        esp_spp_write(client, size, (uint8_t *)buffer);
    }
    return size;
}

void BluetoothSerial::flush()
{
    if (client){
        int qsize = available();
        uint8_t buffer[qsize];
        esp_spp_write(client, qsize, buffer);
    }
}

void BluetoothSerial::end()
{
    _stop_bt();
}

#endif
