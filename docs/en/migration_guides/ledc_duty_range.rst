####################
LEDC maximum duty
####################

Introduction
------------

Starting with Arduino-ESP32 **4.0.x**, LEDC duty handling matches **ESP-IDF**: the valid ``ledcWrite()`` / ``ledcWriteChannel()`` / ``ledcFade*()`` range is **[0, 2^resolution]** (inclusive). Full on is duty **2^resolution**, not **2^resolution - 1**.

Earlier 4.0.x builds remapped ``ledcWrite(pin, (1 << resolution) - 1)`` to full on (see issue `#11049 <https://github.com/espressif/arduino-esp32/issues/11049>`_). That behavior was incorrect for the LEDC API and is removed.

``analogWrite()`` keeps the Arduino range **0** to **(2^resolution) - 1**. The Arduino maximum is mapped to LEDC full on before the write. Sketches that only use ``analogWrite()`` for PWM usually need no change.

Duty ranges
-----------

.. list-table::
    :header-rows: 1
    :widths: 28 36 36

    * - API
      - Valid ``value`` / ``duty``
      - Full on
    * - ``ledcWrite()``, ``ledcWriteChannel()``, ``ledcFade*()``
      - ``0`` … ``2^resolution`` (inclusive)
      - ``ledcWrite(pin, 1 << resolution)``
    * - ``analogWrite()``
      - ``0`` … ``(2^resolution) - 1`` (non-negative)
      - ``analogWrite(pin, (1 << resolution) - 1)`` (mapped internally)

Values above the LEDC maximum are **clamped** to full on. Negative ``analogWrite()`` values are **ignored**; the pin is not reconfigured and duty is left unchanged.

Breaking changes
----------------

``ledcWrite(2^n - 1)`` is no longer 100%
****************************************

**Before (buggy 4.0.x):** ``ledcWrite(pin, 255)`` at 8-bit resolution behaved like full on (duty 256).

**After:** ``ledcWrite(pin, 255)`` is one step below full on (255/256). Use ``ledcWrite(pin, 256)`` for 100%.

At 5-bit resolution (issue report):

.. code-block:: arduino

    ledcAttach(pin, 5000, 5);
    ledcWrite(pin, 31);   // 31/32, not full on
    ledcWrite(pin, 32);   // 32/32, full on

``analogWrite(negative)`` no longer becomes full on
***************************************************

**Before:** A negative ``int`` was converted to a large unsigned duty and could clamp to full on.

**After:** Negative values are ignored. On a pin that was never attached via ``analogWrite()``, LEDC is not attached.

Fade and direct LEDC examples use ``2^n`` as full on
****************************************************

Examples and fades that target 100% duty should use **4096** at 12-bit, **256** at 8-bit, and so on—not ``(1 << resolution) - 1``—when calling ``ledcWrite()`` or ``ledcFade*()`` directly.

Migration checklist
-------------------

1. Search for ``ledcWrite`` / ``ledcWriteChannel`` / ``ledcFade`` with ``255``, ``4095``, or ``(1 << …) - 1`` used as **full brightness**.
2. Replace full-on LEDC duties with ``1 << resolution`` (or the literal ``256``, ``4096``, etc.).
3. Keep ``analogWrite(pin, 255)`` (8-bit default) if you want full on through the Arduino API.
4. Treat negative ``analogWrite()`` as a no-op; do not rely on it to drive the pin high.

Further reading
---------------

API reference: :doc:`/api/ledc`
