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
ESP32-S3  4
ESP32-C3  2
ESP32-C6  2
ESP32-H2  2
========= ================

Arduino-ESP32 Timer API
-----------------------

timerBegin
**********

This function is used to configure the timer. After successful setup the timer will automatically start.

.. code-block:: arduino

    hw_timer_t * timerBegin(uint32_t frequency);

* ``frequency`` select timer frequency in Hz. Sets how quickly the timer counter is “ticking”.

This function will return ``timer`` structure if configuration is successful.
If ``NULL`` is returned, error occurs and the timer was not configured.

timerEnd
********

This function is used to end timer.

.. code-block:: arduino

    void timerEnd(hw_timer_t * timer);

* ``timer`` timer struct.

timerStart
**********

This function is used to start counter of the timer.

.. code-block:: arduino

    void timerStart(hw_timer_t * timer);

* ``timer`` timer struct.

timerStop
*********

This function is used to stop counter of the timer.

.. code-block:: arduino

    void timerStop(hw_timer_t * timer);

* ``timer`` timer struct.

timerRestart
************

This function is used to restart counter of the timer.

.. code-block:: arduino

    void timerRestart(hw_timer_t * timer);

* ``timer`` timer struct.

timerWrite
**********

This function is used to set counter value of the timer.

.. code-block:: arduino

    void timerWrite(hw_timer_t * timer, uint64_t val);

* ``timer`` timer struct.
* ``val`` counter value to be set.

timerRead
*********

This function is used to read counter value of the timer.

.. code-block:: arduino

    uint64_t timerRead(hw_timer_t * timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer.

timerReadMicros
***************

This function is used to read counter value in microseconds of the timer.

.. code-block:: arduino

    uint64_t timerReadMicros(hw_timer_t * timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer in microseconds.

timerReadMilis
**************

This function is used to read counter value in miliseconds of the timer.

.. code-block:: arduino

    uint64_t timerReadMilis(hw_timer_t * timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer in miliseconds.

timerReadSeconds
****************

This function is used to read counter value in seconds of the timer.

.. code-block:: arduino

    double timerReadSeconds(hw_timer_t * timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer in seconds.

timerGetFrequency
*****************

This function is used to get resolution in Hz of the timer.

.. code-block:: arduino

    uint16_t timerGetFrequency(hw_timer_t * timer);

* ``timer`` timer struct.

This function will return ``frequency`` in Hz of the timer.
  
timerAttachInterrupt
********************

This function is used to attach interrupt to timer.

.. code-block:: arduino

    void timerAttachInterrupt(hw_timer_t * timer, void (*userFunc)(void));

* ``timer`` timer struct.
* ``userFunc`` funtion to be called when interrupt is triggered.

timerAttachInterruptArg
***********************

This function is used to attach interrupt to timer using arguments.

.. code-block:: arduino

    void timerAttachInterruptArg(hw_timer_t * timer, void (*userFunc)(void*), void * arg);

* ``timer`` timer struct.
* ``userFunc`` funtion to be called when interrupt is triggered.
* ``arg`` pointer to the interrupt arguments.

timerDetachInterrupt
********************

This function is used to detach interrupt from timer.

.. code-block:: arduino

    void timerDetachInterrupt(hw_timer_t * timer);

* ``timer`` timer struct.

timerAlarm
**********

This function is used to configure alarm value and autoreload of the timer. Alarm is automaticaly enabled.

.. code-block:: arduino

    void timerAlarm(hw_timer_t * timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count);

* ``timer`` timer struct.
* ``alarm_value`` alarm value to generate event.
* ``autoreload`` enabled/disabled autorealod.
* ``reload_count`` number of autoreloads (0 = unlimited). Has no effect if autorealod is disabled.

Example Applications
********************

There are 2 examples uses of Timer:

Repeat timer example:

.. literalinclude:: ../../../libraries/ESP32/examples/Timer/RepeatTimer/RepeatTimer.ino
    :language: arduino

Watchdog timer example:

.. literalinclude:: ../../../libraries/ESP32/examples/Timer/WatchdogTimer/WatchdogTimer.ino
    :language: arduino
