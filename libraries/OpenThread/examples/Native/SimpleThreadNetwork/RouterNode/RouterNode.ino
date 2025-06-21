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
  // Print network information every 5 seconds
  Serial.println("==============================================");
  threadChildNode.otPrintNetworkInformation(Serial);
  delay(5000);
}
