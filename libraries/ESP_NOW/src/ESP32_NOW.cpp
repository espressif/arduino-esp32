#include "ESP32_NOW.h"
#include <string.h>
#include "esp_system.h"
#include "esp32-hal.h"
#include "esp_wifi.h"

static void (*new_cb)(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) = NULL;
static void *new_arg = NULL;  // * tx_arg = NULL, * rx_arg = NULL,
static bool _esp_now_has_begun = false;
static ESP_NOW_Peer *_esp_now_peers[ESP_NOW_MAX_TOTAL_PEER_NUM];

static esp_err_t _esp_now_add_peer(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk, ESP_NOW_Peer *_peer = NULL) {
  log_v(MACSTR, MAC2STR(mac_addr));
  if (esp_now_is_peer_exist(mac_addr)) {
    log_e("Peer Already Exists");
    return ESP_ERR_ESPNOW_EXIST;
  }

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  memcpy(peer.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);
  peer.channel = channel;
  peer.ifidx = iface;
  peer.encrypt = lmk != NULL;
  if (lmk) {
    memcpy(peer.lmk, lmk, ESP_NOW_KEY_LEN);
  }

  esp_err_t result = esp_now_add_peer(&peer);
  if (result == ESP_OK) {
    if (_peer != NULL) {
      for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
        if (_esp_now_peers[i] == NULL) {
          _esp_now_peers[i] = _peer;
          return ESP_OK;
        }
      }
      return ESP_FAIL;
    }
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    log_e("ESPNOW Not Init");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    log_e("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_FULL) {
    log_e("Peer list full");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    log_e("Out of memory");
  } else if (result == ESP_ERR_ESPNOW_EXIST) {
    log_e("Peer already exists");
  }
  return result;
}

static esp_err_t _esp_now_del_peer(const uint8_t *mac_addr) {
  esp_err_t result = esp_now_del_peer(mac_addr);
  if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    log_e("ESPNOW Not Init");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    log_e("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    log_e("Peer Not Found");
  }

  for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
    if (_esp_now_peers[i] != NULL && memcmp(mac_addr, _esp_now_peers[i]->addr(), ESP_NOW_ETH_ALEN) == 0) {
      _esp_now_peers[i] = NULL;
      break;
    }
  }
  return result;
}

static esp_err_t _esp_now_modify_peer(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) {
  log_v(MACSTR, MAC2STR(mac_addr));
  if (!esp_now_is_peer_exist(mac_addr)) {
    log_e("Peer not found");
    return ESP_ERR_ESPNOW_NOT_FOUND;
  }

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(esp_now_peer_info_t));
  memcpy(peer.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);
  peer.channel = channel;
  peer.ifidx = iface;
  peer.encrypt = lmk != NULL;
  if (lmk) {
    memcpy(peer.lmk, lmk, ESP_NOW_KEY_LEN);
  }

  esp_err_t result = esp_now_mod_peer(&peer);
  if (result == ESP_OK) {
    return result;
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    log_e("ESPNOW Not Init");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    log_e("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_FULL) {
    log_e("Peer list full");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    log_e("Out of memory");
  }
  return result;
}

static void _esp_now_rx_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  bool broadcast = memcmp(info->des_addr, ESP_NOW.BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0;
  log_v("%s from " MACSTR ", data length : %u", broadcast ? "Broadcast" : "Unicast", MAC2STR(info->src_addr), len);
  log_buf_v(data, len);
  if (!esp_now_is_peer_exist(info->src_addr) && new_cb != NULL) {
    log_v("Calling new_cb, peer not found.");
    new_cb(info, data, len, new_arg);
    return;
  }
  //find the peer and call it's callback
  for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
    if (_esp_now_peers[i] != NULL) {
      log_v("Checking peer " MACSTR, MAC2STR(_esp_now_peers[i]->addr()));
    }
    if (_esp_now_peers[i] != NULL && memcmp(info->src_addr, _esp_now_peers[i]->addr(), ESP_NOW_ETH_ALEN) == 0) {
      log_v("Calling onReceive");
      _esp_now_peers[i]->onReceive(data, len, broadcast);
      return;
    }
  }
}

static void _esp_now_tx_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
  log_v(MACSTR " : %s", MAC2STR(mac_addr), (status == ESP_NOW_SEND_SUCCESS) ? "SUCCESS" : "FAILED");
  //find the peer and call it's callback
  for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
    if (_esp_now_peers[i] != NULL && memcmp(mac_addr, _esp_now_peers[i]->addr(), ESP_NOW_ETH_ALEN) == 0) {
      _esp_now_peers[i]->onSent(status == ESP_NOW_SEND_SUCCESS);
      return;
    }
  }
}

ESP_NOW_Class::ESP_NOW_Class() {}

ESP_NOW_Class::~ESP_NOW_Class() {}

bool ESP_NOW_Class::begin(const uint8_t *pmk) {
  if (_esp_now_has_begun) {
    return true;
  }

  esp_err_t err = esp_wifi_start();
  if (err != ESP_OK) {
    log_e("WiFi not started! 0x%x)", err);
    return false;
  }

  _esp_now_has_begun = true;

  memset(_esp_now_peers, 0, sizeof(ESP_NOW_Peer *) * ESP_NOW_MAX_TOTAL_PEER_NUM);

  err = esp_now_init();
  if (err != ESP_OK) {
    log_e("esp_now_init failed! 0x%x", err);
    _esp_now_has_begun = false;
    return false;
  }

  if (pmk) {
    err = esp_now_set_pmk(pmk);
    if (err != ESP_OK) {
      log_e("esp_now_set_pmk failed! 0x%x", err);
      _esp_now_has_begun = false;
      return false;
    }
  }

  err = esp_now_register_recv_cb(_esp_now_rx_cb);
  if (err != ESP_OK) {
    log_e("esp_now_register_recv_cb failed! 0x%x", err);
    _esp_now_has_begun = false;
    return false;
  }

  err = esp_now_register_send_cb(_esp_now_tx_cb);
  if (err != ESP_OK) {
    log_e("esp_now_register_send_cb failed! 0x%x", err);
    _esp_now_has_begun = false;
    return false;
  }
  return true;
}

bool ESP_NOW_Class::end() {
  if (!_esp_now_has_begun) {
    return true;
  }
  //remove all peers?
  esp_err_t err = esp_now_deinit();
  if (err != ESP_OK) {
    log_e("esp_now_deinit failed! 0x%x", err);
    return false;
  }
  _esp_now_has_begun = false;
  memset(_esp_now_peers, 0, sizeof(ESP_NOW_Peer *) * ESP_NOW_MAX_TOTAL_PEER_NUM);
  return true;
}

int ESP_NOW_Class::getTotalPeerCount() {
  if (!_esp_now_has_begun) {
    return -1;
  }
  esp_now_peer_num_t num;
  esp_err_t err = esp_now_get_peer_num(&num);
  if (err != ESP_OK) {
    log_e("esp_now_get_peer_num failed! 0x%x", err);
    return -1;
  }
  return num.total_num;
}

int ESP_NOW_Class::getEncryptedPeerCount() {
  if (!_esp_now_has_begun) {
    return -1;
  }
  esp_now_peer_num_t num;
  esp_err_t err = esp_now_get_peer_num(&num);
  if (err != ESP_OK) {
    log_e("esp_now_get_peer_num failed! 0x%x", err);
    return -1;
  }
  return num.encrypt_num;
}

int ESP_NOW_Class::availableForWrite() {
  return ESP_NOW_MAX_DATA_LEN;
}

size_t ESP_NOW_Class::write(const uint8_t *data, size_t len) {
  if (!_esp_now_has_begun) {
    return 0;
  }
  if (len > ESP_NOW_MAX_DATA_LEN) {
    len = ESP_NOW_MAX_DATA_LEN;
  }
  esp_err_t result = esp_now_send(NULL, data, len);
  if (result == ESP_OK) {
    return len;
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    log_e("ESPNOW not init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    log_e("Invalid argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    log_e("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    log_e("Our of memory");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    log_e("Peer not found.");
  } else if (result == ESP_ERR_ESPNOW_IF) {
    log_e("Interface does not match.");
  }
  return 0;
}

void ESP_NOW_Class::onNewPeer(void (*cb)(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg), void *arg) {
  new_cb = cb;
  new_arg = arg;
}

ESP_NOW_Class ESP_NOW;

/*
 *
 *    Inheritable Peer Class
 *
*/

ESP_NOW_Peer::ESP_NOW_Peer(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) {
  added = false;
  if (mac_addr) {
    memcpy(mac, mac_addr, 6);
  }
  chan = channel;
  ifc = iface;
  encrypt = lmk != NULL;
  if (encrypt) {
    memcpy(key, lmk, 16);
  }
}

bool ESP_NOW_Peer::add() {
  if (!_esp_now_has_begun) {
    return false;
  }
  if (added) {
    return true;
  }
  if (_esp_now_add_peer(mac, chan, ifc, encrypt ? key : NULL, this) != ESP_OK) {
    return false;
  }
  log_v("Peer added - " MACSTR, MAC2STR(mac));
  added = true;
  return true;
}

bool ESP_NOW_Peer::remove() {
  if (!_esp_now_has_begun) {
    return false;
  }
  if (!added) {
    return true;
  }
  log_v("Peer removed - " MACSTR, MAC2STR(mac));
  esp_err_t err = _esp_now_del_peer(mac);
  if (err == ESP_OK) {
    added = false;
    return true;
  }
  return false;
}

const uint8_t *ESP_NOW_Peer::addr() const {
  return mac;
}

bool ESP_NOW_Peer::addr(const uint8_t *mac_addr) {
  if (!_esp_now_has_begun || !added) {
    memcpy(mac, mac_addr, 6);
    return true;
  }
  return false;
}

uint8_t ESP_NOW_Peer::getChannel() const {
  return chan;
}

bool ESP_NOW_Peer::setChannel(uint8_t channel) {
  chan = channel;
  if (!_esp_now_has_begun || !added) {
    return true;
  }
  return _esp_now_modify_peer(mac, chan, ifc, encrypt ? key : NULL) == ESP_OK;
}

wifi_interface_t ESP_NOW_Peer::getInterface() const {
  return ifc;
}

bool ESP_NOW_Peer::setInterface(wifi_interface_t iface) {
  ifc = iface;
  if (!_esp_now_has_begun || !added) {
    return true;
  }
  return _esp_now_modify_peer(mac, chan, ifc, encrypt ? key : NULL) == ESP_OK;
}

bool ESP_NOW_Peer::isEncrypted() const {
  return encrypt;
}

bool ESP_NOW_Peer::setKey(const uint8_t *lmk) {
  encrypt = lmk != NULL;
  if (encrypt) {
    memcpy(key, lmk, 16);
  }
  if (!_esp_now_has_begun || !added) {
    return true;
  }
  return _esp_now_modify_peer(mac, chan, ifc, encrypt ? key : NULL) == ESP_OK;
}

size_t ESP_NOW_Peer::send(const uint8_t *data, int len) {
  log_v(MACSTR ", data length %d", MAC2STR(mac), len);
  if (!_esp_now_has_begun || !added) {
    log_e("Peer not added.");
    return 0;
  }
  if (len > ESP_NOW_MAX_DATA_LEN) {
    len = ESP_NOW_MAX_DATA_LEN;
  }
  esp_err_t result = esp_now_send(mac, data, len);
  if (result == ESP_OK) {
    return len;
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    log_e("ESPNOW not init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    log_e("Invalid argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    log_e("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    log_e("Our of memory");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    log_e("Peer not found.");
  } else if (result == ESP_ERR_ESPNOW_IF) {
    log_e("Interface does not match.");
  }
  return 0;
}

ESP_NOW_Peer::operator bool() const {
  return added;
}
