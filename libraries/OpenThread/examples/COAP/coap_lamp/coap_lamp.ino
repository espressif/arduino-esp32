#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

#define OT_CHANNEL            "24"
#define OT_NETWORK_KEY        "00112233445566778899aabbccddeeff"
#define OT_MCAST_ADDR         "ff05::abcd"
#define OT_COAP_RESOURCE_NAME "Lamp"

const char *otSetupLeader[] = {
  // -- clear/disable all
  // stop CoAP
  "coap", "stop",
  // stop Thread
  "thread", "stop",
  // stop the interface
  "ifconfig", "down",
  // clear the dataset
  "dataset", "clear",
  // -- set dataset
  // create a new complete dataset with random data
  "dataset", "init new",
  // set the channel
  "dataset channel", OT_CHANNEL,
  // set the network key
  "dataset networkkey", OT_NETWORK_KEY,
  // commit the dataset
  "dataset", "commit active",
  // -- network start
  // start the interface
  "ifconfig", "up",
  // start the Thread network
  "thread", "start"
};

const char *otCoapLamp[] = {
  // -- create a multicast IPv6 Address for this device
  "ipmaddr add", OT_MCAST_ADDR,
  // -- start and create a CoAP resource
  // start CoAP as server
  "coap", "start",
  // create a CoAP resource
  "coap resource", OT_COAP_RESOURCE_NAME,
  // set the CoAP resource initial value
  "coap set", "0"
};

bool otDeviceSetup(const char **otSetupCmds, uint8_t nCmds1, const char **otCoapCmds, uint8_t nCmds2, ot_device_role_t expectedRole) {
  Serial.println("Starting OpenThread.");
  Serial.println("Running as Lamp (RGB LED) - use the other C6/H2 as a Switch");
  uint8_t i;
  for (i = 0; i < nCmds1; i++) {
    if (!otExecCommand(otSetupCmds[i * 2], otSetupCmds[i * 2 + 1])) {
      break;
    }
  }
  if (i != nCmds1) {
    log_e("Sorry, OpenThread Network setup failed!");
    neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // RED ... failed!
    return false;
  }
  Serial.println("OpenThread started.\r\nWaiting for activating correct Device Role.");
  // wait for the expected Device Role to start
  uint8_t tries = 24;  // 24 x 2.5 sec = 1 min
  while (tries && otGetDeviceRole() != expectedRole) {
    Serial.print(".");
    delay(2500);
    tries--;
  }
  Serial.println();
  if (!tries) {
    log_e("Sorry, Device Role failed by timeout! Current Role: %s.", otGetStringDeviceRole());
    neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // RED ... failed!
    return false;
  }
  Serial.printf("Device is %s.\r\n", otGetStringDeviceRole());
  for (i = 0; i < nCmds2; i++) {
    if (!otExecCommand(otCoapCmds[i * 2], otCoapCmds[i * 2 + 1])) {
      break;
    }
  }
  if (i != nCmds2) {
    log_e("Sorry, OpenThread CoAP setup failed!");
    neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // RED ... failed!
    return false;
  }
  Serial.println("OpenThread setup done. Node is ready.");
  // all fine! LED goes Green
  neopixelWrite(RGB_BUILTIN, 0, 64, 8);  // GREEN ... Lamp is ready!
  return true;
}

void setupNode() {
  // tries to set the Thread Network node and only returns when succeded
  bool startedCorrectly = false;
  while (!startedCorrectly) {
    startedCorrectly |=
      otDeviceSetup(otSetupLeader, sizeof(otSetupLeader) / sizeof(char *) / 2, otCoapLamp, sizeof(otCoapLamp) / sizeof(char *) / 2, OT_ROLE_LEADER);
    if (!startedCorrectly) {
      Serial.println("Setup Failed...\r\nTrying again...");
    }
  }
}

// this function is used by the Lamp mode to listen for CoAP frames from the Switch Node
void otCOAPListen() {
  // waits for the client to send a CoAP request
  char cliResp[256] = {0};
  size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
  cliResp[len - 1] = '\0';
  if (strlen(cliResp)) {
    String sResp(cliResp);
    // cliResp shall be something like:
    // "coap request from fd0c:94df:f1ae:b39a:ec47:ec6d:15e8:804a PUT with payload: 30"
    // payload may be 30 or 31 (HEX) '0' or '1' (ASCII)
    log_d("Msg[%s]", cliResp);
    if (sResp.startsWith("coap request from") && sResp.indexOf("PUT") > 0) {
      char payload = sResp.charAt(sResp.length() - 1);  //  last character in the payload
      log_i("CoAP PUT [%s]\r\n", payload == '0' ? "OFF" : "ON");
      if (payload == '0') {
        for (int16_t c = 248; c > 16; c -= 8) {
          neopixelWrite(RGB_BUILTIN, c, c, c);  // ramp down
          delay(5);
        }
        neopixelWrite(RGB_BUILTIN, 0, 0, 0);  // Lamp Off
      } else {
        for (int16_t c = 16; c < 248; c += 8) {
          neopixelWrite(RGB_BUILTIN, c, c, c);  // ramp up
          delay(5);
        }
        neopixelWrite(RGB_BUILTIN, 255, 255, 255);  // Lamp On
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  // LED starts RED, indicating not connected to Thread network.
  neopixelWrite(RGB_BUILTIN, 64, 0, 0);
  OThreadCLI.begin(false);     // No AutoStart is necessary
  OThreadCLI.setTimeout(250);  // waits 250ms for the OpenThread CLI response
  setupNode();
  // LED goes Green when all is ready and Red when failed.
}

void loop() {
  otCOAPListen();
  delay(10);
}
