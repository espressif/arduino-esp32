/*
 * ESP-NOW validation test – Master device
 *
 * Phases:
 *   1  Init, pre-begin edge cases, master broadcast send (onNewPeer)
 *   2  Slave broadcast send + recv_bcast flag verification
 *   3  Unicast master → slave
 *   4  Unicast slave → master
 *   5  Peer management: getters, addr()/setRate() edge cases, operator bool
 *      (between 5 and 6: setKey is called; isEncrypted / encrypted-peer-count printed)
 *   6  Encrypted unicast master → slave
 *   7  Maximum-length payload master → slave
 *   8  Peer remove / re-add
 *   9  end() / begin() lifecycle + ESP_NOW.write() broadcast to all peers
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

static uint8_t peer_mac[6] = {};
static TestPeer *bcast_peer = nullptr;
static TestPeer *slave_peer = nullptr;

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
  Serial.printf("[MASTER] Ready for phase %d\n", phase);
  while (true) {
    if (Serial.available()) {
      String s = Serial.readStringUntil('\n');
      s.trim();
      if (s == "START") {
        Serial.printf("[MASTER] Phase %d started\n", phase);
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
  Serial.printf("[MASTER] MAC: " MACSTR "\n", MAC2STR(self_mac));
  Serial.println("[MASTER] Send peer MAC:");
  String mac_str = "";
  while (mac_str.length() == 0) {
    if (Serial.available()) {
      mac_str = Serial.readStringUntil('\n');
      mac_str.trim();
    }
    delay(10);
  }
  if (!parseMac(mac_str, peer_mac)) {
    Serial.println("[MASTER] ERROR: invalid peer MAC");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("[MASTER] Peer MAC: " MACSTR "\n", MAC2STR(peer_mac));

  /* ====================================================
   * Phase 1 – Init, pre-begin edge cases, broadcast
   * ==================================================== */

  /* Test API before begin() (edge cases). */
  Serial.printf("[MASTER] getVersion before begin: %d\n", ESP_NOW.getVersion());
  Serial.printf("[MASTER] getMaxDataLen before begin: %d\n", ESP_NOW.getMaxDataLen());
  Serial.printf("[MASTER] getTotalPeerCount before begin: %d\n", ESP_NOW.getTotalPeerCount());

  /* Init ESP-NOW with PMK. */
  if (!ESP_NOW.begin((const uint8_t *)ESPNOW_PMK)) {
    Serial.println("[MASTER] ERROR: begin() failed");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("[MASTER] Version: %d\n", ESP_NOW.getVersion());
  Serial.printf("[MASTER] Max data len: %d\n", ESP_NOW.getMaxDataLen());
  Serial.printf("[MASTER] availableForWrite: %d\n", ESP_NOW.availableForWrite());

  /* Create broadcast peer (no LMK). */
  bcast_peer = new TestPeer(ESP_NOW.BROADCAST_ADDR, ESPNOW_WIFI_CHANNEL, WIFI_IF_STA);
  if (!bcast_peer->begin()) {
    Serial.println("[MASTER] ERROR: failed to add broadcast peer");
    while (true) {
      delay(1000);
    }
  }

  waitForStart(1);

  bcast_peer->sendMsg((const uint8_t *)"Hello broadcast", 15);
  if (bcast_peer->waitSent(5000)) {
    Serial.printf("[MASTER] Broadcast sent: %s\n", bcast_peer->sent_ok ? "success" : "failed");
  } else {
    Serial.println("[MASTER] Broadcast sent: timeout");
  }

  /* Add slave_peer now so its onReceive fires for the slave's broadcast in phase 2. */
  slave_peer = new TestPeer(peer_mac, ESPNOW_WIFI_CHANNEL, WIFI_IF_STA);
  if (!slave_peer->begin()) {
    Serial.println("[MASTER] ERROR: failed to add slave peer");
    while (true) {
      delay(1000);
    }
  }

  /* ====================================================
   * Phase 2 – Slave broadcast + recv_bcast flag
   * ==================================================== */

  slave_peer->recv_cb = false;
  waitForStart(2);

  /* Slave sends a broadcast; our slave_peer.onReceive fires with broadcast=true. */
  if (slave_peer->waitRecv(10000)) {
    Serial.printf("[MASTER] Received slave broadcast: %.*s\n", (int)slave_peer->recv_len, (char *)slave_peer->recv_buf);
    Serial.printf("[MASTER] Received as broadcast: %s\n", slave_peer->recv_bcast ? "true" : "false");
  } else {
    Serial.println("[MASTER] ERROR: no broadcast from slave");
  }

  /* ====================================================
   * Phase 3 – Unicast master → slave
   * ==================================================== */

  waitForStart(3);

  slave_peer->sendMsg((const uint8_t *)"Phase3:M->S", 11);
  if (slave_peer->waitSent(5000)) {
    Serial.printf("[MASTER] Unicast to slave: %s\n", slave_peer->sent_ok ? "success" : "failed");
  } else {
    Serial.println("[MASTER] Unicast to slave: timeout");
  }

  /* ====================================================
   * Phase 4 – Unicast slave → master
   * ==================================================== */

  slave_peer->recv_cb = false;
  waitForStart(4);

  if (slave_peer->waitRecv(10000)) {
    Serial.printf("[MASTER] Received from slave: %.*s\n", (int)slave_peer->recv_len, (char *)slave_peer->recv_buf);
  } else {
    Serial.println("[MASTER] ERROR: no message from slave");
  }

  /* ====================================================
   * Phase 5 – Peer management: getters and edge cases
   * ==================================================== */

  waitForStart(5);

  Serial.printf("[MASTER] Total peers: %d\n", ESP_NOW.getTotalPeerCount());
  Serial.printf("[MASTER] Encrypted peers: %d\n", ESP_NOW.getEncryptedPeerCount());
  Serial.printf("[MASTER] Slave channel: %u\n", slave_peer->getChannel());
  Serial.printf("[MASTER] Slave interface: %d\n", (int)slave_peer->getInterface());
  Serial.printf("[MASTER] Slave isEncrypted: %s\n", slave_peer->isEncrypted() ? "true" : "false");

  /* operator bool(): true when added. */
  Serial.printf("[MASTER] slave_peer bool (added): %s\n", (bool)*slave_peer ? "true" : "false");

  /* addr() getter: should return slave's MAC. */
  Serial.printf("[MASTER] Slave addr: " MACSTR "\n", MAC2STR(slave_peer->addr()));

  /* addr() setter edge case: must fail while peer is added and ESP-NOW is running. */
  uint8_t dummy[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  Serial.printf("[MASTER] addr() set while added: %s\n", slave_peer->addr(dummy) ? "success" : "failed");

  /* Confirm addr is unchanged after failed setter. */
  Serial.printf("[MASTER] Slave addr unchanged: %s\n", memcmp(slave_peer->addr(), peer_mac, 6) == 0 ? "true" : "false");

  /* setRate() edge case: must fail while peer is added and ESP-NOW is running. */
  esp_now_rate_config_t rate_cfg = slave_peer->getRate();
  Serial.printf("[MASTER] setRate() while added: %s\n", slave_peer->setRate(&rate_cfg) ? "success" : "failed");

  /* ---- setKey (between phases 5 and 6) ---- */
  if (slave_peer->setKey((const uint8_t *)ESPNOW_LMK)) {
    Serial.printf("[MASTER] isEncrypted after setKey: %s\n", slave_peer->isEncrypted() ? "true" : "false");
  } else {
    Serial.println("[MASTER] ERROR: setKey failed");
  }
  Serial.printf("[MASTER] Encrypted peers after setKey: %d\n", ESP_NOW.getEncryptedPeerCount());

  /* ====================================================
   * Phase 6 – Encrypted unicast master → slave
   * ==================================================== */

  waitForStart(6);

  slave_peer->sent_cb = false;
  slave_peer->sendMsg((const uint8_t *)"EncryptedMsg", 12);
  if (slave_peer->waitSent(5000)) {
    Serial.printf("[MASTER] Encrypted message sent: %s\n", slave_peer->sent_ok ? "success" : "failed");
  } else {
    Serial.println("[MASTER] Encrypted message sent: timeout");
  }

  /* ====================================================
   * Phase 7 – Maximum-length payload
   * ==================================================== */

  int max_len = ESP_NOW.getMaxDataLen();
  uint8_t *large_buf = (uint8_t *)malloc(max_len);
  if (!large_buf) {
    Serial.println("[MASTER] ERROR: malloc failed");
    while (true) {
      delay(1000);
    }
  }
  for (int i = 0; i < max_len; i++) {
    large_buf[i] = (uint8_t)(i & 0xFF);
  }

  waitForStart(7);

  slave_peer->sent_cb = false;
  slave_peer->sendMsg(large_buf, max_len);
  free(large_buf);
  if (slave_peer->waitSent(5000)) {
    Serial.printf("[MASTER] Large payload sent: %d bytes (%s)\n", max_len, slave_peer->sent_ok ? "success" : "failed");
  } else {
    Serial.println("[MASTER] Large payload sent: timeout");
  }

  /* ====================================================
   * Phase 8 – Peer remove / re-add
   * ==================================================== */

  waitForStart(8);

  ESP_NOW.removePeer(*slave_peer);
  /* operator bool() should be false after remove. */
  Serial.printf("[MASTER] slave_peer bool (removed): %s\n", (bool)*slave_peer ? "true" : "false");
  Serial.printf("[MASTER] Total peers after remove: %d\n", ESP_NOW.getTotalPeerCount());

  /* Re-add – peer still holds the LMK from phase 6. */
  if (slave_peer->begin()) {
    Serial.printf("[MASTER] Total peers after re-add: %d\n", ESP_NOW.getTotalPeerCount());
    slave_peer->sent_cb = false;
    slave_peer->sendMsg((const uint8_t *)"AfterReAdd", 10);
    if (slave_peer->waitSent(5000)) {
      Serial.printf("[MASTER] Send after re-add: %s\n", slave_peer->sent_ok ? "success" : "failed");
    } else {
      Serial.println("[MASTER] Send after re-add: timeout");
    }
  } else {
    Serial.println("[MASTER] ERROR: failed to re-add slave peer");
  }

  /* ====================================================
   * Phase 9 – end() / begin() lifecycle + ESP_NOW.write()
   * ==================================================== */

  /* Perform end/begin BEFORE waitForStart so both devices are fully re-initialised. */
  if (ESP_NOW.end()) {
    Serial.println("[MASTER] end(): success");
  } else {
    Serial.println("[MASTER] end(): failed");
  }

  delay(200);

  if (ESP_NOW.begin((const uint8_t *)ESPNOW_PMK)) {
    Serial.println("[MASTER] begin() after end: success");
  } else {
    Serial.println("[MASTER] begin() after end: failed");
    while (true) {
      delay(1000);
    }
  }

  /* Re-add slave peer (LMK still set from phase 6). bcast_peer is NOT re-added,
   * so ESP_NOW.write() will send only to slave_peer (clean single-recipient test). */
  if (!slave_peer->begin()) {
    Serial.println("[MASTER] ERROR: failed to re-add slave peer after reinit");
    while (true) {
      delay(1000);
    }
  }

  waitForStart(9);

  /* Test ESP_NOW.write() – sends to all registered peers (only slave_peer here). */
  slave_peer->sent_cb = false;
  const uint8_t write_data[] = "WriteToAll";
  size_t written = ESP_NOW.write(write_data, sizeof(write_data) - 1);
  Serial.printf("[MASTER] ESP_NOW.write() bytes: %d\n", (int)written);
  if (slave_peer->waitSent(5000)) {
    Serial.printf("[MASTER] ESP_NOW.write() sent: %s\n", slave_peer->sent_ok ? "success" : "failed");
  } else {
    Serial.println("[MASTER] ESP_NOW.write() sent: timeout");
  }

  Serial.println("[MASTER] Test complete");
}

void loop() {
  delay(1000);
}
