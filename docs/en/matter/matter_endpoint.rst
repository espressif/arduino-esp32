##############
MatterEndPoint
##############

About
-----

The ``MatterEndPoint`` class is the base class for all Matter endpoints. It provides common functionality for all endpoint types.

* **Endpoint Management**: Each endpoint has a unique endpoint ID for identification within the Matter network
* **Attribute Access**: Methods to get and set attribute values from Matter clusters
* **Identify Cluster**: Support for device identification (visual feedback like LED blinking)
* **Secondary Network Interfaces**: Support for multiple network interfaces (WiFi, Thread, Ethernet)
* **Attribute Change Callbacks**: Base framework for handling attribute changes from Matter controllers

All Matter endpoint classes inherit from ``MatterEndPoint``, providing a consistent interface and common functionality across all device types.

MatterEndPoint APIs
-------------------

Endpoint Management
*******************

getEndPointId
^^^^^^^^^^^^

Gets the current Matter Accessory endpoint ID.

.. code-block:: arduino

    uint16_t getEndPointId();

This function will return the endpoint number (typically 1-254).

setEndPointId
^^^^^^^^^^^^

Sets the current Matter Accessory endpoint ID.

.. code-block:: arduino

    void setEndPointId(uint16_t ep);

* ``ep`` - Endpoint number to set

Secondary Network Interface
***************************

createSecondaryNetworkInterface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a secondary network interface endpoint. This can be used for devices that support multiple network interfaces, such as Ethernet, Thread and Wi-Fi.

.. code-block:: arduino

    bool createSecondaryNetworkInterface();

This function will return ``true`` if successful, ``false`` otherwise.

getSecondaryNetworkEndPointId
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the secondary network interface endpoint ID.

.. code-block:: arduino

    uint16_t getSecondaryNetworkEndPointId();

This function will return the secondary network endpoint ID, or 0 if not created.

Attribute Management
********************

getAttribute
^^^^^^^^^^^^

Gets a pointer to an attribute from its cluster ID and attribute ID.

.. code-block:: arduino

    esp_matter::attribute_t *getAttribute(uint32_t cluster_id, uint32_t attribute_id);

* ``cluster_id`` - Cluster ID (e.g., ``OnOff::Attributes::OnOff::Id``)
* ``attribute_id`` - Attribute ID (e.g., ``OnOff::Attributes::OnOff::Id``)

This function will return a pointer to the attribute, or ``NULL`` if not found.

getAttributeVal
^^^^^^^^^^^^^^^^

Gets the value of an attribute from its cluster ID and attribute ID.

.. code-block:: arduino

    bool getAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

* ``cluster_id`` - Cluster ID
* ``attribute_id`` - Attribute ID
* ``attrVal`` - Pointer to store the attribute value

This function will return ``true`` if successful, ``false`` otherwise.

setAttributeVal
^^^^^^^^^^^^^^^

Sets the value of an attribute from its cluster ID and attribute ID.

.. code-block:: arduino

    bool setAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

* ``cluster_id`` - Cluster ID
* ``attribute_id`` - Attribute ID
* ``attrVal`` - Pointer to the attribute value to set

This function will return ``true`` if successful, ``false`` otherwise.

updateAttributeVal
^^^^^^^^^^^^^^^^^^

Updates the value of an attribute from its cluster ID. This is typically used for read-only attributes that are updated by the device itself (e.g., sensor readings).

.. code-block:: arduino

    bool updateAttributeVal(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *attrVal);

* ``cluster_id`` - Cluster ID
* ``attribute_id`` - Attribute ID
* ``attrVal`` - Pointer to the attribute value to update

This function will return ``true`` if successful, ``false`` otherwise.

Identify Cluster
***************

onIdentify
^^^^^^^^^^

Sets a callback function for the Identify cluster. This callback is invoked when clients interact with the Identify Cluster of a specific endpoint.

.. code-block:: arduino

    void onIdentify(EndPointIdentifyCB onEndPointIdentifyCB);

* ``onEndPointIdentifyCB`` - Function pointer to the callback function. The callback receives a boolean parameter indicating if identify is enabled.

The callback signature is:

.. code-block:: arduino

    bool identifyCallback(bool identifyIsEnabled);

When ``identifyIsEnabled`` is ``true``, the device should provide visual feedback (e.g., blink an LED). When ``false``, the device should stop the identification feedback.

Example usage:

.. code-block:: arduino

    myEndpoint.onIdentify([](bool identifyIsEnabled) {
        if (identifyIsEnabled) {
            // Start blinking LED
            digitalWrite(LED_PIN, HIGH);
        } else {
            // Stop blinking LED
            digitalWrite(LED_PIN, LOW);
        }
        return true;
    });

Attribute Change Callback
************************

attributeChangeCB
^^^^^^^^^^^^^^^^^

This function is called by the Matter internal event processor when an attribute changes. It can be overridden by the application if necessary.

.. code-block:: arduino

    virtual bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

* ``endpoint_id`` - Endpoint ID where the attribute changed
* ``cluster_id`` - Cluster ID of the changed attribute
* ``attribute_id`` - Attribute ID that changed
* ``val`` - Pointer to the new attribute value

This function should return ``true`` if the change was handled successfully, ``false`` otherwise.

All endpoint classes implement this function to handle attribute changes specific to their device type. You typically don't need to override this unless you need custom behavior.

Supported Endpoints
-------------------

The Matter library provides specialized endpoint classes that inherit from ``MatterEndPoint``. Each endpoint type includes specific clusters and functionality relevant to that device category.

.. toctree::
    :maxdepth: 1
    :glob:

    ep_*

