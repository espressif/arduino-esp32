##########
OpenThread
##########

About
-----

The OpenThread library provides support for creating Thread network devices using ESP32 SoCs with IEEE 802.15.4 radio support. The library offers two different programming interfaces for interacting with the OpenThread stack:

* **Stream-based CLI enhanced with Helper Functions API**: Command-line interface helper functions that send OpenThread CLI commands and parse responses
* **Classes API**: Object-oriented classes that directly call OpenThread API functions

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

* **Leader**: Manages the Thread network, assigns router IDs, and maintains network state. This device shall be powered by a wall adapter.
* **Router**: Extends network range, routes messages, maintains network topology. This device shall be powered by a wall adapter.
* **Child**: End device that can sleep for extended periods (battery-powered devices). It can be powered by a battery or a wall adapter.
* **Detached**: Device not currently participating in a Thread network.
* **Disabled**: The Thread stack and interface are disabled.

**Other Thread Network Devices:**

* **Thread Border Router**: A device that connects a Thread network to other IP-based networks in the extrenal world. The Thread Border Router is connected to both the Thread network and external IP networks (like Wi-Fi router), enabling Thread devices to communicate with devices on other networks and the Internet. A Border Router can be implemented on a device with any Thread device role (Leader, Router, or Child). It can also act as gateway to other protocols such as MQTT or Zigbee.

OpenThread Library Structure
----------------------------

**The library provides two main programming interfaces:**

* **CLI Helper Functions API**: Functions that interact with OpenThread through the CLI interface
  * ``otGetRespCmd()``: Execute CLI command and get response
  * ``otExecCommand()``: Execute CLI command with arguments
  * ``otPrintRespCLI()``: Execute CLI command and print response to Stream
  * ``OpenThreadCLI``: Stream-based CLI interface class

* **Classes API**: Object-oriented classes that directly call OpenThread API functions
  * ``OpenThread``: Main class for managing Thread network operations
  * ``DataSet``: Class for managing Thread operational datasets

OpenThread Class
****************

The ``OpenThread`` class is the main entry point for Thread operations using the Classes API. It provides direct access to OpenThread API functions for managing the Thread network.

* **Network Management**: Starting, stopping, and managing the Thread network
* **Device Role Management**: Getting and monitoring device role (Leader, Router, Child, Detached, Disabled)
* **Dataset Management**: Setting and getting operational dataset parameters
* **Address Management**: Getting mesh-local addresses, RLOC, and multicast addresses
* **Network Information**: Getting network name, channel, PAN ID, and other network parameters

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

Common Problems and Issues
--------------------------

Troubleshooting
---------------

Common Issues
*************

**Thread network not starting**
  * Ensure the device has IEEE 802.15.4 radio support (ESP32-H2, ESP32-C6, ESP32-C5)
  * Check that Thread is enabled in ESP-IDF configuration (``CONFIG_OPENTHREAD_ENABLED``)
  * Verify that ``OpenThread::begin()`` is called before using Thread functions
  * Check Serial Monitor for initialization errors

**Device not joining network**
  * Verify the operational dataset parameters (network name, network key, channel, PAN ID)
  * Ensure the device is within range of the Thread network
  * Check that the network key and extended PAN ID match the network you're trying to join
  * Verify the device role is not "Detached"

**CLI commands not working**
  * Ensure ``OpenThreadCLI::begin()`` is called before using CLI functions
  * Check that ``OpenThreadCLI::startConsole()`` is called with a valid Stream
  * Verify the CLI is properly initialized and running
  * Check Serial Monitor for CLI initialization errors

**Address not available**
  * Wait for the device to join the Thread network (role should be Child, Router, or Leader)
  * Check that the network interface is up using ``networkInterfaceUp()``
  * Verify the device has obtained a mesh-local address
  * Check network connectivity using ``getRloc()`` or ``getMeshLocalEid()``

**Dataset not applying**
  * Ensure all required dataset parameters are set (network name, network key, channel)
  * Verify the dataset is valid before applying
  * Check that OpenThread is started before applying the dataset
  * Verify the dataset parameters match the target network

Initialization Order
********************

For proper initialization, follow this order:

1. Initialize OpenThread stack: ``OpenThread::begin()``
2. Initialize OpenThreadCLI (if using CLI): ``OpenThreadCLI::begin()``
3. Start CLI console (if using CLI): ``OpenThreadCLI::startConsole()``
4. Configure dataset (if needed): Create and configure ``DataSet``
5. Apply dataset (if needed): ``OThread.commitDataSet(dataset)``
6. Start Thread network: ``OThread.start()``
7. Bring network interface up: ``OThread.networkInterfaceUp()``

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
        OThread.start();
        OThread.networkInterfaceUp();
        
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

