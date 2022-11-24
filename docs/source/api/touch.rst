#####
TOUCH
#####

About
-----

Touch sensor is a peripheral, that has an internal oscilator circuit and it measures charge/discharge frequency over a fixed period of time on respective GPIO pins. 
Therefore these touch sensors are also known as capacitive sensors. For example, if you touch any of these pins, finger electrical charge will change this number of cycles, 
by changing the RC circuit attached to the touch sensor. The TouchRead() will return the number of cycles (charges/discharges) in a certain time (meas). 
The change of this count will be used to validate if a touch has happened or not. These pins can be easily integrated into capacitive pads, and replace mechanical buttons.

.. note:: Touch peripheral is not present in every SoC. Refer to datasheet of each chip for more info.

Arduino-ESP32 TOUCH API
-----------------------

TOUCH common API
****************

touchRead
^^^^^^^^^

This function gets the touch sensor data. Each touch sensor has a counter to count the number of charge/discharge cycles. When the pad is ‘touched’, 
the value in the counter will change because of the larger equivalent capacitance. The change of the data determines if the pad has been touched or not.

.. code-block:: arduino

    touch_value_t touchRead(uint8_t pin);

* ``pin`` GPIO pin to read TOUCH value
  
This function will return touch pad value as uint16_t (ESP32) or uint32_t (ESP32-S2/S3).

touchSetCycles
^^^^^^^^^^^^^^

This function is used to set cycles that measurement operation takes. The result from touchRead, threshold and detection accuracy depend on these values.
The defaults are setting touchRead to take ~0.5ms.

.. code-block:: arduino

    void touchSetCycles(uint16_t measure, uint16_t sleep);

* ``measure`` Sets the time that it takes to measure touch sensor value
* ``sleep`` Sets waiting time before next measure cycle

touchAttachInterrupt
^^^^^^^^^^^^^^^^^^^^

This function is used to attach interrupt to the touch pad. The function will be called if a touch sensor value falls below the given 
threshold for ESP32 or rises above the given threshold for ESP32-S2/S3. To determine a proper threshold value between touched and untouched state, 
use touchRead() function.

.. code-block:: arduino

    void touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), touch_value_t threshold);

* ``pin`` GPIO TOUCH pad pin
* ``userFunc`` Function to be called when interrupt is triggered
* ``threshold`` Sets the threshold when to call interrupt

touchAttachInterruptArg
^^^^^^^^^^^^^^^^^^^^^^^

This function is used to attach interrupt to the touch pad. In the function called by ISR you have the given arguments available.

.. code-block:: arduino

    void touchAttachInterruptArg(uint8_t pin, void (*userFunc)(void*), void *arg, touch_value_t threshold);

* ``pin`` GPIO TOUCH pad pin
* ``userFunc`` Function to be called when interrupt is triggered
* ``arg`` Sets arguments to the interrupt
* ``threshold`` Sets the threshold when to call interrupt

touchDetachInterrupt
^^^^^^^^^^^^^^^^^^^^

This function is used to detach interrupt from the touch pad.

.. code-block:: arduino

    void touchDetachInterrupt(uint8_t pin);

* ``pin`` GPIO TOUCH pad pin.

touchSleepWakeUpEnable
^^^^^^^^^^^^^^^^^^^^^^

This function is used to setup touch pad as the wake up source from the deep sleep.

.. note:: ESP32-S2 and ESP32-S3 only support one sleep wake up touch pad.

.. code-block:: arduino

    void touchSleepWakeUpEnable(uint8_t pin, touch_value_t threshold);

* ``pin`` GPIO TOUCH pad pin
* ``threshold`` Sets the threshold when to wake up

TOUCH API specific for ESP32 chip (TOUCH_V1)
********************************************

touchInterruptSetThresholdDirection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This function is used to tell the driver if it shall activate the interrupt if the sensor is lower or higher than
the threshold value. Default is lower.

.. code-block:: arduino

    void touchInterruptSetThresholdDirection(bool mustbeLower);

TOUCH API specific for ESP32S2 and ESP32S3 chip (TOUCH_V2)
**********************************************************

touchInterruptGetLastStatus
^^^^^^^^^^^^^^^^^^^^^^^^^^^

This function is used get the lastest ISR status for the touch pad.

.. code-block:: arduino

    bool touchInterruptGetLastStatus(uint8_t pin);

This function returns true if the touch pad has been and continues pressed or false otherwise.  

Example Applications
********************

Example of reading the touch sensor.

.. literalinclude:: ../../../libraries/ESP32/examples/Touch/TouchRead/TouchRead.ino
    :language: arduino

A usage example for the touch interrupts.

.. literalinclude:: ../../../libraries/ESP32/examples/Touch/TouchInterrupt/TouchInterrupt.ino
    :language: arduino

More examples can be found in our repository -> `Touch examples <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Touch>`_. 