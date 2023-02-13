// Copyright 2018 Evandro Luis Copercini
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

#ifndef _BLUETOOTH_SERIAL_H_
#define _BLUETOOTH_SERIAL_H_

#include "sdkconfig.h"

#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include "Arduino.h"
#include "Stream.h"
#include <esp_gap_bt_api.h>
#include <esp_spp_api.h>
#include <functional>
#include <map>
#include "BTScan.h"

typedef std::function<void(const uint8_t *buffer, size_t size)> BluetoothSerialDataCb;
typedef std::function<void(uint32_t num_val)> ConfirmRequestCb;
typedef std::function<void(boolean success)> AuthCompleteCb;
typedef std::function<void(BTAdvertisedDevice* pAdvertisedDevice)> BTAdvertisedDeviceCb;

class BluetoothSerial: public Stream
{
    public:

        BluetoothSerial(void);
        ~BluetoothSerial(void);

        bool begin(String localName=String(), bool isMaster=false);
        bool begin(unsigned long baud){//compatibility
            return begin();
        }
        int available(void);
        int peek(void);
        bool hasClient(void);
        int read(void);
        size_t write(uint8_t c);
        size_t write(const uint8_t *buffer, size_t size);
        void flush();
        void end(void);
        void setTimeout(int timeoutMS);
        void onData(BluetoothSerialDataCb cb);
        esp_err_t register_callback(esp_spp_cb_t callback);
        
        void onConfirmRequest(ConfirmRequestCb cb);
        void onAuthComplete(AuthCompleteCb cb);
        void confirmReply(boolean confirm);

        void enableSSP();
        bool setPin(const char *pin);
        bool connect(String remoteName);
        bool connect(uint8_t remoteAddress[], int channel=0, esp_spp_sec_t sec_mask=(ESP_SPP_SEC_ENCRYPT|ESP_SPP_SEC_AUTHENTICATE), esp_spp_role_t role=ESP_SPP_ROLE_MASTER);
        bool connect(const BTAddress &remoteAddress, int channel=0, esp_spp_sec_t sec_mask=(ESP_SPP_SEC_ENCRYPT|ESP_SPP_SEC_AUTHENTICATE), esp_spp_role_t role=ESP_SPP_ROLE_MASTER) {
			return connect(*remoteAddress.getNative(), channel, sec_mask); };
        bool connect();
        bool connected(int timeout=0);
        bool isClosed();
        bool isReady(bool checkMaster=false, int timeout=0);
        bool disconnect();
        bool unpairDevice(uint8_t remoteAddress[]);

        BTScanResults* discover(int timeout=0x30*1280);
        bool discoverAsync(BTAdvertisedDeviceCb cb, int timeout=0x30*1280);
        void discoverAsyncStop();
        void discoverClear();
        BTScanResults* getScanResults();
        
        std::map<int, std::string> getChannels(const BTAddress &remoteAddress);

        const int INQ_TIME = 1280;   // Inquire Time unit 1280 ms
        const int MIN_INQ_TIME = (ESP_BT_GAP_MIN_INQ_LEN * INQ_TIME);
        const int MAX_INQ_TIME = (ESP_BT_GAP_MAX_INQ_LEN * INQ_TIME);
        
        operator bool() const;
        void getBtAddress(uint8_t *mac);
        BTAddress getBtAddressObject();
        String getBtAddressString();
    private:
        String local_name;
        int timeoutTicks=0;
};

#endif

#endif
