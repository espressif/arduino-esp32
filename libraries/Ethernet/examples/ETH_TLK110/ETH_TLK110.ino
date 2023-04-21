/*
    This sketch shows the Ethernet event usage

*/

#include <ETH.h>

#define ETH_ADDR        31
#define ETH_POWER_PIN   17
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_TYPE        ETH_PHY_TLK110

static bool eth_connected = false;

// WARNING: WiFiEvent is called from a separate FreeRTOS task (thread)!
void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // The hostname must be set after the interface is started, but needs
      // to be set before DHCP, so set it from the event handler thread.
      ETH.setHostname("esp32-ethernet");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
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
  WiFi.onEvent(WiFiEvent);  // Will call WiFiEvent() from another thread.
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE);
}


void loop()
{
  if (eth_connected) {
    testClient("google.com", 80);
  }
  delay(10000);
}
