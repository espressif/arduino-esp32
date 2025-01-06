// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief This example demonstrates Zigbee Network Scanning.
 *
 * The example demonstrates how to use ESP Zigbee stack to scan for Zigbee networks.
 *
 * Any Zigbee mode can be selected in Tools->Zigbee mode
 * with proper Zigbee partition scheme in Tools->Partition Scheme.
 *
 * Please check the README.md for instructions and more detailed description.
 *
 * Created by Jan Procházka (https://github.com/P-R-O-C-H-Y/)
 */

#if !defined(ZIGBEE_MODE_ED) && !defined(ZIGBEE_MODE_ZCZR)
#error "Zigbee device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#ifdef ZIGBEE_MODE_ZCZR
zigbee_role_t role = ZIGBEE_ROUTER;  // or can be ZIGBEE_COORDINATOR, but it won't scan itself
#else
zigbee_role_t role = ZIGBEE_END_DEVICE;
#endif

void printScannedNetworks(uint16_t networksFound) {
  if (networksFound == 0) {
    Serial.println("No networks found");
  } else {
    zigbee_scan_result_t *scan_result = Zigbee.getScanResult();
    Serial.println("\nScan done");
    Serial.print(networksFound);
    Serial.println(" networks found:");
    Serial.println("Nr | PAN ID | CH | Permit Joining | Router Capacity | End Device Capacity | Extended PAN ID");
    for (int i = 0; i < networksFound; ++i) {
      // Print all available info for each network found
      Serial.printf("%2d", i + 1);
      Serial.print(" | ");
      Serial.printf("0x%04hx", scan_result[i].short_pan_id);
      Serial.print(" | ");
      Serial.printf("%2d", scan_result[i].logic_channel);
      Serial.print(" | ");
      Serial.printf("%-14.14s", scan_result[i].permit_joining ? "Yes" : "No");
      Serial.print(" | ");
      Serial.printf("%-15.15s", scan_result[i].router_capacity ? "Yes" : "No");
      Serial.print(" | ");
      Serial.printf("%-19.19s", scan_result[i].end_device_capacity ? "Yes" : "No");
      Serial.print(" | ");
      Serial.printf(
        "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", scan_result[i].extended_pan_id[7], scan_result[i].extended_pan_id[6], scan_result[i].extended_pan_id[5],
        scan_result[i].extended_pan_id[4], scan_result[i].extended_pan_id[3], scan_result[i].extended_pan_id[2], scan_result[i].extended_pan_id[1],
        scan_result[i].extended_pan_id[0]
      );
      Serial.println();
      delay(10);
    }
    Serial.println("");
    // Delete the scan result to free memory for code below.
    Zigbee.scanDelete();
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize Zigbee stack without any EPs just for scanning
  if (!Zigbee.begin(role)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }

  Serial.println("Setup done, starting Zigbee network scan...");
  // Start Zigbee Network Scan with default parameters (all channels, scan time 5)
  Zigbee.scanNetworks();
}

void loop() {
  // check Zigbee Network Scan process
  int16_t ZigbeeScanStatus = Zigbee.scanComplete();
  if (ZigbeeScanStatus < 0) {  // it is busy scanning or got an error
    if (ZigbeeScanStatus == ZB_SCAN_FAILED) {
      Serial.println("Zigbee scan has failed. Starting again.");
      Zigbee.scanNetworks();
    }
    // other option is status ZB_SCAN_RUNNING - just wait.
  } else {  // Found Zero or more Wireless Networks
    printScannedNetworks(ZigbeeScanStatus);
    Zigbee.scanNetworks();  // start over...
  }

  // Loop can do something else...
  delay(500);
  Serial.println("Loop running...");
}
