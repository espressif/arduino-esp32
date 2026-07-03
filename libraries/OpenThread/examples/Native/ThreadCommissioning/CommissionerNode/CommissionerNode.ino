/*
 * Joiner-based Simple Thread Network - Commissioner Node.
 *
 * Companion of JoinerNode/JoinerNode.ino. This sketch shows how to bring up
 * a Thread network with a locally-defined Operational Dataset, become the
 * network Commissioner, and allow another device to attach purely with a
 * PSKd, without ever knowing the network key in advance.
 *
 * IMPORTANT: becoming Commissioner is not the same as accepting joiners.
 * After this sketch prints "Press the button ...", press JOIN_BUTTON_PIN
 * (BOOT_PIN by default) to open the commissioning/joiner listening window.
 * Until the button is pressed, the JoinerNode will not be authorized.
 *
 * Workflow:
 *   1. Configure and commit an Operational Dataset.
 *   2. networkInterfaceUp() + start() -> become Leader.
 *   3. Wait until we are Leader (or attached as Router/Child).
 *   4. startCommissioner() -> blocks until OT_COMMISSIONER_STATE_ACTIVE.
 *   5. Press the button (JOIN_BUTTON_PIN) -> addJoiner(PSKd) opens the joiner
 *      window, accepting any joiner presenting the PSKd for JOINER_WINDOW_SEC.
 *   6. The companion JoinerNode sketch runs startJoiner(PSKd).
 */

#include <Arduino.h>
#include "OThread.h"

OpenThread threadCommissionerNode;
DataSet dataset;

// Pre-Shared Key for Device. Joiners must use the same string.
const char PSKD[] = "J01NME";

// How long (seconds) the joiner entry stays in the commissioner table.
// Plenty of time for a single joiner to attach; reduce in production.
const uint32_t JOINER_WINDOW_SEC = 120;

// Default IEEE 802.15.4 channel used to form the Thread network.
//
// Setting a channel here is optional: dataset.initNew() already selects a
// valid (essentially random) channel, so the network would still form without
// it. We pin it so it matches the Joiner side (JoinerNode's THREAD_CHANNEL),
// letting the Joiner attach without scanning every channel. Must be 11..26.
// Override at build time with -DTHREAD_CHANNEL=<11..26>.
#ifndef THREAD_CHANNEL
#define THREAD_CHANNEL 15
#endif

// GPIO of the button that opens the joiner window (starts the "Joiner
// Listener"). Defaults to BOOT_PIN, provided per-board by Arduino.h
// (esp32-hal.h): GPIO9 on C6/H2, GPIO28 on C5. The BOOT button is active-low.
// Override with -DJOIN_BUTTON_PIN=<gpio>.
#ifndef JOIN_BUTTON_PIN
#define JOIN_BUTTON_PIN BOOT_PIN
#endif

ot_device_role_t lastKnownRole = OT_ROLE_DISABLED;
bool commissionerStarted = false;
unsigned long lastStatusMs = 0;

// Detects a press (falling edge) on the active-low button, with a small
// debounce. Returns true once per physical press.
bool joinButtonPressed() {
  static int lastLevel = HIGH;
  int level = digitalRead(JOIN_BUTTON_PIN);
  bool pressed = (lastLevel == HIGH && level == LOW);
  lastLevel = level;
  if (pressed) {
    delay(50);  // debounce
  }
  return pressed;
}

void setup() {
  Serial.begin(115200);

  Serial.println("=== Joiner Demo - Commissioner Node ===");

  // The button (active-low) is used to open the joiner window at runtime.
  pinMode(JOIN_BUTTON_PIN, INPUT_PULLUP);

  // Start OpenThread Stack - false = do not auto-load NVS dataset
  threadCommissionerNode.begin(false);

  // Build the active operational dataset locally. The Joiner will receive
  // it from us during commissioning - it does not need to know any of these
  // values up front.
  dataset.initNew();
  dataset.setNetworkName("ESP_OT_Joiner");
  uint8_t extPanId[OT_EXT_PAN_ID_SIZE] = {0xDE, 0xAD, 0x00, 0xBE, 0xEF, 0x00, 0xCA, 0xFE};
  dataset.setExtendedPanId(extPanId);
  uint8_t networkKey[OT_NETWORK_KEY_SIZE] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  dataset.setNetworkKey(networkKey);
  // Pin the channel so it matches the Joiner's THREAD_CHANNEL hint. Optional:
  // initNew() already picked a valid channel, but pinning avoids a Joiner scan.
  dataset.setChannel(THREAD_CHANNEL);
  dataset.setPanId(0x1234);

  threadCommissionerNode.commitDataSet(dataset);
  threadCommissionerNode.networkInterfaceUp();
  threadCommissionerNode.start();

  Serial.println("Thread network started, waiting to become Leader...");
}

void loop() {
  ot_device_role_t currentRole = threadCommissionerNode.otGetDeviceRole();
  bool attached = (currentRole != OT_ROLE_DETACHED && currentRole != OT_ROLE_DISABLED);

  // Petition the Commissioner role once we are attached (Leader / Router /
  // Child). The joiner window itself is opened later, on a button press.
  if (!commissionerStarted && attached) {
    Serial.printf("Attached as %s. Petitioning Commissioner...\r\n", threadCommissionerNode.otGetStringDeviceRole());

    otError err = threadCommissionerNode.startCommissioner(/*timeoutMs=*/30000);
    if (err != OT_ERROR_NONE) {
      Serial.printf("Commissioner petition failed (err=%d). Retrying in 5 s...\r\n", err);
      delay(5000);
      return;
    }
    commissionerStarted = true;
    Serial.println("Commissioner ACTIVE.");
    Serial.println();
    Serial.printf("===>>>Press the button on GPIO %u to open the joiner window.\r\n", JOIN_BUTTON_PIN);
    Serial.println();
    delay(3000);
  }

  // Start the "Joiner Listener" on demand: pressing the button opens (or
  // re-opens, after it expires) the joiner window for JOINER_WINDOW_SEC.
  if (commissionerStarted && joinButtonPressed()) {
    otError err = threadCommissionerNode.addJoiner(PSKD, JOINER_WINDOW_SEC);
    if (err == OT_ERROR_NONE) {
      Serial.printf("Joiner window OPEN: PSKd \"%s\" accepted for %lu s.\r\n", PSKD, (unsigned long)JOINER_WINDOW_SEC);
      Serial.println("Bring up the JoinerNode sketch now.");
    } else {
      Serial.printf("addJoiner failed (err=%d).\r\n", err);
    }
  }

  // Periodically dump status without blocking button handling.
  if (millis() - lastStatusMs < 5000) {
    delay(20);  // keep the button responsive
    return;
  }
  lastStatusMs = millis();

  if (attached) {
    Serial.println("==============================================");
    Serial.printf("Role:           %s\r\n", threadCommissionerNode.otGetStringDeviceRole());
    Serial.printf("RLOC16:         0x%04x\r\n", threadCommissionerNode.getRloc16());
    Serial.printf("Network Name:   %s\r\n", threadCommissionerNode.getNetworkName().c_str());
    Serial.printf("Channel:        %u\r\n", threadCommissionerNode.getChannel());
    Serial.printf("PAN ID:         0x%04x\r\n", threadCommissionerNode.getPanId());

    const uint8_t *xp = threadCommissionerNode.getExtendedPanId();
    if (xp) {
      Serial.print("Extended PAN:   ");
      for (int i = 0; i < OT_EXT_PAN_ID_SIZE; i++) {
        Serial.printf("%02x", xp[i]);
      }
      Serial.println();
    }

    Serial.printf("Mesh Local EID: %s\r\n", threadCommissionerNode.getMeshLocalEid().toString().c_str());
    Serial.printf("Leader RLOC:    %s\r\n", threadCommissionerNode.getLeaderRloc().toString().c_str());
    Serial.printf("Node RLOC:      %s\r\n", threadCommissionerNode.getRloc().toString().c_str());

    const char *commState = "DISABLED";
    switch (threadCommissionerNode.getCommissionerState()) {
      case OT_COMMISSIONER_STATE_PETITION: commState = "PETITION"; break;
      case OT_COMMISSIONER_STATE_ACTIVE:   commState = "ACTIVE"; break;
      default:                             break;
    }
    Serial.printf("Commissioner:   %s\r\n", commState);

    if (currentRole != lastKnownRole) {
      Serial.printf(
        "Role changed: %s -> %s (clearing address cache)\r\n", (lastKnownRole < 5) ? otRoleString[lastKnownRole] : "Unknown",
        threadCommissionerNode.otGetStringDeviceRole()
      );
      threadCommissionerNode.clearAllAddressCache();
      lastKnownRole = currentRole;
    }
  } else {
    Serial.printf("Status: %s - waiting for network start...\r\n", threadCommissionerNode.otGetStringDeviceRole());
    lastKnownRole = currentRole;
  }
}
