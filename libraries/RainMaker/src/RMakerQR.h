/*
    RMakerQR.h - ESP RainMaker qrcode support.
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
    log_i("Scan this QR code from the phone app for Provisioning.");
    qrcode_display(payload);
    log_i("If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, payload);
}

