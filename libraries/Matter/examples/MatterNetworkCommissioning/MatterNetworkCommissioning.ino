/*
   Matter Network Commissioning Control Example

   This example demonstrates how to control which networks (WiFi/Thread) are available
   for commissioning when using CHIPoBLE (Chip over Bluetooth Low Energy).

   The sketch shows how to:
   1. Enable/disable WiFi network commissioning
   2. Enable/disable Thread network commissioning
   3. Set specific network commissioning modes
   4. Monitor commissioning events

   Instructions:
   1. Use a Matter commissioner app (like CHIPTool) to start commissioning
   2. Connect via BLE and commission the device to your desired network
   3. Use the serial commands to control which networks are available

   Serial Commands:
   - 'w' : Toggle WiFi commissioning
   - 't' : Toggle Thread commissioning  
   - 'r' : Reset to factory defaults
   - 's' : Show current network status
   - '1' : WiFi only mode
   - '2' : Thread only mode
   - '3' : Both networks mode
   - '0' : No networks mode

   Author: ESP32 Arduino Team
   Date: 2024
*/

#include <Matter.h>
#include <WiFi.h>

// Matter Light Endpoint
MatterOnOffLight OnOffLight;

// Network commissioning states
bool wifiCommissioningEnabled = true;
bool threadCommissioningEnabled = false;

void setup() {
  Serial.begin(115200);
  
  // Register Matter event callback
  Matter.onEvent(onMatterEvent);
  
  // Initialize Matter endpoint
  OnOffLight.begin();
  
  // Set initial network commissioning mode BEFORE Matter.begin()
  // Start with WiFi only by default
  if (Matter.setNetworkCommissioningMode(wifiCommissioningEnabled, threadCommissioningEnabled)) {
    Serial.println("Initial network commissioning mode set successfully");
  } else {
    Serial.println("Warning: Failed to set initial network commissioning mode");
  }
  
  // Start Matter stack
  Matter.begin();
  
  // Synchronize our state variables with actual API state
  syncNetworkStates();
  
  Serial.println("Matter Network Commissioning Control Example");
  Serial.println("===========================================");
  printNetworkStatus();
  printCommands();
}

void loop() {
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    handleSerialCommand(cmd);
  }
  
  // Update Matter
  delay(100);
}

void handleSerialCommand(char cmd) {
  switch (cmd) {
    case 'w':
    case 'W':
      toggleWiFiCommissioning();
      break;
      
    case 't':
    case 'T':
      toggleThreadCommissioning();
      break;
      
    case 'r':
    case 'R':
      resetToFactory();
      break;
      
    case 's':
    case 'S':
      printNetworkStatus();
      break;
      
    case '1':
      setWiFiOnlyMode();
      break;
      
    case '2':
      setThreadOnlyMode();
      break;
      
    case '3':
      setBothNetworksMode();
      break;
      
    case '0':
      setNoNetworksMode();
      break;
      
    case 'h':
    case 'H':
    case '?':
      printCommands();
      break;
      
    default:
      if (cmd != '\n' && cmd != '\r') {
        Serial.println("Unknown command. Type 'h' for help.");
      }
      break;
  }
}

void toggleWiFiCommissioning() {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  wifiCommissioningEnabled = !wifiCommissioningEnabled;
  
  if (wifiCommissioningEnabled) {
    if (Matter.enableWiFiNetworkCommissioning()) {
      Serial.println("✓ WiFi commissioning enabled");
    } else {
      Serial.println("✗ Failed to enable WiFi commissioning");
      wifiCommissioningEnabled = false;  // Revert state
    }
  } else {
    if (Matter.disableWiFiNetworkCommissioning()) {
      Serial.println("✓ WiFi commissioning disabled");
    } else {
      Serial.println("✗ Failed to disable WiFi commissioning");
      wifiCommissioningEnabled = true;  // Revert state
    }
  }
  
  // Verify actual state matches our intended state
  bool actualState = Matter.isWiFiNetworkCommissioningEnabled();
  if (actualState != wifiCommissioningEnabled) {
    Serial.printf("⚠️ Warning: State mismatch - intended: %s, actual: %s\n", 
                  wifiCommissioningEnabled ? "enabled" : "disabled",
                  actualState ? "enabled" : "disabled");
    wifiCommissioningEnabled = actualState;  // Sync to actual state
  }
#else
  Serial.println("WiFi commissioning not supported (not compiled in)");
#endif
  printNetworkStatus();
}

void toggleThreadCommissioning() {
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  threadCommissioningEnabled = !threadCommissioningEnabled;
  
  if (threadCommissioningEnabled) {
    if (Matter.enableThreadNetworkCommissioning()) {
      Serial.println("✓ Thread commissioning enabled");
    } else {
      Serial.println("✗ Failed to enable Thread commissioning");
      threadCommissioningEnabled = false;  // Revert state
    }
  } else {
    if (Matter.disableThreadNetworkCommissioning()) {
      Serial.println("✓ Thread commissioning disabled");
    } else {
      Serial.println("✗ Failed to disable Thread commissioning");
      threadCommissioningEnabled = true;  // Revert state
    }
  }
  
  // Verify actual state matches our intended state
  bool actualState = Matter.isThreadNetworkCommissioningEnabled();
  if (actualState != threadCommissioningEnabled) {
    Serial.printf("⚠️ Warning: State mismatch - intended: %s, actual: %s\n", 
                  threadCommissioningEnabled ? "enabled" : "disabled",
                  actualState ? "enabled" : "disabled");
    threadCommissioningEnabled = actualState;  // Sync to actual state
  }
#else
  Serial.println("Thread commissioning not supported (not compiled in)");
#endif
  printNetworkStatus();
}

void setWiFiOnlyMode() {
  Serial.println("Setting WiFi-only commissioning mode...");
  wifiCommissioningEnabled = true;
  threadCommissioningEnabled = false;
  
  if (Matter.setNetworkCommissioningMode(wifiCommissioningEnabled, threadCommissioningEnabled)) {
    Serial.println("✓ WiFi-only mode set successfully");
  } else {
    Serial.println("✗ Failed to set WiFi-only mode");
    // Sync our state with actual API state
    syncNetworkStates();
  }
  printNetworkStatus();
}

void setThreadOnlyMode() {
  Serial.println("Setting Thread-only commissioning mode...");
  wifiCommissioningEnabled = false;
  threadCommissioningEnabled = true;
  
  if (Matter.setNetworkCommissioningMode(wifiCommissioningEnabled, threadCommissioningEnabled)) {
    Serial.println("✓ Thread-only mode set successfully");
  } else {
    Serial.println("✗ Failed to set Thread-only mode");
    // Sync our state with actual API state
    syncNetworkStates();
  }
  printNetworkStatus();
}

void setBothNetworksMode() {
  Serial.println("Setting both networks commissioning mode...");
  wifiCommissioningEnabled = true;
  threadCommissioningEnabled = true;
  
  if (Matter.setNetworkCommissioningMode(wifiCommissioningEnabled, threadCommissioningEnabled)) {
    Serial.println("✓ Both networks mode set successfully");
  } else {
    Serial.println("✗ Failed to set both networks mode");
    // Sync our state with actual API state
    syncNetworkStates();
  }
  printNetworkStatus();
}

void setNoNetworksMode() {
  Serial.println("Disabling all network commissioning...");
  wifiCommissioningEnabled = false;
  threadCommissioningEnabled = false;
  
  if (Matter.setNetworkCommissioningMode(wifiCommissioningEnabled, threadCommissioningEnabled)) {
    Serial.println("✓ All networks disabled successfully");
  } else {
    Serial.println("✗ Failed to disable all networks");
    // Sync our state with actual API state
    syncNetworkStates();
  }
  printNetworkStatus();
}

void resetToFactory() {
  Serial.println("Resetting to factory defaults...");
  Matter.decommission();
  Serial.println("✓ Factory reset completed. Device will restart.");
  delay(1000);
  ESP.restart();
}

void syncNetworkStates() {
  // Synchronize our local state variables with the actual API state
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  wifiCommissioningEnabled = Matter.isWiFiNetworkCommissioningEnabled();
#else
  wifiCommissioningEnabled = false;
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  threadCommissioningEnabled = Matter.isThreadNetworkCommissioningEnabled();
#else
  threadCommissioningEnabled = false;
#endif

  Serial.println("🔄 Network states synchronized with API");
}

void printNetworkStatus() {
  Serial.println("\n--- Network Commissioning Status ---");
  
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  bool wifiEnabled = Matter.isWiFiNetworkCommissioningEnabled();
  bool wifiConnected = Matter.isWiFiConnected();
  Serial.printf("WiFi Commissioning: %s", wifiEnabled ? "ENABLED" : "DISABLED");
  if (wifiEnabled && wifiConnected) {
    Serial.print(" (Connected)");
  }
  Serial.println();
#else
  Serial.println("WiFi Commissioning: NOT SUPPORTED");
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
  bool threadEnabled = Matter.isThreadNetworkCommissioningEnabled();
  bool threadConnected = Matter.isThreadConnected();
  Serial.printf("Thread Commissioning: %s", threadEnabled ? "ENABLED" : "DISABLED");
  if (threadEnabled && threadConnected) {
    Serial.print(" (Connected)");
  }
  Serial.println();
#else
  Serial.println("Thread Commissioning: NOT SUPPORTED");
#endif

  Serial.printf("Device Commissioned: %s\n", Matter.isDeviceCommissioned() ? "YES" : "NO");
  Serial.printf("Device Connected: %s\n", Matter.isDeviceConnected() ? "YES" : "NO");
  Serial.println("----------------------------------\n");
}

void printCommands() {
  Serial.println("\n--- Available Commands ---");
  Serial.println("w/W : Toggle WiFi commissioning");
  Serial.println("t/T : Toggle Thread commissioning");
  Serial.println("r/R : Reset to factory defaults");
  Serial.println("s/S : Show network status");
  Serial.println("1   : WiFi-only mode");
  Serial.println("2   : Thread-only mode");
  Serial.println("3   : Both networks mode");
  Serial.println("0   : No networks mode");
  Serial.println("h/H/? : Show this help");
  Serial.println("-------------------------\n");
}

// Matter event callback
void onMatterEvent(matterEvent_t event, const ChipDeviceEvent *eventData) {
  switch (event) {
    case MATTER_COMMISSIONING_COMPLETE:
      Serial.println("🎉 Matter commissioning completed!");
      printNetworkStatus();
      break;
      
    case MATTER_COMMISSIONING_SESSION_STARTED:
      Serial.println("🔗 Matter commissioning session started");
      break;
      
    case MATTER_COMMISSIONING_SESSION_STOPPED:
      Serial.println("🔌 Matter commissioning session stopped");
      break;
      
    case MATTER_WIFI_CONNECTIVITY_CHANGE:
      Serial.println("📶 WiFi connectivity changed");
      printNetworkStatus();
      break;
      
    case MATTER_THREAD_CONNECTIVITY_CHANGE:
      Serial.println("🕸️ Thread connectivity changed");
      printNetworkStatus();
      break;
      
    case MATTER_CHIPOBLE_CONNECTION_ESTABLISHED:
      Serial.println("📱 CHIPoBLE connection established");
      break;
      
    case MATTER_CHIPOBLE_CONNECTION_CLOSED:
      Serial.println("📱 CHIPoBLE connection closed");
      break;
      
    default:
      break;
  }
}
