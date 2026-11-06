/*
 * Joiner-based Simple Thread Network - Joiner Node.
 *
 * Companion of CommissionerNode/CommissionerNode.ino. This sketch does NOT know the
 * network key, the network name, or the PAN ID up front - it learns them
 * from the Commissioner Node via the Thread Joiner role.
 *
 * Workflow:
 *   1. begin(false)              - start OT stack, do not commit any DataSet.
 *   2. setChannel(THREAD_CHANNEL)- hint the radio channel so the Joiner does
 *                                  not have to scan all of them. Optional
 *                                  (set THREAD_CHANNEL to 0 to scan instead).
 *   3. networkInterfaceUp()      - IPv6 stack up.
 *   4. startJoiner(PSKD)         - blocks until commissioning completes.
 *   5. start()                   - enable Thread protocol with the dataset
 *                                  we just received from the commissioner.
 */

#include <Arduino.h>
#include "OThread.h"

OpenThread threadJoinerNode;

// Must match the commissioner side (CommissionerNode sketch).
const char PSKD[] = "J01NME";

// IEEE 802.15.4 channel the Joiner will use to attach.
//
// Define THREAD_CHANNEL (11..26) to pre-set the radio channel so the Joiner
// skips the (slow) full channel scan and attaches faster. It must match the
// channel the CommissionerNode formed the network on (its own
// THREAD_CHANNEL). Set THREAD_CHANNEL to 0 to let the Joiner scan every
// channel in the supported mask instead. Override at build time with
// -DTHREAD_CHANNEL=<11..26>.
#ifndef THREAD_CHANNEL
#define THREAD_CHANNEL 15
#endif

// How long to wait for the commissioner to finalize the join. 30 s is enough
// for most setups; bump it up if your network is busy or far away.
const uint32_t JOIN_TIMEOUT_MS = 30000;

bool joined = false;
ot_device_role_t lastKnownRole = OT_ROLE_DISABLED;

void setup() {
  Serial.begin(115200);

  Serial.println("=== Joiner Demo - Joiner Node ===");

  // Start OpenThread stack WITHOUT any locally-configured DataSet. The
  // dataset will be installed by the commissioner during the Joiner finalize
  // step, so we must not commit one ourselves.
  threadJoinerNode.begin(false);

  // Optional channel hint: lets the joiner skip the channel scan and attach
  // faster. With THREAD_CHANNEL == 0 the hint is skipped and the joiner
  // discovers the channel automatically by scanning the supported mask.
#if THREAD_CHANNEL
  threadJoinerNode.setChannel(THREAD_CHANNEL);
#endif

  // The IPv6 interface MUST be up before calling startJoiner().
  threadJoinerNode.networkInterfaceUp();

  Serial.print("EUI-64 (Joiner Id input): ");
  Serial.println(threadJoinerNode.getEui64());
  Serial.print("Joining with PSKd: ");
  Serial.println(PSKD);

  // Run the Joiner state machine. Retries on transient failures so the user
  // can power-up the CommissionerNode at any time.
  while (!joined) {
    otError err = threadJoinerNode.startJoiner(PSKD, JOIN_TIMEOUT_MS);
    if (err == OT_ERROR_NONE) {
      Serial.println("Joiner: commissioning SUCCESS.");
      joined = true;
      break;
    }
    switch (err) {
      case OT_ERROR_SECURITY:         Serial.println("Joiner: PSKd mismatch. Check CommissionerNode PSKD."); break;
      case OT_ERROR_NOT_FOUND:        Serial.println("Joiner: no joinable network found. Is the CommissionerNode running?"); break;
      case OT_ERROR_RESPONSE_TIMEOUT: Serial.println("Joiner: commissioner did not respond in time."); break;
      default:                        Serial.printf("Joiner: error %d.\r\n", err); break;
    }
    delay(2000);
  }

  // The active dataset has now been provisioned by the commissioner.
  // Enable the Thread protocol to start participating in the mesh.
  threadJoinerNode.start();

  Serial.println("Thread enabled. Waiting to attach...");
}

void loop() {
  ot_device_role_t currentRole = threadJoinerNode.otGetDeviceRole();

  if (currentRole != OT_ROLE_DETACHED && currentRole != OT_ROLE_DISABLED) {
    Serial.println("==============================================");
    Serial.println("OpenThread Network Information (post-join):");

    const DataSet &activeDataset = threadJoinerNode.getCurrentDataSet();

    Serial.printf("Role:           %s\r\n", threadJoinerNode.otGetStringDeviceRole());
    Serial.printf("RLOC16:         0x%04x\r\n", threadJoinerNode.getRloc16());
    Serial.printf("Network Name:   %s\r\n", activeDataset.getNetworkName());
    Serial.printf("Channel:        %u\r\n", activeDataset.getChannel());
    Serial.printf("PAN ID:         0x%04x\r\n", activeDataset.getPanId());

    const uint8_t *xp = activeDataset.getExtendedPanId();
    if (xp) {
      Serial.print("Extended PAN:   ");
      for (int i = 0; i < OT_EXT_PAN_ID_SIZE; i++) {
        Serial.printf("%02x", xp[i]);
      }
      Serial.println();
    }

    const uint8_t *nk = activeDataset.getNetworkKey();
    if (nk) {
      Serial.print("Network Key:    ");
      for (int i = 0; i < OT_NETWORK_KEY_SIZE; i++) {
        Serial.printf("%02x", nk[i]);
      }
      Serial.println();
    }

    Serial.printf("Mesh Local EID: %s\r\n", threadJoinerNode.getMeshLocalEid().toString().c_str());
    Serial.printf("Node RLOC:      %s\r\n", threadJoinerNode.getRloc().toString().c_str());
    Serial.printf("Leader RLOC:    %s\r\n", threadJoinerNode.getLeaderRloc().toString().c_str());

    if (currentRole != lastKnownRole) {
      threadJoinerNode.clearAllAddressCache();
      lastKnownRole = currentRole;
    }
  } else {
    Serial.printf("Status: %s - attaching after join...\r\n", threadJoinerNode.otGetStringDeviceRole());
    lastKnownRole = currentRole;
  }

  delay(5000);
}
