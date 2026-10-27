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

timerReadMillis
***************

This function is used to read counter value in milliseconds of the timer.

.. code-block:: arduino

    uint64_t timerReadMillis(hw_timer_t * timer);

* ``timer`` timer struct.

This function will return ``counter value`` of the timer in milliseconds.

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
* ``userFunc`` function to be called when interrupt is triggered.

The C++ overload accepts ``std::function<void(void)>``, so capturing lambdas and bound member functions can be used as the callback. Include ``FunctionalInterrupt.h`` to use this overload. It is available only when the SoC has a general-purpose timer (``SOC_GPTIMER_SUPPORTED``).

.. code-block:: arduino

    #include <FunctionalInterrupt.h>

    void timerAttachInterrupt(hw_timer_t * timer, std::function<void(void)> userFunc);

A function pointer matches the C overload. A capturing lambda matches the C++ overload. A non-capturing lambda can convert to both; capture a variable or wrap the lambda in ``std::function<void(void)>`` to select the C++ overload.

Timer callbacks run in ISR context. Keep the handler short. Use ``volatile`` for data shared with ``loop()``.

Do not declare a ``std::function`` object with ``IRAM_ATTR``. That places a writable C++ object in instruction RAM. On RISC-V targets (ESP32-C6 and similar) the constructor then store-faults during ``do_global_ctors`` before ``setup()`` runs. For an IRAM-safe callback, use a C function pointer marked ``ARDUINO_ISR_ATTR``. ``std::function::operator()`` and most lambda bodies stay in flash.

Re-attaching a callback replaces the previous one. ``timerDetachInterrupt`` and ``timerEnd`` release any heap-allocated ``std::function`` state.

.. code-block:: arduino

    #include <Arduino.h>
    #include <FunctionalInterrupt.h>

    hw_timer_t *timer = NULL;
    volatile uint32_t counter = 0;

    void setup() {
      Serial.begin(115200);
      timer = timerBegin(1000000);
      timerAttachInterrupt(timer, [&]() {
        counter = counter + 1;
      });
      timerAlarm(timer, 1000000, true, 0);
    }

    void loop() {
      Serial.printf("Counter: %" PRIu32 "\n", counter);
      delay(1000);
    }

timerAttachInterruptArg
***********************

This function is used to attach interrupt to timer using arguments.

.. code-block:: arduino

    void timerAttachInterruptArg(hw_timer_t * timer, void (*userFunc)(void*), void * arg);

* ``timer`` timer struct.
* ``userFunc`` function to be called when interrupt is triggered.
* ``arg`` pointer to the interrupt arguments.

timerDetachInterrupt
********************

This function is used to detach interrupt from timer.

.. code-block:: arduino

    void timerDetachInterrupt(hw_timer_t * timer);

* ``timer`` timer struct.

timerAlarm
**********

This function is used to configure alarm value and autoreload of the timer. Alarm is automatically enabled.

.. code-block:: arduino

    void timerAlarm(hw_timer_t * timer, uint64_t alarm_value, bool autoreload, uint64_t reload_count);

* ``timer`` timer struct.
* ``alarm_value`` alarm value to generate event.
* ``autoreload`` enabled/disabled autorealod.
* ``reload_count`` number of autoreloads (0 = unlimited). Has no effect if autorealod is disabled.

Example Applications
********************

There are 3 example uses of Timer:

Repeat timer example:

.. literalinclude:: ../../../libraries/ESP32/examples/Timer/RepeatTimer/RepeatTimer.ino
    :language: arduino

Watchdog timer example:

.. literalinclude:: ../../../libraries/ESP32/examples/Timer/WatchdogTimer/WatchdogTimer.ino
    :language: arduino

Functional interrupt (``std::function`` / lambda) example:

.. literalinclude:: ../../../libraries/ESP32/examples/Timer/FunctionalInterrupt/FunctionalInterrupt.ino
    :language: arduino
