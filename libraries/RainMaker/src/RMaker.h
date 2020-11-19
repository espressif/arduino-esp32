 /*
    RMaker.h - ESP RainMaker library
    Copyright (c) 2020 Arduino. All right reserved.
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
  
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
  
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Arduino.h"
#include "RMakerNode.h"
#include "RMakerQR.h"
#include <esp_rmaker_standard_types.h>

class RMakerClass
{
    private:
        esp_rmaker_config_t rainmaker_cfg = {false};
      
    public:
    
        void setTimeSync(bool val);
        Node initNode(const char *name, const char *type = "ESP RainMaker with Arduino");
        esp_err_t deinitNode(Node node);
        esp_err_t setTimeZone(const char *tz = "Asia/Shanghai");
        esp_err_t enableSchedule();
        esp_err_t enableOTA(ota_type_t type, const char *cert = ESP_RMAKER_OTA_DEFAULT_SERVER_CERT);
        esp_err_t start();
        esp_err_t stop();
};

extern RMakerClass RMaker;
