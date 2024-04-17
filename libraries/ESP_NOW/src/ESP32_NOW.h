#pragma once

#include "esp_wifi_types.h"
#include "Print.h"
#include "esp_now.h"
#include "esp32-hal-log.h"
#include "esp_mac.h"

class ESP_NOW_Peer {
private:
  uint8_t mac[6];
  uint8_t chan;
  wifi_interface_t ifc;
  bool encrypt;
  uint8_t key[16];

protected:
  bool added;
  bool add();
  bool remove();
  size_t send(const uint8_t *data, int len);

  ESP_NOW_Peer(const uint8_t *mac_addr, uint8_t channel = 0, wifi_interface_t iface = WIFI_IF_AP, const uint8_t *lmk = NULL);

public:
  virtual ~ESP_NOW_Peer() {}

  const uint8_t *addr() const;
  bool addr(const uint8_t *mac_addr);

  uint8_t getChannel() const;
  bool setChannel(uint8_t channel);

  wifi_interface_t getInterface() const;
  bool setInterface(wifi_interface_t iface);

  bool isEncrypted() const;
  bool setKey(const uint8_t *lmk);

  operator bool() const;

  //optional callbacks to be implemented by the upper class
  virtual void onReceive(const uint8_t *data, size_t len, bool broadcast) {
    log_i("Received %d bytes from " MACSTR " %s", len, MAC2STR(mac), broadcast ? "(broadcast)" : "");
  }

  virtual void onSent(bool success) {
    log_i("Message transmission to peer " MACSTR " %s", MAC2STR(mac), success ? "successful" : "failed");
  }
};

class ESP_NOW_Class : public Print {
public:
  const uint8_t BROADCAST_ADDR[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

  ESP_NOW_Class();
  ~ESP_NOW_Class();

  bool begin(const uint8_t *pmk = NULL /* 16 bytes */);
  bool end();

  int getTotalPeerCount();
  int getEncryptedPeerCount();

  int availableForWrite();
  size_t write(const uint8_t *data, size_t len);
  size_t write(uint8_t data) {
    return write(&data, 1);
  }

  void onNewPeer(void (*cb)(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg), void *arg);
};

extern ESP_NOW_Class ESP_NOW;
