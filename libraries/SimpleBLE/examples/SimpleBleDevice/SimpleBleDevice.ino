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

// Sketch shows how SimpleBLE to advertise the name of the device and change it on button press
// Usefull if you want to advertise some short message
// Button is attached between GPIO 0 and GND and modes are switched with each press

#include "SimpleBLE.h"
SimpleBLE ble;

void onButton(){
    String out = "BLE32 at: ";
    out += String(millis() / 1000);
    Serial.println(out);
    ble.begin(out);
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    pinMode(0, INPUT_PULLUP);
    Serial.print("ESP32 SDK: ");
    Serial.println(ESP.getSdkVersion());
    ble.begin("ESP32 SimpleBLE");
    Serial.println("Press the button to change the device name");
}

void loop() {
    static uint8_t lastPinState = 1;
    uint8_t pinState = digitalRead(0);
    if(!pinState && lastPinState){
        onButton();
    }
    lastPinState = pinState;
    while(Serial.available()) Serial.write(Serial.read());
}
