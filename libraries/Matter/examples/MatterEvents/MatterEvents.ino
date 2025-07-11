// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

// Matter Manager
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
// if the device can be commissioned using BLE, WiFi is not used - save flash space
#include <WiFi.h>
#endif

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
// WiFi is manually set and started
const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password
#endif

// List of Matter Endpoints for this Node
// On/Off Light Endpoint
MatterOnOffLight OnOffLight;

// This function is called when a Matter event occurs
void onMatterEvent(matterEvent_t eventType, const chip::DeviceLayer::ChipDeviceEvent *eventInfo) {
  // Print the event type to Serial
  Serial.print("===> Got a Matter Event: ");
  switch (eventType) {
    case MATTER_WIFI_CONNECTIVITY_CHANGE:   Serial.println("WiFi Connectivity Change"); break;
    case MATTER_THREAD_CONNECTIVITY_CHANGE: Serial.println("Thread Connectivity Change"); break;
    case MATTER_INTERNET_CONNECTIVITY_CHANGE:
    {
      bool newIPAddress = false;
      Serial.print("Internet Connectivity Change :: ");
      if (eventInfo->InternetConnectivityChange.IPv4 != chip::DeviceLayer::ConnectivityChange::kConnectivity_NoChange) {
        Serial.print("IPv4 Connectivity: ");
        switch (eventInfo->InternetConnectivityChange.IPv4) {
          case chip::DeviceLayer::ConnectivityChange::kConnectivity_Established:
          {
            newIPAddress = true;
            break;
          }
          case chip::DeviceLayer::ConnectivityChange::kConnectivity_Lost: Serial.println("Lost"); break;
          default:                                                        Serial.println("Unknown"); break;
        }
      }
      if (eventInfo->InternetConnectivityChange.IPv6 != chip::DeviceLayer::ConnectivityChange::kConnectivity_NoChange) {
        Serial.print("IPv6 Connectivity: ");
        switch (eventInfo->InternetConnectivityChange.IPv6) {
          case chip::DeviceLayer::ConnectivityChange::kConnectivity_Established:
          {
            newIPAddress = true;
            break;
          }
          case chip::DeviceLayer::ConnectivityChange::kConnectivity_Lost: Serial.println("Lost"); break;
          default:                                                        Serial.println("Unknown"); break;
        }
      }
      // Print the IP address if it was established
      if (newIPAddress) {
        Serial.print("Established - IP Address: ");
        char ipAddressStr[chip::Transport::PeerAddress::kMaxToStringSize];
        eventInfo->InternetConnectivityChange.ipAddress.ToString(ipAddressStr);
        Serial.println(ipAddressStr);
      }
      break;
    }
    case MATTER_SERVICE_CONNECTIVITY_CHANGE:     Serial.println("Service Connectivity Change"); break;
    case MATTER_SERVICE_PROVISIONING_CHANGE:     Serial.println("Service Provisioning Change"); break;
    case MATTER_TIME_SYNC_CHANGE:                Serial.println("Time Sync Change"); break;
    case MATTER_CHIPOBLE_CONNECTION_ESTABLISHED: Serial.println("CHIPoBLE Connection Established"); break;
    case MATTER_CHIPOBLE_CONNECTION_CLOSED:      Serial.println("CHIPoBLE Connection Closed"); break;
    case MATTER_CLOSE_ALL_BLE_CONNECTIONS:       Serial.println("Close All BLE Connections"); break;
    case MATTER_WIFI_DEVICE_AVAILABLE:           Serial.println("WiFi Device Available"); break;
    case MATTER_OPERATIONAL_NETWORK_STARTED:     Serial.println("Operational Network Started"); break;
    case MATTER_THREAD_STATE_CHANGE:             Serial.println("Thread State Change"); break;
    case MATTER_THREAD_INTERFACE_STATE_CHANGE:   Serial.println("Thread Interface State Change"); break;
    case MATTER_CHIPOBLE_ADVERTISING_CHANGE:     Serial.println("CHIPoBLE Advertising Change"); break;
    case MATTER_INTERFACE_IP_ADDRESS_CHANGED:
      switch (eventInfo->InterfaceIpAddressChanged.Type) {
        case chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Assigned: Serial.println("IPv4 Address Assigned"); break;
        case chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Lost:     Serial.println("IPv4 Address Lost"); break;
        case chip::DeviceLayer::InterfaceIpChangeType::kIpV6_Assigned: Serial.println("IPv6 Address Assigned"); break;
        case chip::DeviceLayer::InterfaceIpChangeType::kIpV6_Lost:     Serial.println("IPv6 Address Lost"); break;
      }
      break;
    case MATTER_COMMISSIONING_COMPLETE:        Serial.println("Commissioning Complete"); break;
    case MATTER_FAIL_SAFE_TIMER_EXPIRED:       Serial.println("Fail Safe Timer Expired"); break;
    case MATTER_OPERATIONAL_NETWORK_ENABLED:   Serial.println("Operational Network Enabled"); break;
    case MATTER_DNSSD_INITIALIZED:             Serial.println("DNS-SD Initialized"); break;
    case MATTER_DNSSD_RESTART_NEEDED:          Serial.println("DNS-SD Restart Needed"); break;
    case MATTER_BINDINGS_CHANGED_VIA_CLUSTER:  Serial.println("Bindings Changed Via Cluster"); break;
    case MATTER_OTA_STATE_CHANGED:             Serial.println("OTA State Changed"); break;
    case MATTER_SERVER_READY:                  Serial.println("Server Ready"); break;
    case MATTER_BLE_DEINITIALIZED:             Serial.println("BLE Deinitialized"); break;
    case MATTER_COMMISSIONING_SESSION_STARTED: Serial.println("Commissioning Session Started"); break;
    case MATTER_COMMISSIONING_SESSION_STOPPED: Serial.println("Commissioning Session Stopped"); break;
    case MATTER_COMMISSIONING_WINDOW_OPEN:     Serial.println("Commissioning Window Opened"); break;
    case MATTER_COMMISSIONING_WINDOW_CLOSED:   Serial.println("Commissioning Window Closed"); break;
    case MATTER_FABRIC_WILL_BE_REMOVED:        Serial.println("Fabric Will Be Removed"); break;
    case MATTER_FABRIC_REMOVED:                Serial.println("Fabric Removed"); break;
    case MATTER_FABRIC_COMMITTED:              Serial.println("Fabric Committed"); break;
    case MATTER_FABRIC_UPDATED:                Serial.println("Fabric Updated"); break;
    case MATTER_ESP32_SPECIFIC_EVENT:          Serial.println("Sending ESP32 Platform Specific Events"); break;
    case MATTER_ESP32_PUBLIC_SPECIFIC_EVENT:   Serial.println("Next Event Has Populated EventInfo"); break;
    default:
      // If the event type is not recognized, print "Unknown" and the event ID
      Serial.println("Unknown, EventID = 0x" + String(eventType, HEX));
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);  // Wait for Serial to initialize
  }

// CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
#if !CONFIG_ENABLE_CHIPOBLE
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  // Manually connect to WiFi
  WiFi.enableIPv6(true);  // Enable IPv6 if needed
  WiFi.begin(ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\r\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);
#endif

  // Initialize at least one Matter EndPoint
  OnOffLight.begin();

  // Set the Matter Event Callback
  Matter.onEvent(onMatterEvent);
  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  Serial.println("Starting Matter Commission Test...");
}

void loop() {
  // Check Matter Commissioning state
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Light Commissioning.
    while (!Matter.isDeviceCommissioned()) {
      delay(5000);
      Serial.println("Matter Fabric not commissioned yet. Waiting for commissioning.");
    }
  }
  Serial.println("Matter Node is commissioned and connected to Wi-Fi.");
  Serial.println("====> Decommissioning in 60 seconds. <====");
  delay(60000);
  Matter.decommission();
  Serial.println("Matter Node is decommissioned. Commissioning widget shall start over.");
}
