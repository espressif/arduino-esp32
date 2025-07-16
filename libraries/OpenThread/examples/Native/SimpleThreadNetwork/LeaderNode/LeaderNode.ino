#include "OThread.h"

OpenThread threadLeaderNode;
DataSet dataset;

// Track last known device role for state change detection
ot_device_role_t lastKnownRole = OT_ROLE_DISABLED;

void setup() {
  Serial.begin(115200);

  // Start OpenThread Stack - false for not using NVS dataset information
  threadLeaderNode.begin(false);

  // Create a new Thread Network Dataset for a Leader Node
  dataset.initNew();
  // Configure the dataset
  dataset.setNetworkName("ESP_OpenThread");
  uint8_t extPanId[OT_EXT_PAN_ID_SIZE] = {0xDE, 0xAD, 0x00, 0xBE, 0xEF, 0x00, 0xCA, 0xFE};
  dataset.setExtendedPanId(extPanId);
  uint8_t networkKey[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  dataset.setNetworkKey(networkKey);
  dataset.setChannel(15);
  dataset.setPanId(0x1234);

  // Apply the dataset and start the network
  threadLeaderNode.commitDataSet(dataset);
  threadLeaderNode.networkInterfaceUp();
  threadLeaderNode.start();
}

void loop() {
  // Get current device role
  ot_device_role_t currentRole = threadLeaderNode.otGetDeviceRole();

  // Only print network information when not detached
  if (currentRole != OT_ROLE_DETACHED && currentRole != OT_ROLE_DISABLED) {
    Serial.println("==============================================");
    Serial.println("OpenThread Network Information:");

    // Basic network information
    Serial.printf("Role: %s\r\n", threadLeaderNode.otGetStringDeviceRole());
    Serial.printf("RLOC16: 0x%04x\r\n", threadLeaderNode.getRloc16());
    Serial.printf("Network Name: %s\r\n", threadLeaderNode.getNetworkName().c_str());
    Serial.printf("Channel: %d\r\n", threadLeaderNode.getChannel());
    Serial.printf("PAN ID: 0x%04x\r\n", threadLeaderNode.getPanId());

    // Extended PAN ID
    const uint8_t *extPanId = threadLeaderNode.getExtendedPanId();
    if (extPanId) {
      Serial.print("Extended PAN ID: ");
      for (int i = 0; i < OT_EXT_PAN_ID_SIZE; i++) {
        Serial.printf("%02x", extPanId[i]);
      }
      Serial.println();
    }

    // Network Key
    const uint8_t *networkKey = threadLeaderNode.getNetworkKey();
    if (networkKey) {
      Serial.print("Network Key: ");
      for (int i = 0; i < OT_NETWORK_KEY_SIZE; i++) {
        Serial.printf("%02x", networkKey[i]);
      }
      Serial.println();
    }

    // Mesh Local EID
    IPAddress meshLocalEid = threadLeaderNode.getMeshLocalEid();
    Serial.printf("Mesh Local EID: %s\r\n", meshLocalEid.toString().c_str());

    // Leader RLOC
    IPAddress leaderRloc = threadLeaderNode.getLeaderRloc();
    Serial.printf("Leader RLOC: %s\r\n", leaderRloc.toString().c_str());

    // Node RLOC
    IPAddress nodeRloc = threadLeaderNode.getRloc();
    Serial.printf("Node RLOC: %s\r\n", nodeRloc.toString().c_str());

    // Demonstrate address listing with two different methods:
    // Method 1: Unicast addresses using counting API (individual access)
    Serial.println("\r\n--- Unicast Addresses (Using Count + Index API) ---");
    size_t unicastCount = threadLeaderNode.getUnicastAddressCount();
    for (size_t i = 0; i < unicastCount; i++) {
      IPAddress addr = threadLeaderNode.getUnicastAddress(i);
      Serial.printf("  [%zu]: %s\r\n", i, addr.toString().c_str());
    }

    // Method 2: Multicast addresses using std::vector (bulk access)
    Serial.println("\r\n--- Multicast Addresses (Using std::vector API) ---");
    std::vector<IPAddress> allMulticast = threadLeaderNode.getAllMulticastAddresses();
    for (size_t i = 0; i < allMulticast.size(); i++) {
      Serial.printf("  [%zu]: %s\r\n", i, allMulticast[i].toString().c_str());
    }

    // Check for role change and clear cache if needed (only when active)
    if (currentRole != lastKnownRole) {
      Serial.printf(
        "Role changed from %s to %s - clearing address cache\r\n", (lastKnownRole < 5) ? otRoleString[lastKnownRole] : "Unknown",
        threadLeaderNode.otGetStringDeviceRole()
      );
      threadLeaderNode.clearAllAddressCache();
      lastKnownRole = currentRole;
    }
  } else {
    Serial.printf("Thread Node Status: %s - Waiting for thread network start...\r\n", threadLeaderNode.otGetStringDeviceRole());

    // Update role tracking even when detached/disabled, but don't clear cache
    lastKnownRole = currentRole;
  }

  delay(5000);
}
