#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

#define OT_CHANNEL "24"
#define OT_NETWORK_KEY "00112233445566778899aabbccddeeff"
#define OT_MCAST_ADDR "ff05::abcd"
#define OT_COAP_RESOURCE_NAME "Lamp"

const char *otSetupLeader[] = {
  // clear/disable all
  "coap",  "stop",
  "thread", "stop",
  "ifconfig",  "down",
  "dataset", "clear",
  // set dataset
  "dataset", "init new",
  "dataset channel", OT_CHANNEL,
  "dataset networkkey", OT_NETWORK_KEY,
  "dataset", "commit active",
  // network start
  "ifconfig", "up",
  "thread", "start"
};

const char *otCoapLamp[] = {
  // create a multicast IPv6 Address for this device
  "ipmaddr add", OT_MCAST_ADDR,
  // start and create a CoAP resource
  "coap", "start",
  "coap resource", OT_COAP_RESOURCE_NAME,
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
  uint8_t tries = 24; // 24 x 2.5 sec = 1 min
  while (tries && getOtDeviceRole() != expectedRole) {
    Serial.print(".");
    delay(2500);
    tries--;
  }
  Serial.println();
  if (!tries) {
    log_e("Sorry, Device Role failed by timeout! Current Role: %s.", getStringOtDeviceRole());
    neopixelWrite(RGB_BUILTIN, 255, 0, 0);  // RED ... failed!
    return false;
  }
  Serial.printf("Device is %s.\r\n", getStringOtDeviceRole());
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
  neopixelWrite(RGB_BUILTIN, 0, 64, 8);   // GREEN ... Lamp is ready!
  return true;
}

void setupNode() {
  // tries to set the Thread Network node and only returns when succeded
  bool startedCorrectly = false;
  while (!startedCorrectly) {
    startedCorrectly |= otDeviceSetup(otSetupLeader, sizeof(otSetupLeader) / sizeof(char *) / 2,
                                      otCoapLamp, sizeof(otCoapLamp) / sizeof(char *) / 2,
                                      OT_ROLE_LEADER);
    if (!startedCorrectly) {
      Serial.println("Setup Failed...\r\nTrying again...");
    }
  }

}

// this function is used by the Lamp mode to listen for CoAP frames from the Switch Node
void otCOAPListen() {
  // waits for the client to send a CoAP request
  char cliResp[256];
  size_t len = OThreadCLI.readBytesUntil('\n', cliResp, sizeof(cliResp));
  cliResp[len] = '\0';
  if (strlen(cliResp)) {
    String sResp(cliResp);
    log_d("Msg[%s]", cliResp);
    if (sResp.startsWith("coap request from") && sResp.indexOf("PUT") > 0) {
      uint16_t payloadIdx = sResp.indexOf("payload: ") + 10;  // 0x30 | 0x31
      char payload = sResp.charAt(payloadIdx);
      log_i("CoAP PUT [%s]\r\n", payload == '0' ? "OFF" : "ON");
      if (payload == '0') {
        for (int16_t c = 248; c > 16; c -= 8) {
          neopixelWrite(RGB_BUILTIN, c, c, c); // ramp down
          delay(5);
        }
        neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Lamp Off
      } else {
        for (int16_t c = 16; c < 248; c += 8) {
          neopixelWrite(RGB_BUILTIN, c, c, c); // ramp up
          delay(5);
        }
        neopixelWrite(RGB_BUILTIN, 255, 255, 255); // Lamp On
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  // LED starts RED, indicating not connected to Thread network.
  neopixelWrite(RGB_BUILTIN, 64, 0, 0);
  OThreadCLI.begin(false); // No AutoStart is necessary
  OThreadCLI.setTimeout(250); // waits 250ms for the OpenThread CLI response
  setupNode();
}

void loop() {
  otCOAPListen();
  delay(10);
}
