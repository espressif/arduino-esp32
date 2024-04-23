#######
ESP-NOW
#######

About
-----

ESP-NOW is a communication protocol designed for low-power, low-latency, and high-throughput communication between ESP32 devices without the need for an access point (AP).
It is ideal for scenarios where devices need to communicate directly with each other in a local network.
ESP-NOW can be used for smart lights, remote control devices, sensors and many other applications.

This library provides an easy-to-use interface for setting up ESP-NOW communication, adding and removing peers, and sending and receiving data packets.

Arduino-ESP32 ESP-NOW API
-------------------------

ESP-NOW Class
*************

The `ESP_NOW_Class` is the main class used for managing ESP-NOW communication.

begin
^^^^^

Initialize the ESP-NOW communication. This function must be called before using any other ESP-NOW functionalities.

.. code-block:: cpp

    bool begin(const uint8_t *pmk = NULL);

* ``pmk``: Optional. Pass the pairwise master key (PMK) if encryption is enabled.

Returns ``true`` if initialization is successful, ``false`` otherwise.

end
^^^

End the ESP-NOW communication. This function releases all resources used by the ESP-NOW library.

.. code-block:: cpp

    bool end();

Returns ``true`` if the operation is successful, ``false`` otherwise.

getTotalPeerCount
^^^^^^^^^^^^^^^^^

Get the total number of peers currently added.

.. code-block:: cpp

    int getTotalPeerCount();

Returns the total number of peers, or ``-1`` if an error occurs.

getEncryptedPeerCount
^^^^^^^^^^^^^^^^^^^^^

Get the number of peers using encryption.

.. code-block:: cpp

    int getEncryptedPeerCount();

Returns the number of peers using encryption, or ``-1`` if an error occurs.

onNewPeer
^^^^^^^^^

You can register a callback function to handle incoming data from new peers using the `onNewPeer` function.

.. code-block:: cpp

    void onNewPeer(void (*cb)(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg), void *arg);

* ``cb``: Pointer to the callback function.
* ``arg``: Optional. Pointer to user-defined argument to be passed to the callback function.

``cb`` function signature:

.. code-block:: cpp

    void cb(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg);

``info``: Information about the received packet.
``data``: Pointer to the received data.
``len``: Length of the received data.
``arg``: User-defined argument passed to the callback function.

ESP-NOW Peer Class
******************

The `ESP_NOW_Peer` class represents a peer device in the ESP-NOW network. It is an abstract class that must be inherited by a child class that properly handles the peer connections and implements the `_onReceive` and `_onSent` methods.

Constructor
^^^^^^^^^^^

Create an instance of the `ESP_NOW_Peer` class.

.. code-block:: cpp

    ESP_NOW_Peer(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk);

* ``mac_addr``: MAC address of the peer device.
* ``channel``: Communication channel.
* ``iface``: WiFi interface.
* ``lmk``: Optional. Pass the local master key (LMK) if encryption is enabled.

add
^^^

Add the peer to the ESP-NOW network.

.. code-block:: cpp

    bool add();

Returns ``true`` if the peer is added successfully, ``false`` otherwise.

remove
^^^^^^

Remove the peer from the ESP-NOW network.

.. code-block:: cpp

    bool remove();

Returns ``true`` if the peer is removed successfully, ``false`` otherwise.

send
^^^^

Send data to the peer.

.. code-block:: cpp

    size_t send(const uint8_t *data, int len);

* ``data``: Pointer to the data to be sent.
* ``len``: Length of the data in bytes.

Returns the number of bytes sent, or ``0`` if an error occurs.

addr
^^^^

Get the MAC address of the peer.

.. code-block:: cpp

    const uint8_t * addr() const;

Returns a pointer to the MAC address.

addr
^^^^

Set the MAC address of the peer.

.. code-block:: cpp

    void addr(const uint8_t *mac_addr);

* ``mac_addr``: MAC address of the peer.

getChannel
^^^^^^^^^^

Get the communication channel of the peer.

.. code-block:: cpp

    uint8_t getChannel() const;

Returns the communication channel.

setChannel
^^^^^^^^^^

Set the communication channel of the peer.

.. code-block:: cpp

    void setChannel(uint8_t channel);

* ``channel``: Communication channel.

getInterface
^^^^^^^^^^^^

Get the WiFi interface of the peer.

.. code-block:: cpp

    wifi_interface_t getInterface() const;

Returns the WiFi interface.

setInterface
^^^^^^^^^^^^

Set the WiFi interface of the peer.

.. code-block:: cpp

    void setInterface(wifi_interface_t iface);

* ``iface``: WiFi interface.

isEncrypted
^^^^^^^^^^^

Check if the peer is using encryption.

.. code-block:: cpp

    bool isEncrypted() const;

Returns ``true`` if the peer is using encryption, ``false`` otherwise.

setKey
^^^^^^

Set the local master key (LMK) for the peer.

.. code-block:: cpp

    void setKey(const uint8_t *lmk);

* ``lmk``: Local master key.

onReceive
^^^^^^^^^^

Callback function to handle incoming data from the peer. This is a virtual method can be implemented by the upper class for custom handling.

.. code-block:: cpp

    void onReceive(const uint8_t *data, int len, bool broadcast);

* ``data``: Pointer to the received data.
* ``len``: Length of the received data.
* ``broadcast``: ``true`` if the data is broadcasted, ``false`` otherwise.

onSent
^^^^^^^

Callback function to handle the completion of sending data to the peer. This is a virtual method can be implemented by the upper class for custom handling.

.. code-block:: cpp

    void onSent(bool success);

* ``success``: ``true`` if the data is sent successfully, ``false`` otherwise.

Examples
--------

Set of 2 examples of the ESP-NOW library to send and receive data using broadcast messages between multiple ESP32 devices (multiple masters, multiple slaves).

1. ESP-NOW Broadcast Master Example:

.. literalinclude:: ../../../libraries/ESP_NOW/examples/ESP_NOW_Broadcast_Master/ESP_NOW_Broadcast_Master.ino
    :language: cpp

2. ESP-NOW Broadcast Slave Example:

.. literalinclude:: ../../../libraries/ESP_NOW/examples/ESP_NOW_Broadcast_Slave/ESP_NOW_Broadcast_Slave.ino
    :language: cpp

Example of the ESP-NOW Serial library to send and receive data as a stream between 2 ESP32 devices using the serial monitor:

.. literalinclude:: ../../../libraries/ESP_NOW/examples/ESP_NOW_Serial/ESP_NOW_Serial.ino
    :language: cpp
