##########
OpenThread
##########

About
-----

The OpenThread library provides support for creating Thread network devices using ESP32 SoCs with IEEE 802.15.4 radio support. The library offers two different programming interfaces for interacting with the OpenThread stack:

* **Stream-based CLI enhanced with Helper Functions API**: Command-line interface helper functions that send OpenThread CLI commands and parse responses.
* **Classes API**: Object-oriented classes that directly call OpenThread API functions.

The OpenThread library is built on top of `ESP OpenThread <https://github.com/espressif/esp-openthread>`_ and provides a high-level Arduino-style interface for creating Thread devices.

Thread Protocol Overview
************************

Thread is an IPv6-based, low-power wireless mesh networking protocol designed for smart home and IoT applications. It provides secure, reliable, and scalable connectivity for battery-powered devices.

**Key Features:**

* **IPv6-based**: Native IPv6 addressing and routing
* **Mesh Networking**: Self-healing mesh topology with automatic routing
* **Low Power**: Optimized for battery-operated devices
* **Security**: Built-in security features including encryption and authentication
* **Scalability**: Supports up to 250+ devices per network
* **Reliability**: Automatic route discovery and self-healing capabilities

Thread Network Topology
***********************

.. code-block:: text

                               ┌─────────────────┐
                               │     Internet    │
                               └─────────────────┘
                                        ▲
                                        │
                                        │
                               ┌─────────────────┐
                               │  Wi-Fi Router   │
                               │                 │
                               └─────────────────┘
                                        │
                        ┌───────────────┴───────────────┐
                        │                               │
                        ▼                               ▼
            ┌───────────────────────┐         ┌──────────────────┐
            │  Other Wi-Fi Devices  │         │  Thread Border   │
            │                       │         │     Router       │
            └───────────────────────┘         └──────────────────┘
                                                        │
                                                        │ Thread Network
                                                        │
                              ┌─────────────────────────┼─────────────────────────┐
                              │                         │                         │
                              ▼                         ▼                         ▼
                      ┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
                      │  Thread Leader  │◄─────►│  Thread Router  │◄─────►│  Thread Child   │
                      │                 │       │                 │       │                 │
                      └─────────────────┘       └─────────────────┘       └─────────────────┘
                            │                          │                          │
                            └──────────────────────────┴──────────────────────────┘
                                              Other Thread Nodes


**Thread Device Roles:**

* **Leader**: Manages the Thread network, assigns router IDs, and maintains network state. This device should be powered by a wall adapter.
* **Router**: Extends network range, routes messages, maintains network topology. This device should be powered by a wall adapter.
* **Child**: End device that can sleep for extended periods (battery-powered devices). It can be powered by a battery or a wall adapter.
* **Detached**: Device not currently participating in a Thread network.
* **Disabled**: The Thread stack and interface are disabled.

**Other Thread Network Devices:**

* **Thread Border Router**: A device that connects a Thread network to other IP-based networks in the external world. The Thread Border Router is connected to both the Thread network and external IP networks (like a Wi-Fi router), enabling Thread devices to communicate with devices on other networks and the Internet. A Border Router can be implemented on a device with any Thread device role (Leader, Router, or Child). It can also act as a gateway to other protocols such as MQTT or Zigbee.

OpenThread Library Structure
----------------------------

**The library provides two main programming interfaces:**

* **CLI Helper Functions API**: Functions that interact with OpenThread through the CLI interface
  * ``otGetRespCmd()``: Execute CLI command and get response
  * ``otExecCommand()``: Execute CLI command with arguments
  * ``otPrintRespCLI()``: Execute CLI command and print response to Stream
  * ``OpenThreadCLI``: Stream-based CLI interface class

* **Classes API**: Object-oriented classes that directly call OpenThread API functions
  * ``OpenThread``: Main class for managing Thread network operations (includes the Joiner / Commissioner roles)
  * ``DataSet``: Class for managing Thread operational datasets
  * ``OThreadUDP``: Arduino ``UDP``-compatible class for sending and receiving IPv6 UDP datagrams over the Thread mesh, backed by the raw ``otUdpSocket`` API (no lwIP)

OpenThread Class
****************

The ``OpenThread`` class is the main entry point for Thread operations using the Classes API. It provides direct access to OpenThread API functions for managing the Thread network.

* **Network Management**: Starting, stopping, and managing the Thread network
* **Device Role Management**: Getting and monitoring device role (Leader, Router, Child, Detached, Disabled)
* **Dataset Management**: Setting and getting operational dataset parameters (offline via ``DataSet`` + ``commitDataSet`` or live via per-parameter setters)
* **Joiner Role**: Synchronously attach a brand-new device to a Thread network using only a PSKd (Pre-Shared Key for Device)
* **Commissioner Role**: Authorise remote Joiners on the network side using a PSKd
* **Address Management**: Getting mesh-local addresses, RLOC, and multicast addresses
* **Network Information**: Getting network name, channel, PAN ID, EUI-64, Thread version, TX power, and other network parameters

The ``OpenThread`` class is implemented as a singleton, meaning there's only one instance available globally. You access it directly as ``OThread`` without creating an instance.

.. toctree::
    :maxdepth: 2

    openthread_core

DataSet Class
*************

The ``DataSet`` class provides a convenient way to manage Thread operational datasets. It allows you to create, configure, and apply operational datasets to the Thread network.

* **Dataset Creation**: Create new operational datasets
* **Parameter Configuration**: Set network name, channel, PAN ID, network key, and extended PAN ID
* **Dataset Application**: Apply datasets to the Thread network
* **Dataset Retrieval**: Get current dataset parameters

.. toctree::
    :maxdepth: 2

    openthread_dataset

OpenThreadCLI
*************

The ``OpenThreadCLI`` class provides a Stream-based interface for interacting with the OpenThread CLI. It allows you to send CLI commands and receive responses programmatically.

* **CLI Interface**: Stream-based interface for sending commands and receiving responses
* **Command Execution**: Execute OpenThread CLI commands programmatically
* **Response Handling**: Parse and handle CLI command responses
* **Console Mode**: Interactive console mode for debugging and testing

.. toctree::
    :maxdepth: 2

    openthread_cli

OThreadUDP
**********

The ``OThreadUDP`` class is an Arduino ``UDP``-compatible class that sends and receives IPv6 UDP datagrams directly through the OpenThread ``otUdpSocket`` API, without going through lwIP. It is the lightest path for application-level UDP traffic over a Thread mesh and is particularly well suited to memory-constrained 802.15.4-only SoCs (ESP32-H2).

* **Familiar API**: Inherits from Arduino's ``UDP`` base, so ``begin``, ``beginPacket``, ``write``, ``endPacket``, ``parsePacket``, ``read``, ``remoteIP``, ``remotePort`` work like any other Arduino UDP driver.
* **Native OpenThread**: Uses ``otUdpOpen`` / ``otUdpBind`` / ``otUdpSend`` and exposes one ``otUdpSocket`` per instance.
* **Multicast**: ``beginMulticast(group, port)`` joins an IPv6 multicast group via ``otIp6SubscribeMulticastAddress``.
* **Tunable RX queue**: ``OT_UDP_MAX_PACKET_SIZE`` and ``OT_UDP_RX_QUEUE_DEPTH`` build-time defines control per-instance memory footprint.

.. toctree::
    :maxdepth: 2

    openthread_udp

CLI Helper Functions API
*************************

The CLI Helper Functions API provides utility functions for executing OpenThread CLI commands and parsing responses. This API is useful when you need to interact with OpenThread through the CLI interface.

* **Command Execution**: Execute CLI commands with arguments
* **Response Parsing**: Get and parse CLI command responses
* **Error Handling**: Handle CLI command errors and return codes

**Key Functions:**

* ``otGetRespCmd()``: Execute CLI command and get response string
* ``otExecCommand()``: Execute CLI command with arguments and error handling
* ``otPrintRespCLI()``: Execute CLI command and print response to Stream

For detailed documentation on the CLI Helper Functions API, see the :doc:`openthread_cli` documentation.

Supported Hardware
------------------

The OpenThread library requires ESP32 SoCs with IEEE 802.15.4 radio support:

* **ESP32-H2**: Native Thread support with IEEE 802.15.4 radio
* **ESP32-C6**: Thread support with IEEE 802.15.4 radio (when Thread is enabled)
* **ESP32-C5**: Thread support with IEEE 802.15.4 radio (when Thread is enabled)

**Note:** Thread support must be enabled in the ESP-IDF configuration (``CONFIG_OPENTHREAD_ENABLED``). This is done automatically when using the ESP32 Arduino OpenThread library.

OpenThread Examples
-------------------

The OpenThread library includes CLI-based and Native API examples demonstrating Thread network setup, CLI command automation, CoAP, UDP, and simple IoT device workflows. All examples are available in the `ESP Arduino GitHub repository <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples>`_.

**CLI Examples:**

* **Simple CLI** - Starts the OpenThread CLI console over Serial for interactive command-line testing. `View Simple CLI code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleCLI>`_
* **Simple Node** - Demonstrates a minimal CLI-driven Thread node setup. `View Simple Node code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleNode>`_
* **Thread Scan** - Uses OpenThread CLI commands to scan and report nearby Thread networks. `View Thread Scan code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/ThreadScan>`_
* **onReceive** - Shows how to handle asynchronous CLI output using the ``OpenThreadCLI`` receive callback. `View onReceive code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/onReceive>`_

**CLI Simple Thread Network Examples:**

* **Leader Node** - Creates and starts a Thread network using CLI commands. `View CLI Leader Node code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/LeaderNode>`_
* **Router Node** - Joins the CLI-created Thread network as a router-capable node. `View CLI Router Node code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/RouterNode>`_
* **Extended Router Node** - Demonstrates a CLI-driven router node with extended network behavior. `View CLI Extended Router Node code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/ExtendedRouterNode>`_

**CLI CoAP Examples:**

* **CoAP Lamp** - Implements a CoAP-controlled lamp using OpenThread CLI commands. `View CoAP Lamp code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_lamp>`_
* **CoAP Switch** - Sends CoAP control commands to the CoAP lamp example over Thread. `View CoAP Switch code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch>`_

**CLI UDP Examples:**

* **UDP Sensor Collector** - Creates a Thread Leader and UDP sink that receives sensor telemetry and sends acknowledgments. `View UDP Sensor Collector code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/UDP/udp_sensor_collector>`_
* **UDP Sensor Node** - Creates a sensor node, optionally configured as a sleepy child, that sends UDP telemetry to the collector. `View UDP Sensor Node code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/UDP/udp_sensor_node>`_

**Native API Examples:**

* **Native Examples Overview** - Describes the Native API examples and how to choose between them. `View Native examples README on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native>`_
* **Native Leader Node** - Creates and starts a Thread network using the OpenThread Classes API. `View Native Leader Node code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode>`_
* **Native Router Node** - Joins the Native API simple Thread network as a router-capable node. `View Native Router Node code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode>`_
* **Thread Commissioning** - Demonstrates a CommissionerNode that forms a network and authorises a JoinerNode that joins using only a PSKd. `View Thread Commissioning examples on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning>`_
* **UDP Light + Switch** - Demonstrates a Native UDP light server and one or more switch clients using Thread commissioning and application port ``5051``. `View UDP Light + Switch examples on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP_Light_Switch>`_
* **UDP Sensor Network** - Demonstrates a Native UDP collector and multiple sensor nodes, including application-level sequence ACKs, application port ``5050``, and optional Sleepy End Device behavior. `View UDP Sensor Network examples on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP_SensorNetwork>`_

Common Problems and Issues
--------------------------

Troubleshooting
---------------

Common Issues
*************

**Thread network not starting**
  * Ensure the device has IEEE 802.15.4 radio support (ESP32-H2, ESP32-C6, ESP32-C5).
  * Check that Thread is enabled in ESP-IDF configuration (``CONFIG_OPENTHREAD_ENABLED``).
  * Verify that ``OpenThread::begin()`` is called before using Thread functions.
  * Check Serial Monitor for initialization errors.

**Device not joining network**
  * Verify the operational dataset parameters (network name, network key, channel, PAN ID).
  * Ensure the device is within range of the Thread network.
  * Check that the network key and extended PAN ID match the network you're trying to join.
  * Verify the device role is not "Detached".

**CLI commands not working**
  * Ensure ``OpenThreadCLI::begin()`` is called before using CLI functions.
  * Check that ``OpenThreadCLI::startConsole()`` is called with a valid Stream.
  * Verify the CLI is properly initialized and running.
  * Check Serial Monitor for CLI initialization errors.

**Address not available**
  * Wait for the device to join the Thread network (role should be Child, Router, or Leader).
  * Check that the network interface is up using ``networkInterfaceUp()``.
  * Verify the device has obtained a mesh-local address.
  * Check network connectivity using ``getRloc()`` or ``getMeshLocalEid()``.

**Dataset not applying**
  * Ensure all required dataset parameters are set (network name, network key, channel).
  * Verify the dataset is valid before applying.
  * Check that OpenThread has been initialized with ``OpenThread::begin()`` before applying the dataset.
  * Apply or update the Active Operational Dataset while Thread is stopped; then bring the interface up and call ``OThread.start()``.
  * Verify the dataset parameters match the target network.

**Unexpected network after reboot**
  * OpenThread persists the committed Active Operational Dataset in NVS.
  * After ``OpenThread::begin()`` returns, use ``OThread.hasActiveDataset()`` to check whether a dataset was restored from NVS.
  * If a dataset is present, bring the interface up and start Thread to resume the existing network.
  * If you want to apply changed dataset constants, clear the stored dataset first (for example with the OpenThread CLI ``factoryreset`` command).

**Joiner fails to attach**
  * ``OT_ERROR_SECURITY``: The PSKd does not match the value the Commissioner used in ``addJoiner()``. PSKd is case-sensitive and must use the base32-thread alphabet (no ``I``, ``O``, ``Q``, ``0``).
  * ``OT_ERROR_NOT_FOUND``: No Commissioner is advertising a joinable network on the scanned channels. Make sure the Commissioner sketch is running and that ``startCommissioner()`` returned success.
  * ``OT_ERROR_RESPONSE_TIMEOUT``: The Commissioner started the handshake but did not finish it in time. Increase the ``timeoutMs`` argument of ``startJoiner`` and the ``timeoutSec`` argument of ``addJoiner``, or retry.
  * ``OT_ERROR_INVALID_STATE``: ``networkInterfaceUp()`` was not called, or ``start()`` has already been called (Thread must be stopped when the Joiner runs).
  * Build flag: confirm ``CONFIG_OPENTHREAD_JOINER=y`` is set.

**Commissioner cannot start**
  * ``OT_ERROR_INVALID_STATE``: The device must be attached to a Thread network (Leader / Router / Child) before petitioning. Wait until ``otGetDeviceRole()`` is not ``OT_ROLE_DETACHED`` / ``OT_ROLE_DISABLED``.
  * ``OT_ERROR_REJECTED``: Another Commissioner is already active in the partition, or the petition was denied. Stop the other Commissioner or wait for it to time out.
  * Build flag: confirm ``CONFIG_OPENTHREAD_COMMISSIONER=y`` is set.

**OThreadUDP packets not received**
  * Make sure the device has finished attaching (role != Detached) before binding the socket; binding before attach succeeds but no traffic flows until a mesh-local address is acquired.
  * If using multicast, double-check the group scope (``ff02::`` = link-local, ``ff03::`` = realm-local).
  * Avoid OpenThread-reserved UDP ports for application traffic. ``5683`` / ``5684`` are CoAP/CoAPs ports and ``61631`` is Thread TMF CoAP; the Native examples use ``5050`` and ``5051``.
  * If you are seeing dropped or truncated packets, bump ``OT_UDP_MAX_PACKET_SIZE`` and ``OT_UDP_RX_QUEUE_DEPTH`` at build time.
  * If ``otUdpNewMessage()`` consistently returns ``NULL``, the OpenThread message buffer pool is exhausted - reduce send rate or increase ``CONFIG_OPENTHREAD_NUM_MESSAGE_BUFFERS``.

Initialization Order
********************

For proper initialization, follow this order:

1. Initialize OpenThread stack: ``OpenThread::begin()``
2. Initialize OpenThreadCLI (if using CLI): ``OpenThreadCLI::begin()``
3. Start CLI console (if using CLI): ``OpenThreadCLI::startConsole()``
4. Configure dataset (if needed): Create and configure ``DataSet``
5. Apply dataset (if needed): ``OThread.commitDataSet(dataset)``
6. Bring network interface up: ``OThread.networkInterfaceUp()``
7. Start Thread network: ``OThread.start()``

Joiner Initialization Order (Commissioning)
*******************************************

When the device must attach to a network using only a PSKd (and learn the
operational dataset from a Commissioner instead of carrying it in source
code or NVS), follow this order on the **joiner** side:

1. ``OThread.begin(false);``           - start the stack without loading a DataSet from NVS.
2. (optional) ``OThread.setChannel(...) / setPanId(...) / setExtendedPanId(...)`` - hint the radio so the Joiner does not have to scan every channel.
3. ``OThread.networkInterfaceUp();``   - the IPv6 stack must be up before running the Joiner state machine.
4. ``OThread.startJoiner(PSKD);``      - blocks until commissioning completes or the timeout expires.
5. ``OThread.start();``                - enable Thread protocol using the dataset just provisioned by the Commissioner.

And on the **commissioner** side, after the device has already attached to the network (typically as Leader):

1. ``OThread.startCommissioner();``    - blocks until ``OT_COMMISSIONER_STATE_ACTIVE``.
2. ``OThread.addJoiner(PSKD);``        - authorise an incoming joiner (any EUI-64 by default, valid for 120 s).

For detailed API documentation see :doc:`openthread_core`, sections
"Joiner Role" and "Commissioner Role". The
``examples/Native/ThreadCommissioning/`` directory contains a working
CommissionerNode / JoinerNode pair using this flow. A more elaborate end-to-end
demo that combines Joiner / Commissioner with the ``OThreadUDP`` socket
class lives in ``examples/Native/UDP_Light_Switch/`` (``light`` server +
``switch`` client) - one Leader/Commissioner accepts any number of
joiner switches that drive an on-board RGB lamp via realm-local
multicast and receive a UDP ACK for every command.

Example
-------

Basic OpenThread Setup
**********************

Using the Classes API:

.. code-block:: arduino

    #include <OThread.h>

    void setup() {
        Serial.begin(115200);

        // Initialize OpenThread stack
        OpenThread::begin();

        // Wait for OpenThread to be ready
        while (!OThread) {
            delay(100);
        }

        // Create and configure dataset
        DataSet dataset;
        dataset.initNew();
        dataset.setNetworkName("MyThreadNetwork");
        dataset.setChannel(15);

        // Apply dataset and start network
        OThread.commitDataSet(dataset);
        OThread.networkInterfaceUp();
        OThread.start();

        // Print network information
        OpenThread::otPrintNetworkInformation(Serial);
    }

Using the CLI Helper Functions API:

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadCLI.h>
    #include <OThreadCLI_Util.h>

    void setup() {
        Serial.begin(115200);

        // Initialize OpenThread stack
        OpenThread::begin();

        // Initialize and start CLI
        OThreadCLI.begin();
        OThreadCLI.startConsole(Serial);

        // Wait for CLI to be ready
        while (!OThreadCLI) {
            delay(100);
        }

        // Execute CLI commands
        char resp[256];
        if (otGetRespCmd("state", resp)) {
            Serial.printf("Thread state: %s\r\n", resp);
        }

        if (otExecCommand("networkname", "MyThreadNetwork", NULL)) {
            Serial.println("Network name set successfully");
        }

        if (otExecCommand("ifconfig", "up", NULL)) {
            Serial.println("Network interface up");
        }
    }

Joiner-based Commissioning
**************************

Joiner (new device) - obtains the network key over the air from a Commissioner using only a PSKd:

.. code-block:: arduino

    #include <OThread.h>

    const char PSKD[] = "J01NME";

    void setup() {
        Serial.begin(115200);

        OThread.begin(false);              // stack up, no DataSet
        OThread.setChannel(15);            // optional channel hint
        OThread.networkInterfaceUp();      // IPv6 must be up

        otError err = OThread.startJoiner(PSKD);
        if (err == OT_ERROR_NONE) {
            OThread.start();               // enable Thread with provisioned dataset
            Serial.println("Joined network!");
        } else {
            Serial.printf("Joiner error: %d\r\n", err);
        }
    }

Commissioner (Leader / attached Router) - admits new joiners via PSKd:

.. code-block:: arduino

    #include <OThread.h>

    void setup() {
        Serial.begin(115200);

        // ... bring up the device as Leader (DataSet + commitDataSet + start) ...

        // Wait until the device is attached
        while (OThread.otGetDeviceRole() == OT_ROLE_DETACHED ||
               OThread.otGetDeviceRole() == OT_ROLE_DISABLED) {
            delay(100);
        }

        // Become the Commissioner and accept any joiner with PSKd "J01NME"
        if (OThread.startCommissioner() == OT_ERROR_NONE) {
            OThread.addJoiner("J01NME", nullptr, 120);  // window: 120 s
        }
    }

UDP over Thread
***************

Sending and receiving IPv6 UDP datagrams over the Thread mesh, using
``OThreadUDP`` (raw ``otUdpSocket``, no lwIP):

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadUDP.h>

    const uint8_t allNodesBytes[16] = {0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    IPAddress allNodes(IPv6, allNodesBytes);   // ff02::1
    OThreadUDP Udp;

    void setup() {
        Serial.begin(115200);

        // ... bring up Thread (DataSet or Joiner) ...
        OThread.networkInterfaceUp();
        OThread.start();

        Udp.begin(7);                          // bind to UDP port 7
    }

    void loop() {
        Udp.beginPacket(allNodes, 7);
        Udp.write((const uint8_t *)"hi", 2);
        Udp.endPacket();

        if (int n = Udp.parsePacket()) {
            char buf[64];
            int  r  = Udp.read(buf, sizeof(buf) - 1);
            buf[r]  = 0;
            Serial.printf("From [%s]:%u -> '%s'\r\n",
                          Udp.remoteIP().toString().c_str(),
                          Udp.remotePort(), buf);
        }

        delay(1000);
    }
