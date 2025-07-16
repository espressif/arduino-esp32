#include "OThread.h"

OpenThread threadChildNode;
DataSet dataset;

void setup() {
  Serial.begin(115200);

  // Start OpenThread Stack - false for not using NVS dataset information
  threadChildNode.begin(false);

  // clear dataset
  dataset.clear();
  // Configure the dataset with the same Network Key of the Leader Node
  uint8_t networkKey[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  dataset.setNetworkKey(networkKey);

  // Apply the dataset and start the network
  threadChildNode.commitDataSet(dataset);
  threadChildNode.networkInterfaceUp();
  threadChildNode.start();
}

void loop() {
  // Get current device role
  ot_device_role_t currentRole = threadChildNode.otGetDeviceRole();

  // Only print detailed network information when node is active
  if (currentRole != OT_ROLE_DETACHED && currentRole != OT_ROLE_DISABLED) {
    Serial.println("==============================================");
    Serial.println("OpenThread Network Information (Active Dataset):");

    // Get and display the current active dataset
    const DataSet &activeDataset = threadChildNode.getCurrentDataSet();

    Serial.printf("Role: %s\r\n", threadChildNode.otGetStringDeviceRole());
    Serial.printf("RLOC16: 0x%04x\r\n", threadChildNode.getRloc16());

    // Dataset information
    Serial.printf("Network Name: %s\r\n", activeDataset.getNetworkName());
    Serial.printf("Channel: %d\r\n", activeDataset.getChannel());
    Serial.printf("PAN ID: 0x%04x\r\n", activeDataset.getPanId());

    // Extended PAN ID from dataset
    const uint8_t *extPanId = activeDataset.getExtendedPanId();
    if (extPanId) {
      Serial.print("Extended PAN ID: ");
      for (int i = 0; i < OT_EXT_PAN_ID_SIZE; i++) {
        Serial.printf("%02x", extPanId[i]);
      }
      Serial.println();
    }

    // Network Key from dataset
    const uint8_t *networkKey = activeDataset.getNetworkKey();
    if (networkKey) {
      Serial.print("Network Key: ");
      for (int i = 0; i < OT_NETWORK_KEY_SIZE; i++) {
        Serial.printf("%02x", networkKey[i]);
      }
      Serial.println();
    }

    // Additional runtime information
    IPAddress meshLocalEid = threadChildNode.getMeshLocalEid();
    Serial.printf("Mesh Local EID: %s\r\n", meshLocalEid.toString().c_str());

    IPAddress nodeRloc = threadChildNode.getRloc();
    Serial.printf("Node RLOC: %s\r\n", nodeRloc.toString().c_str());

    IPAddress leaderRloc = threadChildNode.getLeaderRloc();
    Serial.printf("Leader RLOC: %s\r\n", leaderRloc.toString().c_str());

    Serial.println();

  } else {
    Serial.println("==============================================");
    Serial.printf("Thread Node Status: %s - Waiting for thread network start...\r\n", threadChildNode.otGetStringDeviceRole());

    Serial.println();
  }

  delay(5000);
}
