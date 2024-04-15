###
DAC
###

About
-----

DAC (digital to analog converter) is a very common peripheral used to convert a digital signal to an
analog form.

ESP32 and ESP32-S2 have two 8-bit DAC channels. The DAC driver allows these channels to be set to arbitrary voltages.

DACs can be used for generating a specific (and dynamic) reference voltage for external sensors,
controlling transistors, etc.

========= ========= =========
ESP32 SoC DAC_1 pin DAC_2 pin
========= ========= =========
ESP32     GPIO 25   GPIO 26
ESP32-S2  GPIO 17   GPIO 18
========= ========= =========

Arduino-ESP32 DAC API
---------------------

dacWrite
********

This function is used to set the DAC value for a given pin/DAC channel.

.. code-block:: arduino

    void dacWrite(uint8_t pin, uint8_t value);

* ``pin`` GPIO pin.
* ``value`` to be set. Range is 0 - 255 (equals 0V - 3.3V).

dacDisable
**********

This function is used to disable DAC output on a given pin/DAC channel.

.. code-block:: arduino

    void dacDisable(uint8_t pin);

* ``pin`` GPIO pin.
