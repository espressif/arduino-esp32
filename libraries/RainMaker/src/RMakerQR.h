// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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
#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK
#include "esp_system.h"
#include <qrcode.h>

#define PROV_QR_VERSION "v1"
#define QRCODE_BASE_URL     "https://rainmaker.espressif.com/qrcode.html"

static void printQR(const char *name, const char *pop, const char *transport)
{
    if (!name || !pop || !transport) {
        log_w("Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150];
    snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" \
                    ",\"pop\":\"%s\",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, name, pop, transport);
    Serial.printf("Scan this QR code from the ESP RainMaker phone app.\n");
    qrcode_display(payload);
    Serial.printf("If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s\n", QRCODE_BASE_URL, payload);
}
#endif