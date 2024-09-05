#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

// Leader node shall use the same Network Key and channel
#define CLI_NETWORK_KEY    "dataset networkkey 00112233445566778899aabbccddeeff"
#define CLI_NETWORK_CHANEL "dataset channel 24"
bool otStatus = true;

void setup() {
  Serial.begin(115200);
  OThreadCLI.begin(false);  // No AutoStart - fresh start
  Serial.println("Setting up OpenThread Node as Router/Child");
  Serial.println("Make sure the Leader Node is already running");

  // This sketch will use CLI responses, therefore, it shall always process each CLI return
  char resp[256];
  otStatus &= otGetRespCmd("dataset clear", resp);
  otStatus &= otGetRespCmd(CLI_NETWORK_KEY, resp);
  otStatus &= otGetRespCmd(CLI_NETWORK_CHANEL, resp);
  otStatus &= otGetRespCmd("dataset commit active", resp);
  otStatus &= otGetRespCmd("ifconfig up", resp);
  otStatus &= otGetRespCmd("thread start", resp);

  if (!otStatus) {
    Serial.println("\r\n\t===> Failed starting Thread Network!");
    return;
  }
  // wait for the node to enter in the router state
  uint32_t timeout = millis() + 90000; // waits 90 seconds to
  while (otGetDeviceRole() != OT_ROLE_CHILD && otGetDeviceRole() != OT_ROLE_ROUTER) {
    Serial.print(".");
    if (millis() > timeout) {
      Serial.println("\r\n\t===> Timeout! Failed.");
      otStatus = false;
      break;
    }
    delay(500);
  }

  if (otStatus) {
    // print the PanID using 2 methods

    // CLI
    if (otGetRespCmd("panid", resp)) {
      Serial.printf("\r\nPanID[using CLI]: %s\r\n", resp);
    } else {
      Serial.printf("\r\nPanID[using CLI]: FAILED!\r\n");
    }

    // OpenThread API
    Serial.printf("PanID[using OT API]: 0x%x\r\n", (uint16_t) otLinkGetPanId(esp_openthread_get_instance()));
  }
  Serial.println("\r\n");
}

void loop() {
  if (otStatus) {
    Serial.println("Thread NetworkInformation: ");
    Serial.println("---------------------------");
    otPrintNetworkInformation(Serial);
    Serial.println("---------------------------");
  } else {
    Serial.println("Some OpenThread operation has failed...");
  }
  delay(10000);
}
