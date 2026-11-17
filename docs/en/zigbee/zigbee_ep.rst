########
ZigbeeEP
########

About
-----

The ``ZigbeeEP`` class is the base class for all Zigbee endpoints. It provides common functionality for all endpoint types.

* **Device Information**: Every endpoint can be configured with manufacturer and model information that helps identify the device on the network
* **Binding Management**: Binding allows endpoints to establish direct communication links with other devices. This enables automatic command transmission without requiring manual addressing
* **Power Management**: Endpoints can report their power source type and battery status, which helps the network optimize communication patterns
* **Time Synchronization**: Endpoints can synchronize with network time, enabling time-based operations and scheduling
* **OTA Support**: Endpoints can receive over-the-air firmware updates to add new features or fix bugs
* **Device Discovery**: Endpoints can read manufacturer and model information from other devices on the network


ZigbeeEP APIs
-------------

Device Information
******************

setManufacturerAndModel
^^^^^^^^^^^^^^^^^^^^^^^

Sets the manufacturer name and model identifier for the device.

.. code-block:: arduino

    bool setManufacturerAndModel(const char *name, const char *model);

* ``name`` - Manufacturer name (max 32 characters)
* ``model`` - Model identifier (max 32 characters)

This function will return ``true`` if set successfully, ``false`` otherwise.

setVersion
^^^^^^^^^^

Sets the application version for the endpoint. The value is stored in the endpoint config and added to the Basic cluster (Application Version attribute). Must be called before ``Zigbee.begin()``.

.. code-block:: arduino

    void setVersion(uint8_t version);

* ``version`` - Application version (0-255)

setHardwareVersion
^^^^^^^^^^^^^^^^^^

Sets the hardware version for the endpoint. The value is added to the Basic cluster (Hardware Version attribute). Must be called before ``Zigbee.begin()``.

.. code-block:: arduino

    void setHardwareVersion(uint8_t version);

* ``version`` - Hardware version (0-255)

setDeviceEnabled
^^^^^^^^^^^^^^^^

Sets the Basic cluster ``DeviceEnabled`` attribute (0x0012). When ``false``, the Zigbee stack ignores ZCL commands on this endpoint (the device stays on the network).

Call once **before** ``Zigbee.addEndpoint()`` to add the attribute to the endpoint descriptor (same pattern as ``setManufacturerAndModel()``). After that, ``setDeviceEnabled()`` / ``getDeviceEnabled()`` work before and after ``Zigbee.begin()``. If the attribute was never added, a call after ``begin()`` returns ``false``.

Useful for soft on/off of an endpoint without leaving the network: maintenance mode, temporarily ignoring hub commands while calibrating a sensor, or disabling an unused secondary endpoint on a multi-EP device. A coordinator or other client can also write this attribute remotely once it is exposed.

.. code-block:: arduino

    bool setDeviceEnabled(bool enabled);

* ``enabled`` - ``true`` to accept ZCL commands (default), ``false`` to ignore ZCL commands on this endpoint

This function will return ``true`` if set successfully, ``false`` otherwise.

getDeviceEnabled
^^^^^^^^^^^^^^^^

Returns the current ``DeviceEnabled`` value. Useful to check whether the endpoint is accepting commands (for example after a remote write or reboot). If the attribute is not present, returns ``true`` (ZCL default).

.. code-block:: arduino

    bool getDeviceEnabled();

setReportingDefaultResponse
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enables or disables Default Response on **automatic attribute reports** for one attribute.
Call after ``setReporting()`` (reporting info must already exist; typically after ``Zigbee.begin()``).

Controls whether automatic reports request a Default Response from the receiver.
Leave enabled (default) when the sketch waits for DefaultRsp to confirm delivery (for example the
sleepy temperature example). Set ``enabled`` to ``false`` only if reports are fire-and-forget and
you want to avoid the extra response traffic.

.. code-block:: arduino

    bool setReportingDefaultResponse(uint16_t cluster_id, uint16_t attr_id, bool enabled, uint8_t cluster_role = EZB_ZCL_CLUSTER_SERVER);

* ``cluster_id`` / ``attr_id`` - Reportable attribute to configure
* ``enabled`` - ``true`` to request DefaultRsp on reports, ``false`` to disable it
* ``cluster_role`` - Cluster role (default server)

This function will return ``true`` if set successfully, ``false`` otherwise.

getEndpoint
^^^^^^^^^^^

Gets the endpoint number assigned to this device.

.. code-block:: arduino

    uint8_t getEndpoint();

This function will return the endpoint number (1-254).

Binding Management
******************

bound
^^^^^

Checks if the endpoint has any bound devices.

.. code-block:: arduino

    bool bound();

This function will return ``true`` if the endpoint has bound devices, ``false`` otherwise.

getBoundDevices
^^^^^^^^^^^^^^^

Gets the list of devices bound to this endpoint.

.. code-block:: arduino

    std::list<zb_device_params_t *> getBoundDevices() const;

This function will return list of bound device parameters.

printBoundDevices
^^^^^^^^^^^^^^^^^

Prints information about bound devices to Serial or a custom Print object.

.. code-block:: arduino

    void printBoundDevices(Print &print = Serial);

* ``print`` - Custom Print object (optional, defaults to Serial)

allowMultipleBinding
^^^^^^^^^^^^^^^^^^^^

Enables or disables multiple device binding for this endpoint.

.. code-block:: arduino

    void allowMultipleBinding(bool bind);

* ``bind`` - ``true`` to allow multiple bindings, ``false`` for single binding only

setManualBinding
^^^^^^^^^^^^^^^^

Enables or disables manual binding mode. Manual binding mode is supposed to be used when using ZHA or Z2M where you bind devices manually.

.. code-block:: arduino

    void setManualBinding(bool bind);

* ``bind`` - ``true`` for manual binding, ``false`` for automatic binding (default)

clearBoundDevices
^^^^^^^^^^^^^^^^^

Removes all bound devices from this endpoint.

.. code-block:: arduino

    void clearBoundDevices();

Binding Status
**************

epAllowMultipleBinding
^^^^^^^^^^^^^^^^^^^^^^

Gets whether multiple device binding is allowed for this endpoint.

.. code-block:: arduino

    bool epAllowMultipleBinding();

This function will return ``true`` if multiple bindings are allowed, ``false`` otherwise.

epUseManualBinding
^^^^^^^^^^^^^^^^^^

Gets whether manual binding mode is enabled for this endpoint.

.. code-block:: arduino

    bool epUseManualBinding();

This function will return ``true`` if manual binding is enabled, ``false`` otherwise.

Power Management
****************

setPowerSource
^^^^^^^^^^^^^^

Sets the power source type for the endpoint.

.. code-block:: arduino

    bool setPowerSource(uint8_t source, uint8_t percentage = 0xff, uint8_t voltage = 0xff);

* ``source`` - Power source type (``ZB_POWER_SOURCE_MAINS``, ``ZB_POWER_SOURCE_BATTERY``, etc.)
* ``percentage`` - Battery percentage (0-100, default: 0xff)
* ``voltage`` - Battery voltage in 100 mV units (default: 0xff)

This function will return ``true`` if set successfully, ``false`` otherwise.

setBatteryPercentage
^^^^^^^^^^^^^^^^^^^^

Sets the current battery percentage.

.. code-block:: arduino

    bool setBatteryPercentage(uint8_t percentage);

* ``percentage`` - Battery percentage (0-100)

This function will return ``true`` if set successfully, ``false`` otherwise.

setBatteryVoltage
^^^^^^^^^^^^^^^^^

Sets the battery voltage.

.. code-block:: arduino

    bool setBatteryVoltage(uint8_t voltage);

* ``voltage`` - Battery voltage in 100 mV units (e.g., 35 for 3.5 V)

This function will return ``true`` if set successfully, ``false`` otherwise.

reportBatteryPercentage
^^^^^^^^^^^^^^^^^^^^^^^

Reports the current battery percentage to the network.

.. code-block:: arduino

    bool reportBatteryPercentage();

This function will return ``true`` if reported successfully, ``false`` otherwise.

Time Synchronization
********************

addTimeCluster
^^^^^^^^^^^^^^

Adds time synchronization cluster to the endpoint. When you want to add a server cluster (have the time and GMT offset) fill the time structure with the current time and GMT offset.
For client cluster (get the time and GMT offset) keep the default parameters.

.. code-block:: arduino

    bool addTimeCluster(tm time = {}, int32_t gmt_offset = 0);

* ``time`` - Current time structure (default: empty)
* ``gmt_offset`` - GMT offset in seconds (default: 0)

This function will return ``true`` if added successfully, ``false`` otherwise.

setTime
^^^^^^^

Sets the current time for the endpoint.

.. code-block:: arduino

    bool setTime(tm time);

* ``time`` - Time structure to set

This function will return ``true`` if set successfully, ``false`` otherwise.

setTimezone
^^^^^^^^^^^

Sets the timezone offset for the endpoint.

.. code-block:: arduino

    bool setTimezone(int32_t gmt_offset);

* ``gmt_offset`` - GMT offset in seconds

This function will return ``true`` if set successfully, ``false`` otherwise.

getTime
^^^^^^^

Gets the current network time.

.. code-block:: arduino

    struct tm getTime(uint8_t endpoint = 1, int32_t short_addr = 0x0000, const uint8_t *ieee_addr = nullptr);

* ``endpoint`` - Target endpoint (default: 1)
* ``short_addr`` - Target device short address (default: 0x0000)
* ``ieee_addr`` - Target device IEEE address (default: ``nullptr``)

This function will return network time structure.

getTimezone
^^^^^^^^^^^

Gets the timezone offset.

.. code-block:: arduino

    int32_t getTimezone(uint8_t endpoint = 1, int32_t short_addr = 0x0000, const uint8_t *ieee_addr = nullptr);

* ``endpoint`` - Target endpoint (default: 1)
* ``short_addr`` - Target device short address (default: 0x0000)
* ``ieee_addr`` - Target device IEEE address (default: ``nullptr``)

This function will return GMT offset in seconds.

OTA Support
***********

The OTA Upgrade client cluster receives a **full** Zigbee OTA image (not Delta OTA).
Use a dual-OTA partition scheme (for example ``Zigbee 4MB with spiffs``).

Manufacturer code, image type and file version must match the OTA image header and the
coordinator policy (ZHA / Zigbee2MQTT / ESP Zigbee ``ota_server``).

addOTAClient
^^^^^^^^^^^^

Adds OTA client to the endpoint for firmware updates. Call before ``Zigbee.addEndpoint()``.

.. code-block:: arduino

    bool addOTAClient(uint32_t file_version, uint32_t downloaded_file_ver, uint16_t hw_version, uint16_t manufacturer = 0x1001, uint16_t image_type = 0x1011, uint8_t max_data_size = 48);

* ``file_version`` - Current firmware version (``CurrentFileVersion``)
* ``downloaded_file_ver`` - ``DownloadedFileVersion`` (``0xFFFFFFFF`` = none stored). Must not equal the image version offered by the OTA server or the stack will skip the download after Query approve.
* ``hw_version`` - Hardware version (passed in Query Next Image when non-zero)
* ``manufacturer`` - Manufacturer code (default: 0x1001)
* ``image_type`` - Image type code (default: 0x1011)
* ``max_data_size`` - Maximum Image Block size (default: 48, the stack default; use 223 with Espressif ``ota_server``)

After ``Zigbee.begin()``, ``requestOTAUpdate()`` applies ``hw_version`` and ``max_data_size``
via the stack APIs.

This function will return ``true`` if added successfully, ``false`` otherwise.

requestOTAUpdate
^^^^^^^^^^^^^^^^

Discovers an OTA Upgrade server on the coordinator (Match Descriptor) and sends
Query Next Image. Call after the device is connected. Further queries are sent
every hour automatically. A coordinator (for example Home Assistant ZHA) can also
start an upgrade with Image Notify without this call.

.. code-block:: arduino

    void requestOTAUpdate();

onOTAStateChange / onOTAComplete
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Optional callbacks for OTA transfer lifecycle:

.. code-block:: arduino

    void onOTAStateChange(void (*callback)(bool active));
    void onOTAComplete(void (*callback)(bool success));

* ``onOTAStateChange`` - ``true`` when download/apply becomes active, ``false`` when inactive
* ``onOTAComplete`` - ``true`` on successful FINISH (before reboot), ``false`` on abort/failure

Creating a Zigbee OTA image (Export)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

With **Tools → Zigbee Mode** enabled and OTA macros / ``addOTAClient()`` in the
sketch, **Sketch → Export compiled Binary** runs ``tools/zb_ota_image.py`` and
writes ``MMMM-TTTT-VVVVVVVV-ota-file.zigbee`` beside the ``.bin`` (build folder
and ``sketch/build/<fqbn>/``).

Sketch ``#define`` literals the tool reads:

* ``OTA_UPGRADE_MANUFACTURER`` (default ``0x1001`` if omitted but OTA is detected)
* ``OTA_UPGRADE_IMAGE_TYPE`` (default ``0x1011``)
* ``OTA_UPGRADE_FILE_VERSION`` or ``OTA_UPGRADE_RUNNING_FILE_VERSION`` (required;
  must be **greater than** the version running on the device for ZHA to offer it)
* Optional: ``OTA_UPGRADE_MIN_HW_VERSION`` + ``OTA_UPGRADE_MAX_HW_VERSION`` (only if
  you need hardware filtering; omit for ZHA local-file testing), and
  ``OTA_UPGRADE_HEADER_STRING``

The hook soft-skips (does not fail Export) when Zigbee Mode is Disabled, the
sketch has no OTA intent, or required macros are missing.

Manual alternative: Espressif's
`image_builder_tool <https://github.com/espressif/esp-zigbee-sdk/tree/main/tools/image_builder_tool>`_
(see
`Generate and apply OTA upgrade image <https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/user-guide/zcl_ota_upgrade.html>`_).

OTA with Home Assistant ZHA
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Until you place any matching ``.zigbee`` file in the Home Assistant OTA folder
(``zigpy_ota``), ZHA shows the device firmware as **Unknown** — it has nothing
to compare against. Upload an image with the **same** version as the firmware
already on the device to see **Up-to-date**, or a **newer** version when you want
an Update available.

1. USB-flash a build that is already joined to ZHA (note the running
   ``OTA_UPGRADE_RUNNING_FILE_VERSION`` on Serial).
2. Bump ``OTA_UPGRADE_RUNNING_FILE_VERSION`` to a **higher** value (do not USB-flash
   that build), keep Zigbee Mode enabled, and use **Sketch → Export compiled Binary**
   to produce the ``.zigbee`` file (or wrap the ``.bin`` with ``image_builder_tool``).
   ZHA only offers images newer than the device's current file version.
3. On Home Assistant, install the **Samba share** add-on (or another way to reach
   the ``config`` folder). Create ``config/zigpy_ota/`` and copy the ``.zigbee``
   file there.
4. Add Zigpy OTA config to ``configuration.yaml``. The ``advanced`` provider
   requires the exact ``warning`` text below (do not paraphrase — zigpy rejects
   any other string)::

       zha:
         zigpy_config:
           ota:
             extra_providers:
               - type: advanced
                 path: /config/zigpy_ota
                 warning: >-
                   I understand I can *destroy* my devices by enabling OTA updates from files.
                   Some OTA updates can be mistakenly applied to the wrong device, breaking it.
                   I am consciously using this at my own risk.
5. After copying or replacing the ``.zigbee`` file, open
   **Settings → Devices & services → Zigbee Home Automation**, click the **⋮**
   (three dots) on the ZHA integration card, and choose **Reload** so the OTA
   provider picks up the new file (a full Home Assistant restart also works).
6. Join the Arduino end device if needed, open the device page and use
   **Update** to start the firmware upgrade. Wait until the device reboots on
   the new image.

Device Discovery
****************

readManufacturer
^^^^^^^^^^^^^^^^

Reads the manufacturer name from a remote device.

.. code-block:: arduino

    char *readManufacturer(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Target device short address
* ``ieee_addr`` - Target device IEEE address

This function will return pointer to manufacturer string, or ``NULL`` if read failed.

readModel
^^^^^^^^^

Reads the model identifier from a remote device.

.. code-block:: arduino

    char *readModel(uint8_t endpoint, uint16_t short_addr, const uint8_t *ieee_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Target device short address
* ``ieee_addr`` - Target device IEEE address

This function will return pointer to model string, or ``NULL`` if read failed.

Event Handling
**************

onIdentify
^^^^^^^^^^

Sets a callback function for identify events.

.. code-block:: arduino

    void onIdentify(void (*callback)(uint16_t));

* ``callback`` - Function to call when identify event occurs
* ``time`` - Identify time in seconds

identifyQuery
^^^^^^^^^^^^^

Sends an Identify Query command. Devices that are currently identifying reply with an Identify Query Response
(see ``onIdentifyQueryResponse``). Useful for finding & binding style discovery: broadcast a query and collect
responses from devices the user has put into identify mode (for example via the hub or a button).

.. code-block:: arduino

    bool identifyQuery(uint16_t short_addr = 0xFFFF, uint8_t dst_endpoint = EZB_AF_ENDPOINT_BROADCAST_ID);

* ``short_addr`` - Destination short address (default ``0xFFFF`` = broadcast)
* ``dst_endpoint`` - Destination endpoint (default broadcast endpoint)

This function will return ``true`` if the command was sent, ``false`` otherwise.

onIdentifyQueryResponse
^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback for Identify Query Response messages received on this endpoint.

.. code-block:: arduino

    void onIdentifyQueryResponse(void (*callback)(uint16_t timeout, uint8_t src_endpoint, ezb_address_t src_address));

* ``callback`` - Function to call for each response
* ``timeout`` - Remaining identify time in seconds on the remote device
* ``src_endpoint`` - Endpoint of the identifying device
* ``src_address`` - Address of the identifying device

onDefaultResponse
^^^^^^^^^^^^^^^^^

Sets a callback for default response messages for this endpoint.

.. code-block:: arduino

    void onDefaultResponse(void (*callback)(zb_cmd_type_t resp_to_cmd, ezb_zcl_status_t status));

* ``callback`` - Function pointer to the callback function

This callback will be called when a default response is received for this specific endpoint.

Supported Endpoints
-------------------

The Zigbee library provides specialized endpoint classes for different device types. Each endpoint type includes specific clusters and functionality relevant to that device category.

.. toctree::
    :maxdepth: 1
    :glob:

    ep_*
