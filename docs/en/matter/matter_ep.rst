##############
MatterEndPoint
##############

About
-----

The ``MatterEndPoint`` class is the base class for all Matter endpoints. It provides common functionality for all endpoint types.

* **Endpoint Management**: Each endpoint has a unique endpoint ID for identification within the Matter network
* **Attribute Access**: Methods to get and set attribute values from Matter clusters
* **Identify Cluster**: Support for device identification (visual feedback like LED blinking)
* **Semantic Tags**: Descriptor cluster ``TagList`` support via ``setTagList()``, so controllers can tell sibling endpoints of the same device type apart
* **Secondary Network Interfaces**: Support for multiple network interfaces (Wi-Fi, Thread, Ethernet)
* **Attribute Change Callbacks**: Base framework for handling attribute changes from Matter controllers

All Matter endpoint classes inherit from ``MatterEndPoint``, providing a consistent interface and common functionality across all device types.

MatterEndPoint APIs
-------------------

Endpoint Management
*******************

getEndPointId
^^^^^^^^^^^^^

Gets the current Matter Accessory endpoint ID.

.. code-block:: arduino

    uint16_t getEndPointId();

This function will return the endpoint number (typically 1-254).

setEndPointId
^^^^^^^^^^^^^

Sets the current Matter Accessory endpoint ID.

.. code-block:: arduino

    void setEndPointId(uint16_t ep);

* ``ep`` - Endpoint number to set

Secondary Network Interface
***************************

createSecondaryNetworkInterface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
^^^^^^^^^^^^^^^

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
****************

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

Semantic Tags (TagList)
***********************

``setTagList()`` writes the Descriptor cluster ``TagList`` attribute for this endpoint. Use it to disambiguate sibling endpoints that share the same Matter device type (for example three lights tagged Top/Middle/Bottom, or Generic Switch buttons tagged On/Off plus a custom-labeled Scene).

Call ``setTagList()`` after the endpoint ``begin()`` and before ``Matter.begin()``. The first call enables the Descriptor TagList feature on that endpoint; sketches that never tag an endpoint do not pay the extra FLASH cost. Generic Switch is the exception: it still enables TagList during ``begin()``, matching the previous behavior of that endpoint type. ``setTagList()`` logs an error and returns ``false`` if the endpoint ``begin()`` has not been called.

At most ``MatterEndPoint::MAX_TAG_LIST_SIZE`` (3) tags are accepted. That limit comes from esp-matter (``ESP_MATTER_MAX_SEMANTIC_TAG_COUNT``). Optional ``label`` pointers are not copied and must remain valid for as long as the endpoint is running (string literals are fine).

Named presets live in ``MatterTags`` (see ``MatterTags.h``): ``Position``, ``Number``, ``Switches``, and ``Location``. Use ``MatterTags::createTag(namespaceId, tag, label)`` for a custom namespace/tag/label combination. For a Switches Custom tag with a user-visible label, use ``MatterTags::Switches::createCustomTag(label)``. Position Row/Column tags require a non-empty label; the Matter spec uses an Arabic numeral such as ``"1"`` for the first row/column. Use ``MatterTags::Position::createRowTag(label)`` and ``createColumnTag(label)``.

setTagList
^^^^^^^^^^

Sets the Descriptor cluster TagList attribute, replacing any list set previously.

.. code-block:: arduino

    bool setTagList(const MatterTag *tagList, uint8_t count);
    bool setTagList(std::initializer_list<MatterTag> tagList);

* ``tagList`` - Array or brace-enclosed list of ``MatterTag`` entries
* ``count`` - Number of entries (pointer overload only); must be 1..3

This function will return ``true`` if successful, ``false`` otherwise.

Example usage:

.. code-block:: arduino

    Light1.begin();
    Light2.begin();
    Light3.begin();

    Light1.setTagList({MatterTags::Position::Top, MatterTags::Number::One});
    Light2.setTagList({MatterTags::Position::Middle, MatterTags::Number::Two});
    Light3.setTagList({MatterTags::Location::Outdoor, MatterTags::Position::Bottom});

    // Position Row/Column require a non-empty label (spec uses "1" for the first row/column)
    GridCell.setTagList({MatterTags::Position::createRowTag("1"), MatterTags::Position::createColumnTag("2")});

    ButtonOn.begin();
    ButtonOn.setTagList({MatterTags::Switches::On});

    // Switches Custom tag with a label (the string literal must outlive the endpoint)
    ButtonScene.begin();
    ButtonScene.setTagList({MatterTags::Switches::createCustomTag("Scene 1")});

    // Custom namespace/tag with a label (the string literal must outlive the endpoint)
    Pump.setTagList({MatterTags::createTag(0x60, 3, "pump-A"), MatterTags::Position::Left});

See the `MatterSmartButtonsTagList <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterSmartButtonsTagList>`_ example for a complete sketch (On, Off, and a custom-labeled switch).

Attribute Change Callback
*************************

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
