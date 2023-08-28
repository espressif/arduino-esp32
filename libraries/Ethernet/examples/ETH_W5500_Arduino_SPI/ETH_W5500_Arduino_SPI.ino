/*
    This sketch shows the Ethernet event usage

*/

#include <ETH.h>
#include <SPI.h>

// Set this to 1 to enable dual Ethernet support
#define USE_TWO_ETH_PORTS 0

#define ETH_TYPE        ETH_PHY_W5500
#define ETH_ADDR         1
#define ETH_CS          15
#define ETH_IRQ          4
#define ETH_RST          5

// SPI pins
#define ETH_SPI_SCK     14
#define ETH_SPI_MISO    12
#define ETH_SPI_MOSI    13

#if USE_TWO_ETH_PORTS
// Second port on shared SPI bus
#define ETH1_TYPE        ETH_PHY_W5500
#define ETH1_ADDR         1
#define ETH1_CS          32
#define ETH1_IRQ         33
#define ETH1_RST         18
ETHClass ETH1(1);
#endif

static bool eth_connected = false;

void onEvent(arduino_event_id_t event, arduino_event_info_t info)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-eth0");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.printf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif));
      ETH.printInfo(Serial);
#if USE_TWO_ETH_PORTS
      ETH1.printInfo(Serial);
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
    default:
      break;
  }
}

void testClient(const char * host, uint16_t port)
{
  Serial.print("\nconnecting to ");
  Serial.println(host);

  WiFiClient client;
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

void setup()
{
  Serial.begin(115200);
  WiFi.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  ETH.begin(ETH_TYPE, ETH_ADDR, ETH_CS, ETH_IRQ, ETH_RST, SPI);
#if USE_TWO_ETH_PORTS
  ETH1.begin(ETH1_TYPE, ETH1_ADDR, ETH1_CS, ETH1_IRQ, ETH1_RST, SPI);
#endif
}


void loop()
{
  if (eth_connected) {
    testClient("google.com", 80);
  }
  delay(10000);
}
