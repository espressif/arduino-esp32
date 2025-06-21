#include "OThread.h"

OpenThread threadLeaderNode;
DataSet dataset;

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
  // Print network information every 5 seconds
  Serial.println("==============================================");
  threadLeaderNode.otPrintNetworkInformation(Serial);
  delay(5000);
}
