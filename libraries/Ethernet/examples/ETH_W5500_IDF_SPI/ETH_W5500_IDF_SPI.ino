/*
    This sketch shows the Ethernet event usage

*/

#include <ETH.h>

// Set this to 1 to enable dual Ethernet support
#define USE_TWO_ETH_PORTS 0

#ifndef ETH_PHY_CS
#define ETH_PHY_TYPE     ETH_PHY_W5500
#define ETH_PHY_ADDR     1
#define ETH_PHY_CS       15
#define ETH_PHY_IRQ      4
#define ETH_PHY_RST      5
#define ETH_PHY_SPI_HOST SPI2_HOST
#define ETH_PHY_SPI_SCK  14
#define ETH_PHY_SPI_MISO 12
#define ETH_PHY_SPI_MOSI 13
#endif

#if USE_TWO_ETH_PORTS
// Second port on shared SPI bus
#ifndef ETH1_PHY_CS
#define ETH1_PHY_TYPE ETH_PHY_W5500
#define ETH1_PHY_ADDR 1
#define ETH1_PHY_CS   32
#define ETH1_PHY_IRQ  33
#define ETH1_PHY_RST  18
#endif
ETHClass ETH1(1);
#endif

static bool eth_connected = false;

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-eth0");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: Serial.println("ETH Connected"); break;
    case ARDUINO_EVENT_ETH_GOT_IP:    Serial.printf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif)); Serial.println(ETH);
#if USE_TWO_ETH_PORTS
      Serial.println(ETH1);
#endif
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH Lost IP");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default: break;
  }
}

void testClient(const char *host, uint16_t port) {
  Serial.print("\nconnecting to ");
  Serial.println(host);

  NetworkClient client;
  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    return;
  }
  client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
  while (client.connected() && !client.available());
  while (client.available()) {
    Serial.write(client.read());
  }

  Serial.println("closing connection\n");
  client.stop();
}

void setup() {
  Serial.begin(115200);
  Network.onEvent(onEvent);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, ETH_PHY_SPI_HOST, ETH_PHY_SPI_SCK, ETH_PHY_SPI_MISO, ETH_PHY_SPI_MOSI);
#if USE_TWO_ETH_PORTS
  // Since SPI bus is shared, we should skip the SPI pins when calling ETH1.begin()
  ETH1.begin(ETH1_PHY_TYPE, ETH1_PHY_ADDR, ETH1_PHY_CS, ETH1_PHY_IRQ, ETH1_PHY_RST, ETH_PHY_SPI_HOST);
#endif
}

void loop() {
  if (eth_connected) {
    testClient("google.com", 80);
  }
  delay(10000);
}
