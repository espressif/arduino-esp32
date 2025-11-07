###########
Network API
###########

About
-----

The Network API provides a unified, object-oriented interface for managing multiple network interfaces on ESP32 devices.
The Network Manager acts as a central coordinator that manages Wi-Fi (Station and Access Point modes), Ethernet, and PPP interfaces,
providing a consistent API regardless of the underlying network technology.

**Key Features:**

* **Unified Interface Management**: Single API for all network types (Wi-Fi, Ethernet, PPP)
* **Event-Driven Architecture**: Centralized event handling through NetworkEvents
* **Multiple Interface Support**: Simultaneous operation of multiple network interfaces
* **Default Interface Selection**: Automatic or manual selection of the default network interface
* **IPv4 and IPv6 Support**: Full support for both IP protocol versions
* **Network Communication Classes**: Unified Client, Server, and UDP classes that work with any interface

Architecture
------------

The Network library follows a tree-like hierarchical structure:

.. code-block:: text

    NetworkManager (NetworkEvents)
    │
    ├── NetworkInterface (Base Class)
    │   │
    │   ├── Wi-Fi
    │   │   ├── STAClass (Station Mode)
    │   │   └── APClass (Access Point Mode)
    │   │
    │   ├── Ethernet
    │   │   └── ETHClass
    │   │
    │   └── PPP
    │       └── PPPClass
    │
    └── Network Communication
        ├── NetworkClient
        ├── NetworkServer
        └── NetworkUdp

**Class Hierarchy:**

* **NetworkManager**: Extends ``NetworkEvents`` and ``Printable``. Manages all network interfaces and provides global network functions.
* **NetworkEvents**: Provides event callback registration and handling for all network events.
* **NetworkInterface**: Base class that all network interfaces extend. Provides common functionality for IP configuration, status checking, and network information.
* **Interface Implementations**: 
  * ``STAClass``: Wi-Fi Station (client) mode
  * ``APClass``: Wi-Fi Access Point mode
  * ``ETHClass``: Ethernet interface
  * ``PPPClass``: Point-to-Point Protocol (modem/cellular)

Network Manager
---------------

The ``NetworkManager`` class (global instance: ``Network``) is the central coordinator for all network interfaces.
It extends ``NetworkEvents`` to provide event handling capabilities and implements ``Printable`` for status output.

Initialization
**************

.. code-block:: arduino

    bool begin();

Initializes the Network Manager and the underlying ESP-IDF network interface system.
This must be called before using any network interfaces. Returns ``true`` on success, ``false`` on failure.

**Example:**

.. code-block:: arduino

    #include "Network.h"

    void setup() {
      Serial.begin(115200);
      Network.begin();  // Initialize Network Manager
    }

Default Interface Management
****************************

.. code-block:: arduino

    bool setDefaultInterface(NetworkInterface &ifc);
    NetworkInterface *getDefaultInterface();

Sets or gets the default network interface. The default interface is used for network operations when no specific interface is specified.

**Example:**

.. code-block:: arduino

    #include "Network.h"
    #include "WiFi.h"

    void setup() {
      Network.begin();
      WiFi.begin("ssid", "password");
      
      // Wait for WiFi to connect
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
      }
      
      // Set WiFi as default interface
      Network.setDefaultInterface(WiFi);
      
      // Get current default interface
      NetworkInterface *defaultIf = Network.getDefaultInterface();
      Serial.print("Default interface: ");
      Serial.println(defaultIf->impl_name());
    }

Hostname Management
*******************

.. code-block:: arduino

    static const char *getHostname();
    static bool setHostname(const char *hostname);
    static bool hostname(const String &aHostname);

Gets or sets the system hostname. The hostname is used for network identification and mDNS.

**Example:**

.. code-block:: arduino

    // Set hostname
    Network.setHostname("my-esp32-device");
    
    // Get hostname
    const char *hostname = Network.getHostname();
    Serial.println(hostname);

DNS Resolution
**************

.. code-block:: arduino

    int hostByName(const char *aHostname, IPAddress &aResult);

Resolves a hostname to an IP address using DNS. Returns ``1`` on success, error code on failure.

**Example:**

.. code-block:: arduino

    IPAddress ip;
    if (Network.hostByName("www.example.com", ip) == 1) {
      Serial.print("Resolved IP: ");
      Serial.println(ip);
    }

MAC Address
***********

.. code-block:: arduino

    uint8_t *macAddress(uint8_t *mac);
    String macAddress();

Gets the MAC address of the default network interface.

**Example:**

.. code-block:: arduino

    uint8_t mac[6];
    Network.macAddress(mac);
    Serial.print("MAC: ");
    for (int i = 0; i < 6; i++) {
      if (i > 0) Serial.print(":");
      Serial.print(mac[i], HEX);
    }
    Serial.println();
    
    // Or as String
    String macStr = Network.macAddress();
    Serial.println(macStr);

Network Events
--------------

The ``NetworkEvents`` class provides a centralized event handling system for all network-related events.
``NetworkManager`` extends ``NetworkEvents``, so you can register event callbacks directly on the ``Network`` object.

Event Types
***********

Network events are defined in ``arduino_event_id_t`` enum:

**Ethernet Events:**
* ``ARDUINO_EVENT_ETH_START``
* ``ARDUINO_EVENT_ETH_STOP``
* ``ARDUINO_EVENT_ETH_CONNECTED``
* ``ARDUINO_EVENT_ETH_DISCONNECTED``
* ``ARDUINO_EVENT_ETH_GOT_IP``
* ``ARDUINO_EVENT_ETH_LOST_IP``
* ``ARDUINO_EVENT_ETH_GOT_IP6``

**Wi-Fi Station Events:**
* ``ARDUINO_EVENT_WIFI_STA_START``
* ``ARDUINO_EVENT_WIFI_STA_STOP``
* ``ARDUINO_EVENT_WIFI_STA_CONNECTED``
* ``ARDUINO_EVENT_WIFI_STA_DISCONNECTED``
* ``ARDUINO_EVENT_WIFI_STA_GOT_IP``
* ``ARDUINO_EVENT_WIFI_STA_LOST_IP``
* ``ARDUINO_EVENT_WIFI_STA_GOT_IP6``

**Wi-Fi AP Events:**
* ``ARDUINO_EVENT_WIFI_AP_START``
* ``ARDUINO_EVENT_WIFI_AP_STOP``
* ``ARDUINO_EVENT_WIFI_AP_STACONNECTED``
* ``ARDUINO_EVENT_WIFI_AP_STADISCONNECTED``
* ``ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED``
* ``ARDUINO_EVENT_WIFI_AP_GOT_IP6``

**PPP Events:**
* ``ARDUINO_EVENT_PPP_START``
* ``ARDUINO_EVENT_PPP_STOP``
* ``ARDUINO_EVENT_PPP_CONNECTED``
* ``ARDUINO_EVENT_PPP_DISCONNECTED``
* ``ARDUINO_EVENT_PPP_GOT_IP``
* ``ARDUINO_EVENT_PPP_LOST_IP``
* ``ARDUINO_EVENT_PPP_GOT_IP6``

Registering Event Callbacks
****************************

Three types of callback functions are supported:

**1. Simple Event Callback (Event ID only):**

.. code-block:: arduino

    typedef void (*NetworkEventCb)(arduino_event_id_t event);
    network_event_handle_t onEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

**2. Functional Callback (Event ID and Info):**

.. code-block:: arduino

    typedef std::function<void(arduino_event_id_t event, arduino_event_info_t info)> NetworkEventFuncCb;
    network_event_handle_t onEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

**3. System Callback (Event Structure):**

.. code-block:: arduino

    typedef void (*NetworkEventSysCb)(arduino_event_t *event);
    network_event_handle_t onEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

**Example - Simple Callback:**

.. code-block:: arduino

    void onNetworkEvent(arduino_event_id_t event) {
      Serial.print("Network event: ");
      Serial.println(NetworkEvents::eventName(event));
      
      if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        Serial.println("WiFi connected!");
      }
    }
    
    void setup() {
      Network.begin();
      Network.onEvent(onNetworkEvent);
    }

**Example - Functional Callback with Event Info:**

.. code-block:: arduino

    void setup() {
      Network.begin();
      
      Network.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
          Serial.print("IP Address: ");
          Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
          Serial.print("Gateway: ");
          Serial.println(IPAddress(info.got_ip.ip_info.gw.addr));
        }
      });
    }

**Example - System Callback:**

.. code-block:: arduino

    void onNetworkEventSys(arduino_event_t *event) {
      Serial.print("Event: ");
      Serial.println(NetworkEvents::eventName(event->event_id));
      
      if (event->event_id == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        IPAddress ip = IPAddress(event->event_info.got_ip.ip_info.ip.addr);
        Serial.print("Got IP: ");
        Serial.println(ip);
      }
    }
    
    void setup() {
      Network.begin();
      Network.onEvent(onNetworkEventSys);
    }

Removing Event Callbacks
************************

.. code-block:: arduino

    void removeEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
    void removeEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
    void removeEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
    void removeEvent(network_event_handle_t event_handle);

Remove event callbacks by function pointer or by handle (recommended).

**Example:**

.. code-block:: arduino

    network_event_handle_t eventHandle;
    
    void setup() {
      Network.begin();
      // Register and save handle
      eventHandle = Network.onEvent(onNetworkEvent);
    }
    
    void loop() {
      // Later, remove by handle
      Network.removeEvent(eventHandle);
    }

Event Information
*****************

.. code-block:: arduino

    static const char *eventName(arduino_event_id_t id);

Returns a human-readable name for an event ID.

**Example:**

.. code-block:: arduino

    Serial.println(NetworkEvents::eventName(ARDUINO_EVENT_WIFI_STA_GOT_IP));
    // Output: "WIFI_STA_GOT_IP"

Network Interface Base Class
-----------------------------

The ``NetworkInterface`` class is the base class for all network interfaces (Wi-Fi STA, Wi-Fi AP, Ethernet, PPP).
It provides common functionality for IP configuration, status checking, and network information retrieval.

All network interfaces inherit from ``NetworkInterface`` and can be used polymorphically.

IP Configuration
****************

.. code-block:: arduino

    bool config(
      IPAddress local_ip = (uint32_t)0x00000000,
      IPAddress gateway = (uint32_t)0x00000000,
      IPAddress subnet = (uint32_t)0x00000000,
      IPAddress dns1 = (uint32_t)0x00000000,
      IPAddress dns2 = (uint32_t)0x00000000,
      IPAddress dns3 = (uint32_t)0x00000000
    );
    bool dnsIP(uint8_t dns_no, IPAddress ip);

Configures static IP address, gateway, subnet mask, and DNS servers.
For server interfaces (Wi-Fi AP), ``dns1`` is the DHCP lease range start and ``dns2`` is the DNS server.

**Example:**

.. code-block:: arduino

    #include "Network.h"
    #include "WiFi.h"
    
    void setup() {
      Network.begin();
      
      // Configure static IP
      IPAddress local_ip(192, 168, 1, 100);
      IPAddress gateway(192, 168, 1, 1);
      IPAddress subnet(255, 255, 255, 0);
      IPAddress dns1(8, 8, 8, 8);
      IPAddress dns2(8, 8, 4, 4);
      
      WiFi.config(local_ip, gateway, subnet, dns1, dns2);
      WiFi.begin("ssid", "password");
    }

Hostname
********

.. code-block:: arduino

    const char *getHostname() const;
    bool setHostname(const char *hostname) const;

Gets or sets the hostname for the specific interface.

**Example:**

.. code-block:: arduino

    WiFi.setHostname("my-wifi-device");
    Serial.println(WiFi.getHostname());

Status Checking
***************

.. code-block:: arduino

    bool started() const;
    bool connected() const;
    bool hasIP() const;
    bool hasLinkLocalIPv6() const;
    bool hasGlobalIPv6() const;
    bool linkUp() const;

Check the status of the network interface.

**Example:**

.. code-block:: arduino

    if (WiFi.started()) {
      Serial.println("WiFi interface started");
    }
    
    if (WiFi.connected()) {
      Serial.println("WiFi connected");
    }
    
    if (WiFi.hasIP()) {
      Serial.println("WiFi has IP address");
    }

IPv6 Support
************

.. code-block:: arduino

    bool enableIPv6(bool en = true);

Enables or disables IPv6 support on the interface.

**Example:**

.. code-block:: arduino

    WiFi.enableIPv6(true);
    
    if (WiFi.hasGlobalIPv6()) {
      IPAddress ipv6 = WiFi.globalIPv6();
      Serial.print("Global IPv6: ");
      Serial.println(ipv6);
    }

IP Address Information
**********************

.. code-block:: arduino

    IPAddress localIP() const;
    IPAddress subnetMask() const;
    IPAddress gatewayIP() const;
    IPAddress dnsIP(uint8_t dns_no = 0) const;
    IPAddress broadcastIP() const;
    IPAddress networkID() const;
    uint8_t subnetCIDR() const;
    IPAddress linkLocalIPv6() const;  // IPv6 only
    IPAddress globalIPv6() const;    // IPv6 only

Get IP address information from the interface.

**Example:**

.. code-block:: arduino

    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
    
    Serial.print("Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    
    Serial.print("Broadcast: ");
    Serial.println(WiFi.broadcastIP());
    
    Serial.print("Network ID: ");
    Serial.println(WiFi.networkID());
    
    Serial.print("Subnet CIDR: /");
    Serial.println(WiFi.subnetCIDR());

MAC Address
***********

.. code-block:: arduino

    uint8_t *macAddress(uint8_t *mac) const;
    String macAddress() const;

Get the MAC address of the interface.

**Example:**

.. code-block:: arduino

    uint8_t mac[6];
    WiFi.macAddress(mac);
    Serial.print("MAC: ");
    for (int i = 0; i < 6; i++) {
      if (i > 0) Serial.print(":");
      Serial.print(mac[i], HEX);
    }
    Serial.println();
    
    // Or as String
    Serial.println(WiFi.macAddress());

Default Interface
*****************

.. code-block:: arduino

    bool setDefault();
    bool isDefault() const;

Set this interface as the default network interface, or check if it is the default.

**Example:**

.. code-block:: arduino

    // Set WiFi as default
    WiFi.setDefault();
    
    // Check if it's default
    if (WiFi.isDefault()) {
      Serial.println("WiFi is the default interface");
    }

Route Priority
***************

.. code-block:: arduino

    int getRoutePrio() const;
    int setRoutePrio(int prio);  // ESP-IDF 5.5+

Gets or sets the route priority for the interface. Higher priority interfaces are preferred for routing.

**Example:**

.. code-block:: arduino

    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    // Set higher priority for Ethernet
    ETH.setRoutePrio(100);
    WiFi.setRoutePrio(50);
    #endif

Status Bits and Waiting
***********************

.. code-block:: arduino

    int getStatusBits() const;
    int waitStatusBits(int bits, uint32_t timeout_ms) const;

Get current status bits or wait for specific status bits to be set.

**Status Bits:**
* ``ESP_NETIF_STARTED_BIT``: Interface has been started
* ``ESP_NETIF_CONNECTED_BIT``: Interface is connected
* ``ESP_NETIF_HAS_IP_BIT``: Interface has an IP address
* ``ESP_NETIF_HAS_LOCAL_IP6_BIT``: Interface has link-local IPv6
* ``ESP_NETIF_HAS_GLOBAL_IP6_BIT``: Interface has global IPv6
* ``ESP_NETIF_WANT_IP6_BIT``: Interface wants IPv6
* ``ESP_NETIF_HAS_STATIC_IP_BIT``: Interface has static IP configuration

**Example:**

.. code-block:: arduino

    // Wait for WiFi to get IP address (with 10 second timeout)
    if (WiFi.waitStatusBits(ESP_NETIF_HAS_IP_BIT, 10000) & ESP_NETIF_HAS_IP_BIT) {
      Serial.println("WiFi got IP address!");
    } else {
      Serial.println("Timeout waiting for IP address");
    }

Interface Information
*********************

.. code-block:: arduino

    const char *ifkey() const;
    const char *desc() const;
    String impl_name() const;
    int impl_index() const;
    esp_netif_t *netif();

Get interface identification and description information.

**Example:**

.. code-block:: arduino

    Serial.print("Interface key: ");
    Serial.println(WiFi.ifkey());
    
    Serial.print("Description: ");
    Serial.println(WiFi.desc());
    
    Serial.print("Implementation name: ");
    Serial.println(WiFi.impl_name());
    
    Serial.print("Implementation index: ");
    Serial.println(WiFi.impl_index());

Network Interface Implementations
----------------------------------

Wi-Fi Station (STA)
********************

The ``STAClass`` (accessed via ``WiFi`` object) extends ``NetworkInterface`` and provides Wi-Fi Station (client) mode functionality.

**Key Features:**
* Connect to Wi-Fi access points
* Automatic reconnection
* WPA2/WPA3 security support
* Enterprise Wi-Fi support (WPA2-Enterprise)
* Scanning for available networks

**Example:**

.. code-block:: arduino

    #include "WiFi.h"
    
    void setup() {
      Network.begin();
      WiFi.begin("ssid", "password");
      
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      
      Serial.println();
      Serial.print("Connected! IP: ");
      Serial.println(WiFi.localIP());
    }

For detailed Wi-Fi Station API documentation, see :doc:`wifi`.

Wi-Fi Access Point (AP)
***********************

The ``APClass`` (accessed via ``WiFiAP`` object) extends ``NetworkInterface`` and provides Wi-Fi Access Point mode functionality.

**Key Features:**
* Create Wi-Fi access points
* DHCP server for connected stations
* Network Address Translation (NAT)
* Captive portal support

**Example:**

.. code-block:: arduino

    #include "WiFi.h"
    
    void setup() {
      Network.begin();
      WiFiAP.begin();
      WiFiAP.create("MyESP32AP", "password123");
      
      Serial.print("AP IP: ");
      Serial.println(WiFiAP.localIP());
    }

For detailed Wi-Fi AP API documentation, see :doc:`wifi`.

Ethernet
********

The ``ETHClass`` (accessed via ``ETH`` object) extends ``NetworkInterface`` and provides Ethernet connectivity.

**Key Features:**
* Support for multiple Ethernet PHY types
* SPI-based and EMAC-based Ethernet controllers
* Automatic link detection
* Full-duplex operation

**Example:**

.. code-block:: arduino

    #include "ETH.h"
    
    void setup() {
      Network.begin();
      ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
      
      while (!ETH.connected()) {
        delay(500);
      }
      
      Serial.print("Ethernet IP: ");
      Serial.println(ETH.localIP());
    }

For detailed Ethernet API documentation, see :doc:`ethernet`.

PPP (Point-to-Point Protocol)
*******************************

The ``PPPClass`` (accessed via ``PPP`` object) extends ``NetworkInterface`` and provides PPP connectivity, typically used with cellular modems.

**Key Features:**
* Cellular modem support (SIM7000, SIM7600, BG96, etc.)
* APN configuration
* PIN code support
* Hardware flow control

**Example:**

.. code-block:: arduino

    #include "PPP.h"
    
    void setup() {
      Network.begin();
      PPP.begin(PPP_MODEM_SIM7600, 1, 115200);
      PPP.setApn("your.apn.here");
      
      while (!PPP.connected()) {
        delay(500);
      }
      
      Serial.print("PPP IP: ");
      Serial.println(PPP.localIP());
    }

For detailed PPP API documentation, see the PPP library documentation.

Network Communication Classes
------------------------------

The Network library provides unified communication classes that work with any network interface:
``NetworkClient``, ``NetworkServer``, and ``NetworkUdp``.

NetworkClient
*************

The ``NetworkClient`` class provides TCP client functionality that works with any network interface.

**Key Features:**
* Connect to TCP servers
* Read/write data
* Timeout configuration
* Socket options

**Example:**

.. code-block:: arduino

    #include "Network.h"
    #include "WiFi.h"
    
    NetworkClient client;
    
    void setup() {
      Network.begin();
      WiFi.begin("ssid", "password");
      while (WiFi.status() != WL_CONNECTED) delay(500);
      
      if (client.connect("www.example.com", 80)) {
        client.println("GET / HTTP/1.1");
        client.println("Host: www.example.com");
        client.println();
        
        while (client.available()) {
          char c = client.read();
          Serial.print(c);
        }
        
        client.stop();
      }
    }

NetworkServer
*************

The ``NetworkServer`` class provides TCP server functionality that works with any network interface.

**Key Features:**
* Accept incoming TCP connections
* Multiple client support
* Timeout configuration

**Example:**

.. code-block:: arduino

    #include "Network.h"
    #include "WiFi.h"
    
    NetworkServer server(80);
    
    void setup() {
      Network.begin();
      WiFi.begin("ssid", "password");
      while (WiFi.status() != WL_CONNECTED) delay(500);
      
      server.begin();
      Serial.print("Server started on: ");
      Serial.println(WiFi.localIP());
    }
    
    void loop() {
      NetworkClient client = server.accept();
      if (client) {
        Serial.println("New client connected");
        client.println("Hello from ESP32!");
        client.stop();
      }
    }

NetworkUdp
**********

The ``NetworkUdp`` class provides UDP communication that works with any network interface.

**Key Features:**
* Send/receive UDP packets
* Multicast support
* Remote IP and port information

**Example:**

.. code-block:: arduino

    #include "Network.h"
    #include "WiFi.h"
    
    NetworkUDP udp;
    
    void setup() {
      Network.begin();
      WiFi.begin("ssid", "password");
      while (WiFi.status() != WL_CONNECTED) delay(500);
      
      udp.begin(1234);
    }
    
    void loop() {
      int packetSize = udp.parsePacket();
      if (packetSize) {
        Serial.print("Received packet from: ");
        Serial.print(udp.remoteIP());
        Serial.print(":");
        Serial.println(udp.remotePort());
        
        char buffer[255];
        int len = udp.read(buffer, 255);
        if (len > 0) {
          buffer[len] = 0;
          Serial.println(buffer);
        }
      }
    }

Multiple Interface Management
-----------------------------

The Network Manager allows you to manage multiple network interfaces simultaneously and switch between them as needed.

**Example - Multiple Interfaces:**

.. code-block:: arduino

    #include "Network.h"
    #include "WiFi.h"
    #include "ETH.h"
    
    void setup() {
      Network.begin();
      
      // Start Ethernet
      ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
      
      // Start WiFi as backup
      WiFi.begin("ssid", "password");
      
      // Wait for either interface to connect
      while (!ETH.connected() && WiFi.status() != WL_CONNECTED) {
        delay(500);
      }
      
      // Set the connected interface as default
      if (ETH.connected()) {
        Network.setDefaultInterface(ETH);
        Serial.println("Using Ethernet");
      } else if (WiFi.status() == WL_CONNECTED) {
        Network.setDefaultInterface(WiFi);
        Serial.println("Using WiFi");
      }
    }

**Example - Interface Priority:**

.. code-block:: arduino

    void setup() {
      Network.begin();
      
      // Start both interfaces
      ETH.begin(...);
      WiFi.begin("ssid", "password");
      
      // Set route priorities (ESP-IDF 5.5+)
      #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
      ETH.setRoutePrio(100);  // Higher priority
      WiFi.setRoutePrio(50);   // Lower priority
      #endif
      
      // Ethernet will be preferred for routing when both are connected
    }

Event Handling Examples
-----------------------

**Example - Monitor All Network Events:**

.. code-block:: arduino

    #include "Network.h"
    
    void onNetworkEvent(arduino_event_id_t event) {
      Serial.print("[Network Event] ");
      Serial.println(NetworkEvents::eventName(event));
      
      switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          Serial.println("WiFi Station got IP!");
          break;
        case ARDUINO_EVENT_ETH_GOT_IP:
          Serial.println("Ethernet got IP!");
          break;
        case ARDUINO_EVENT_PPP_GOT_IP:
          Serial.println("PPP got IP!");
          break;
        default:
          break;
      }
    }
    
    void setup() {
      Serial.begin(115200);
      Network.begin();
      Network.onEvent(onNetworkEvent);
    }

**Example - Interface-Specific Event Handling:**

.. code-block:: arduino

    #include "Network.h"
    #include "WiFi.h"
    
    void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        IPAddress ip = IPAddress(info.got_ip.ip_info.ip.addr);
        IPAddress gateway = IPAddress(info.got_ip.ip_info.gw.addr);
        IPAddress subnet = IPAddress(info.got_ip.ip_info.netmask.addr);
        
        Serial.println("WiFi Connected!");
        Serial.print("IP: ");
        Serial.println(ip);
        Serial.print("Gateway: ");
        Serial.println(gateway);
        Serial.print("Subnet: ");
        Serial.println(subnet);
      }
    }
    
    void setup() {
      Network.begin();
      Network.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
      WiFi.begin("ssid", "password");
    }

Troubleshooting
---------------

**Interface Not Starting:**
* Ensure ``Network.begin()`` is called before using any interface
* Check that the interface-specific initialization is correct
* Verify hardware connections (for Ethernet/PPP)

**Default Interface Not Working:**
* Explicitly set the default interface using ``Network.setDefaultInterface()``
* Check interface status using ``started()``, ``connected()``, and ``hasIP()``
* Verify route priorities if using multiple interfaces

**Events Not Firing:**
* Ensure ``Network.begin()`` is called to initialize the event system
* Check that callbacks are registered before the events occur
* Use ``NetworkEvents::eventName()`` to verify event IDs

**IP Configuration Issues:**
* For static IP, ensure all parameters (IP, gateway, subnet) are provided
* Check that the IP address is not already in use on the network
* Verify DNS server addresses are correct

**Multiple Interface Conflicts:**
* Set appropriate route priorities to control which interface is used
* Use ``setDefaultInterface()`` to explicitly select the default
* Monitor events to see which interface gets IP addresses first

Related Documentation
---------------------

* :doc:`wifi` - Wi-Fi API documentation
* :doc:`ethernet` - Ethernet API documentation
* `ESP-IDF Network Interface Documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_netif.html>`_

