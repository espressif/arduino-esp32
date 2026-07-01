###################
MatterGenericSwitch
###################

About
-----

The ``MatterGenericSwitch`` class provides a generic switch endpoint for Matter networks. It implements the Matter **Switch** cluster as a momentary smart button that reports press gestures to the Matter controller.

**Features:**

* Configurable Switch cluster features (short click, long press, multi-press)
* Individual event methods matching the Matter specification
* ``click()`` convenience helper for simple automations
* Automation trigger support for Apple Home, Amazon Alexa, Google Home, and Home Assistant
* Matter standard compliance

**Use Cases:**

* Smart buttons and scene triggers
* Remote controls with single, double, or long press
* Event generators for smart home automation

Switch Cluster Features
-----------------------

The Matter Switch cluster exposes optional **FeatureMap** bits. This class provides constants that match the specification:

+---------------------------+-------+-----------------------------------+
| Constant                  | Bit   | Events enabled                    |
+===========================+=======+===================================+
| ``FEATURE_MOMENTARY``     | 0x02  | ``InitialPress``                  |
+---------------------------+-------+-----------------------------------+
| ``FEATURE_RELEASE``       | 0x04  | ``ShortRelease``                  |
+---------------------------+-------+-----------------------------------+
| ``FEATURE_LONG_PRESS``    | 0x08  | ``LongPress``, ``LongRelease``    |
+---------------------------+-------+-----------------------------------+
| ``FEATURE_MULTI_PRESS``   | 0x10  | ``MultiPressOngoing``,            |
|                           |       | ``MultiPressComplete``            |
+---------------------------+-------+-----------------------------------+

**Preset combinations:**

* ``FEATURE_SIMPLE`` (default) — ``FEATURE_MOMENTARY | FEATURE_RELEASE`` for a single short click
* ``FEATURE_ALL`` — all momentary gesture features above

**Dependencies:**

* ``FEATURE_LONG_PRESS`` requires ``FEATURE_RELEASE``
* ``FEATURE_MULTI_PRESS`` requires ``FEATURE_RELEASE``

API Reference
-------------

Constructor
***********

MatterGenericSwitch
^^^^^^^^^^^^^^^^^^^

Creates a new Matter generic switch endpoint.

.. code-block:: arduino

    MatterGenericSwitch();

Feature Constants
*****************

.. code-block:: arduino

    static constexpr uint32_t FEATURE_MOMENTARY;
    static constexpr uint32_t FEATURE_RELEASE;
    static constexpr uint32_t FEATURE_LONG_PRESS;
    static constexpr uint32_t FEATURE_MULTI_PRESS;
    static constexpr uint32_t FEATURE_SIMPLE;   // short click
    static constexpr uint32_t FEATURE_ALL;      // all gestures

Initialization
**************

begin
^^^^^

Initializes the Matter generic switch endpoint.

.. code-block:: arduino

    bool begin(uint32_t featureFlags = FEATURE_SIMPLE, uint8_t multiPressMax = 5);

* **featureFlags** — Switch cluster features to enable at endpoint creation. Defaults to ``FEATURE_SIMPLE``.
* **multiPressMax** — Maximum press count for multi-press (2–255). Used only when ``FEATURE_MULTI_PRESS`` is set.

Returns ``true`` on success, ``false`` otherwise.

**Examples:**

.. code-block:: arduino

    // Simple short-click button (default)
    mySwitch.begin();

    // Full gesture support (long press + multi-press up to 5 clicks)
    mySwitch.begin(MatterGenericSwitch::FEATURE_ALL, 5);

end
^^^

Stops processing Matter switch events.

.. code-block:: arduino

    void end();

hasFeature
^^^^^^^^^^

Returns ``true`` if the given feature bit is enabled.

.. code-block:: arduino

    bool hasFeature(uint32_t feature) const;

Event Generation
****************

Call these from your button driver when the corresponding physical action occurs.

press
^^^^^

Sends ``InitialPress`` (button pressed down).

.. code-block:: arduino

    void press();

release
^^^^^^^

Sends ``ShortRelease`` (button released after a short press).

.. code-block:: arduino

    void release();

longPress
^^^^^^^^^

Sends ``LongPress`` (button held past the long-press threshold).

.. code-block:: arduino

    void longPress();

longRelease
^^^^^^^^^^^

Sends ``LongRelease`` (button released after a long press).

.. code-block:: arduino

    void longRelease();

multiPressOngoing
^^^^^^^^^^^^^^^^^

Sends ``MultiPressOngoing`` when an additional press starts within the multi-press window.

.. code-block:: arduino

    void multiPressOngoing(uint8_t count);

multiPressComplete
^^^^^^^^^^^^^^^^^^

Sends ``MultiPressComplete`` when the multi-press window expires after the last release.

.. code-block:: arduino

    void multiPressComplete(uint8_t count);

click
^^^^^

Convenience helper: sends ``InitialPress`` followed by ``ShortRelease`` when the release feature is enabled.

.. code-block:: arduino

    void click();

Gesture Event Flow
------------------

A correct short click sends two events:

1. ``InitialPress`` on button down
2. ``ShortRelease`` on button up

Long press adds ``LongPress`` while held and ``LongRelease`` on release.

Multi-press (double/triple click) follows the ESP-Matter generic switch pattern:

1. First press down → ``InitialPress``
2. First release → ``ShortRelease``
3. Second press down (within window) → ``MultiPressOngoing`` (count = 2)
4. Second release → ``ShortRelease``
5. Window expires → ``MultiPressComplete`` (total count)

Examples
--------

Simple Smart Button
*******************

Minimal short-click implementation using ``press()`` on down and ``release()`` on up.

`View Matter Smart Button example on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterSmartButton>`_

Enhanced Smart Button
*********************

Full gesture support with long press and multi-press.

`View Matter Enhanced Smart Button example on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEnhancedSmartButton>`_
