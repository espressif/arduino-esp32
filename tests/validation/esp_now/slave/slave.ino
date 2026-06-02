/*
 * ESP-NOW validation test – Slave device
 *
 * Phases mirror the master.  The slave:
 *   – registers an onNewPeer callback to capture the first broadcast (phase 1)
 *   – adds its own broadcast peer to send a broadcast in phase 2
 *   – uses the dynamically-created master_peer for all subsequent receives/sends
 */

#include <Arduino.h>
#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>

#define ESPNOW_WIFI_CHANNEL 1
#define ESPNOW_PMK          "pmk1234567890123" /* exactly 16 bytes */
#define ESPNOW_LMK          "lmk1234567890123" /* exactly 16 bytes */

/* ---------- Inheritable peer with callbacks and helpers ---------- */

class TestPeer : public ESP_NOW_Peer {
public:
  volatile bool sent_cb = false;
  volatile bool sent_ok = false;
  volatile bool recv_cb = false;
  volatile bool recv_bcast = false;
  volatile size_t recv_len = 0;
  uint8_t recv_buf[1470] = {};

  TestPeer(const uint8_t *mac, uint8_t ch, wifi_interface_t ifc, const uint8_t *lmk = nullptr) : ESP_NOW_Peer(mac, ch, ifc, lmk) {}
  ~TestPeer() {
    remove();
  }

  bool begin() {
    return add();
  }

  bool sendMsg(const uint8_t *data, size_t len) {
    sent_cb = false;
    sent_ok = false;
    return send(data, len) == len;
  }

  void onSent(bool success) override {
    sent_ok = success;
    sent_cb = true;
  }

  void onReceive(const uint8_t *data, size_t len, bool broadcast) override {
    recv_bcast = broadcast;
    size_t n = len < sizeof(recv_buf) ? len : sizeof(recv_buf);
    memcpy(recv_buf, data, n);
    recv_len = n;
    recv_cb = true;
  }

  bool waitSent(uint32_t ms = 5000) {
    uint32_t t = millis();
    while (!sent_cb) {
      if (millis() - t > ms) {
        return false;
      }
      delay(5);
    }
    return true;
  }

  bool waitRecv(uint32_t ms = 5000) {
    uint32_t t = millis();
    while (!recv_cb) {
      if (millis() - t > ms) {
        return false;
      }
      delay(5);
    }
    return true;
  }
};

/* ---------- Globals ---------- */

static uint8_t peer_mac[6] = {};        /* master's MAC (from serial exchange) */
static TestPeer *master_peer = nullptr; /* created inside onNewPeer callback    */
static TestPeer *bcast_peer = nullptr;  /* broadcast peer for phase 2           */
static volatile bool bcast_received = false;
static char bcast_msg[32] = {};

/* ---------- onNewPeer callback (fires for unknown senders) ---------- */

static void onNewPeer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
  if (master_peer != nullptr) {
    return; /* already registered */
  }
  /* Only accept broadcasts from the expected master MAC. */
  if (memcmp(info->des_addr, ESP_NOW.BROADCAST_ADDR, ESP_NOW_ETH_ALEN) != 0) {
    return;
  }
  if (memcmp(info->src_addr, peer_mac, ESP_NOW_ETH_ALEN) != 0) {
    return;
  }
  master_peer = new TestPeer(info->src_addr, ESPNOW_WIFI_CHANNEL, WIFI_IF_STA);
  if (!master_peer || !master_peer->begin()) {
    delete master_peer;
    master_peer = nullptr;
    return;
  }
  size_t n = (size_t)len < sizeof(bcast_msg) - 1 ? (size_t)len : sizeof(bcast_msg) - 1;
  memcpy(bcast_msg, data, n);
  bcast_msg[n] = '\0';
  bcast_received = true;
}

/* ---------- Helpers ---------- */

static bool parseMac(const String &s, uint8_t *mac) {
  unsigned int m[6];
  if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
    for (int i = 0; i < 6; i++) {
      mac[i] = (uint8_t)m[i];
    }
    return true;
  }
  return false;
}

/* Blocks until "START\n" is received from the test script. */
static void waitForStart(int phase) {
  Serial.printf("[SLAVE] Ready for phase %d\n", phase);
  while (true) {
    if (Serial.available()) {
      String s = Serial.readStringUntil('\n');
      s.trim();
      if (s == "START") {
        Serial.printf("[SLAVE] Phase %d started\n", phase);
        return;
      }
    }
    delay(10);
  }
}

/* ---------- setup ---------- */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  /* ---------- Startup synchronization ----------
   * Block until the test script is ready so that both DUTs start together
   * even when one device finishes flashing significantly before the other. */
  waitForStart(0);

  /* ---------- WiFi init ---------- */
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  /* ---------- MAC exchange with test script ---------- */
  uint8_t self_mac[6];
  WiFi.macAddress(self_mac);
  Serial.printf("[SLAVE] MAC: " MACSTR "\n", MAC2STR(self_mac));
  Serial.println("[SLAVE] Send peer MAC:");
  String mac_str = "";
  while (mac_str.length() == 0) {
    if (Serial.available()) {
      mac_str = Serial.readStringUntil('\n');
      mac_str.trim();
    }
    delay(10);
  }
  if (!parseMac(mac_str, peer_mac)) {
    Serial.println("[SLAVE] ERROR: invalid peer MAC");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("[SLAVE] Peer MAC: " MACSTR "\n", MAC2STR(peer_mac));

  /* ====================================================
   * Phase 1 – Init, register onNewPeer, wait for broadcast
   * ==================================================== */

  if (!ESP_NOW.begin((const uint8_t *)ESPNOW_PMK)) {
    Serial.println("[SLAVE] ERROR: begin() failed");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("[SLAVE] Version: %d\n", ESP_NOW.getVersion());
  Serial.printf("[SLAVE] Max data len: %d\n", ESP_NOW.getMaxDataLen());

  /* Register callback for unknown senders (fires on first broadcast). */
  ESP_NOW.onNewPeer(onNewPeer, nullptr);

  waitForStart(1);

  /* Wait for the broadcast from master, which triggers onNewPeer. */
  uint32_t t = millis();
  while (!bcast_received && millis() - t < 10000) {
    delay(10);
  }
  if (bcast_received && master_peer) {
    Serial.printf("[SLAVE] Received broadcast: %s\n", bcast_msg);
    Serial.printf("[SLAVE] master_peer bool (added): %s\n", (bool)*master_peer ? "true" : "false");
  } else {
    Serial.println("[SLAVE] ERROR: no broadcast received");
  }

  /* Add slave-side broadcast peer so we can send a broadcast in phase 2. */
  bcast_peer = new TestPeer(ESP_NOW.BROADCAST_ADDR, ESPNOW_WIFI_CHANNEL, WIFI_IF_STA);
  if (!bcast_peer->begin()) {
    Serial.println("[SLAVE] ERROR: failed to add broadcast peer");
    while (true) {
      delay(1000);
    }
  }

  /* ====================================================
   * Phase 2 – Slave broadcast + recv_bcast flag
   * ==================================================== */

  waitForStart(2);

  bcast_peer->sendMsg((const uint8_t *)"Hello from slave bcast", 22);
  if (bcast_peer->waitSent(5000)) {
    Serial.printf("[SLAVE] Broadcast sent: %s\n", bcast_peer->sent_ok ? "success" : "failed");
  } else {
    Serial.println("[SLAVE] Broadcast sent: timeout");
  }

  /* ====================================================
   * Phase 3 – Unicast master → slave
   * ==================================================== */

  master_peer->recv_cb = false;
  waitForStart(3);

  if (master_peer->waitRecv(10000)) {
    Serial.printf("[SLAVE] Received unicast: %.*s\n", (int)master_peer->recv_len, (char *)master_peer->recv_buf);
  } else {
    Serial.println("[SLAVE] ERROR: no unicast received");
  }

  /* ====================================================
   * Phase 4 – Unicast slave → master
   * ==================================================== */

  waitForStart(4);

  master_peer->sendMsg((const uint8_t *)"Phase4:S->M", 11);
  if (master_peer->waitSent(5000)) {
    Serial.printf("[SLAVE] Unicast to master: %s\n", master_peer->sent_ok ? "success" : "failed");
  } else {
    Serial.println("[SLAVE] Unicast to master: timeout");
  }

  /* ====================================================
   * Phase 5 – Peer management: getters and edge cases
   * ==================================================== */

  waitForStart(5);

  /* Slave has master_peer (from phase 1) + bcast_peer (from phase 2) = 2 total. */
  Serial.printf("[SLAVE] Total peers: %d\n", ESP_NOW.getTotalPeerCount());
  Serial.printf("[SLAVE] Encrypted peers: %d\n", ESP_NOW.getEncryptedPeerCount());
  Serial.printf("[SLAVE] Master channel: %u\n", master_peer->getChannel());
  Serial.printf("[SLAVE] Master interface: %d\n", (int)master_peer->getInterface());
  Serial.printf("[SLAVE] Master isEncrypted: %s\n", master_peer->isEncrypted() ? "true" : "false");

  /* operator bool(): true when added. */
  Serial.printf("[SLAVE] master_peer bool (added): %s\n", (bool)*master_peer ? "true" : "false");

  /* addr() getter: should return master's MAC. */
  Serial.printf("[SLAVE] Master addr: " MACSTR "\n", MAC2STR(master_peer->addr()));

  /* addr() setter edge case: must fail while peer is added and ESP-NOW is running. */
  uint8_t dummy[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  Serial.printf("[SLAVE] addr() set while added: %s\n", master_peer->addr(dummy) ? "success" : "failed");

  /* Confirm addr is unchanged after failed setter. */
  Serial.printf("[SLAVE] Master addr unchanged: %s\n", memcmp(master_peer->addr(), peer_mac, 6) == 0 ? "true" : "false");

  /* setRate() edge case: must fail while peer is added and ESP-NOW is running. */
  esp_now_rate_config_t rate_cfg = master_peer->getRate();
  Serial.printf("[SLAVE] setRate() while added: %s\n", master_peer->setRate(&rate_cfg) ? "success" : "failed");

  /* ---- setKey (between phases 5 and 6) ---- */
  if (master_peer->setKey((const uint8_t *)ESPNOW_LMK)) {
    Serial.printf("[SLAVE] isEncrypted after setKey: %s\n", master_peer->isEncrypted() ? "true" : "false");
  } else {
    Serial.println("[SLAVE] ERROR: setKey failed");
  }
  Serial.printf("[SLAVE] Encrypted peers after setKey: %d\n", ESP_NOW.getEncryptedPeerCount());

  /* ====================================================
   * Phase 6 – Encrypted unicast master → slave
   * ==================================================== */

  master_peer->recv_cb = false;
  waitForStart(6);

  if (master_peer->waitRecv(10000)) {
    Serial.printf("[SLAVE] Received encrypted: %.*s\n", (int)master_peer->recv_len, (char *)master_peer->recv_buf);
  } else {
    Serial.println("[SLAVE] ERROR: no encrypted message received");
  }

  /* ====================================================
   * Phase 7 – Maximum-length payload
   * ==================================================== */

  int max_len = ESP_NOW.getMaxDataLen();
  master_peer->recv_cb = false;
  waitForStart(7);

  if (master_peer->waitRecv(10000)) {
    /* Verify received length and byte pattern. */
    bool pattern_ok = ((int)master_peer->recv_len == max_len);
    for (size_t i = 0; i < master_peer->recv_len && pattern_ok; i++) {
      if (master_peer->recv_buf[i] != (uint8_t)(i & 0xFF)) {
        pattern_ok = false;
      }
    }
    Serial.printf("[SLAVE] Large payload received: %d bytes\n", (int)master_peer->recv_len);
    Serial.printf("[SLAVE] Large payload pattern: %s\n", pattern_ok ? "ok" : "error");
  } else {
    Serial.println("[SLAVE] ERROR: no large payload received");
  }

  /* ====================================================
   * Phase 8 – Peer remove / re-add (master side only)
   * ==================================================== */

  /* Slave just receives; no peer changes on this side. */
  master_peer->recv_cb = false;
  waitForStart(8);

  if (master_peer->waitRecv(10000)) {
    Serial.printf("[SLAVE] Received after re-add: %.*s\n", (int)master_peer->recv_len, (char *)master_peer->recv_buf);
  } else {
    Serial.println("[SLAVE] ERROR: no message after re-add");
  }

  /* ====================================================
   * Phase 9 – end() / begin() lifecycle + ESP_NOW.write()
   * ==================================================== */

  /* Perform end/begin BEFORE waitForStart so both devices are fully re-initialised. */
  if (ESP_NOW.end()) {
    Serial.println("[SLAVE] end(): success");
  } else {
    Serial.println("[SLAVE] end(): failed");
  }

  delay(200);

  if (ESP_NOW.begin((const uint8_t *)ESPNOW_PMK)) {
    Serial.println("[SLAVE] begin() after end: success");
  } else {
    Serial.println("[SLAVE] begin() after end: failed");
    while (true) {
      delay(1000);
    }
  }

  /* Re-add master peer (LMK still set from phase 6). */
  if (!master_peer->begin()) {
    Serial.println("[SLAVE] ERROR: failed to re-add master peer after reinit");
    while (true) {
      delay(1000);
    }
  }

  master_peer->recv_cb = false;
  waitForStart(9);

  if (master_peer->waitRecv(10000)) {
    Serial.printf("[SLAVE] Received after reinit: %.*s\n", (int)master_peer->recv_len, (char *)master_peer->recv_buf);
  } else {
    Serial.println("[SLAVE] ERROR: no message after reinit");
  }

  Serial.println("[SLAVE] Test complete");
}

void loop() {
  delay(1000);
}
