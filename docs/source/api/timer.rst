#####
Timer
#####

About
-----

The ESP32 SoCs contains from 2 to 4 hardware timers. 
They are all 64-bit (54-bit for ESP32-C3) generic timers based on 16-bit pre-scalers and 64-bit (54-bit for ESP32-C3) 
up / down counters which are capable of being auto-reloaded.

========= ================
ESP32 SoC Number of timers
========= ================
ESP32     4
ESP32-S2  4
ESP32-C3  2
ESP32-S3  4
========= ================

Arduino-ESP32 Timer API
-----------------------

timerBegin
**********

This function is used to configure the timer. After successful setup the timer will automatically start.

.. code-block:: arduino

    hw_timer_t * timerBegin(uint8_t num, uint16_t divider, bool countUp);

* ``num`` select timer number.
* ``divider`` select timer divider. Sets how quickly the timer counter is “ticking”.
* ``countUp`` select timer direction. Sets if the counter should be incrementing or decrementing.

This function will return ``timer`` structure if configuration is successful.
If ``NULL`` is returned, error occurs and the timer was not configured.

timerEnd
********

This function is used to end timer.

.. code-block:: arduino

    void timerEnd(hw_timer_t *timer);

* ``timer`` timer struct.

timerSetConfig
**************

This function is used to configure initialized timer (timerBegin() called).

.. code-block:: arduino

    uint32_t timerGetConfig(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``configuration`` as uint32_t number. 
This can be translated by inserting it to struct ``timer_cfg_t.val``.

timerAttachInterrupt
********************

This function is used to attach interrupt to timer.

.. code-block:: arduino

    void timerAttachInterrupt(hw_timer_t *timer, void (*fn)(void), bool edge);

* ``timer`` timer struct.
* ``fn`` funtion to be called when interrupt is triggered.
* ``edge`` select edge to trigger interrupt (only LEVEL trigger is currently supported).

timerDetachInterrupt
********************

This function is used to detach interrupt from timer.

.. code-block:: arduino

    void timerDetachInterrupt(hw_timer_t *timer);

* ``timer`` timer struct.

timerStart
**********

This function is used to start counter of the timer.

.. code-block:: arduino

    void timerStart(hw_timer_t *timer);

* ``timer`` timer struct.

timerStop
*********

This function is used to stop counter of the timer.

.. code-block:: arduino

    void timerStop(hw_timer_t *timer);

* ``timer`` timer struct.

timerRestart
************

This function is used to restart counter of the timer.

.. code-block:: arduino

    void timerRestart(hw_timer_t *timer);

* ``timer`` timer struct.

timerWrite
**********

This function is used to set counter value of the timer.

.. code-block:: arduino

    void timerWrite(hw_timer_t *timer, uint64_t val);

* ``timer`` timer struct.
* ``val`` counter value to be set.

timerSetDivider
***************

This function is used to set the divider of the timer.

.. code-block:: arduino

    void timerSetDivider(hw_timer_t *timer, uint16_t divider);

* ``timer`` timer struct.
* ``divider`` divider to be set.

timerSetCountUp
***************

This function is used to configure counting direction of the timer.

.. code-block:: arduino

    void timerSetCountUp(hw_timer_t *timer, bool countUp);

* ``timer`` timer struct.
* ``countUp`` select counting direction (``true`` = increment).

timerSetAutoReload
******************

This function is used to set counter value of the timer.

.. code-block:: arduino

    void timerSetAutoReload(hw_timer_t *timer, bool autoreload);

* ``timer`` timer struct.
* ``autoreload`` select autoreload (``true`` = enabled).

timerStarted
************

This function is used to get if the timer is running.

.. code-block:: arduino

    bool timerStarted(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``true`` if the timer is running. If ``false`` is returned, timer is stopped.

timerRead
*********

This function is used to read counter value of the timer.

.. code-block:: arduino

    uint64_t timerRead(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer.

timerReadMicros
***************

This function is used to read counter value in microseconds of the timer.

.. code-block:: arduino

    uint64_t timerReadMicros(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer in microseconds.

timerReadMilis
**************

This function is used to read counter value in miliseconds of the timer.

.. code-block:: arduino

    uint64_t timerReadMilis(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer in miliseconds.

timerReadSeconds
****************

This function is used to read counter value in seconds of the timer.

.. code-block:: arduino

    double timerReadSeconds(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer in seconds.

timerGetDivider
***************

This function is used to get divider of the timer.

.. code-block:: arduino

    uint16_t timerGetDivider(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``divider`` of the timer.

timerGetCountUp
***************

This function is used get counting direction of the timer.

.. code-block:: arduino

    bool timerGetCountUp(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``true`` if the timer counting direction is UP (incrementing).
If ``false`` returned, the timer counting direction is DOWN (decrementing).

timerGetAutoReload
******************

This function is used to get configuration of auto reload of the timer.

.. code-block:: arduino

    bool timerGetAutoReload(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``true`` if the timer auto reload is enabled.
If ``false`` returned, the timer auto reload is disabled.

timerAlarmEnable
****************

This function is used to enable generation of timer alarm events.

.. code-block:: arduino

    void timerAlarmEnable(hw_timer_t *timer);

* ``timer`` timer struct.

timerAlarmDisable
*****************

This function is used to disable generation of timer alarm events.

.. code-block:: arduino

    void timerAlarmDisable(hw_timer_t *timer);

* ``timer`` timer struct.
  
timerAlarmWrite
***************

This function is used to configure alarm value and autoreload of the timer.

.. code-block:: arduino

    void timerAlarmWrite(hw_timer_t *timer, uint64_t alarm_value, bool autoreload);

* ``timer`` timer struct.
* ``alarm_value`` alarm value to generate event.
* ``autoreload`` enabled/disabled autorealod.

timerAlarmEnabled
*****************

This function is used to get status of timer alarm.

.. code-block:: arduino

    bool timerAlarmEnabled(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``true`` if the timer alarm is enabled.
If ``false`` returned, the timer alarm is disabled.

timerAlarmRead
**************

This function is used to read alarm value of the timer.

.. code-block:: arduino

    uint64_t timerAlarmRead(hw_timer_t *timer);

* ``timer`` timer struct.

timerAlarmReadMicros
********************

This function is used to read alarm value of the timer in microseconds.

.. code-block:: arduino

    uint64_t timerAlarmReadMicros(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``alarm value`` of the timer in microseconds.

timerAlarmReadSeconds
*********************

This function is used to read alarm value of the timer in seconds.

.. code-block:: arduino

    double timerAlarmReadSeconds(hw_timer_t *timer);

* ``timer`` timer struct.

This function will return ``alarm value`` of the timer in seconds.

Example Applications
********************

There are 2 examples uses of Timer:

Repeat timer example:

.. literalinclude:: ../../../libraries/ESP32/examples/Timer/RepeatTimer/RepeatTimer.ino
    :language: arduino

Watchdog timer example:

.. literalinclude:: ../../../libraries/ESP32/examples/Timer/WatchdogTimer/WatchdogTimer.ino
    :language: arduino