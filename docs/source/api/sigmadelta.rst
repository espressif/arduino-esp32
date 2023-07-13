##########
SigmaDelta
##########

About
-----

ESP32 provides a second-order sigma delta modulation module and 8 (4 for ESP32-C3)
independent modulation channels. The channels are capable to output 1-bit 
signals (output index: 100 ~ 107) with sigma delta modulation.

========= =============================
ESP32 SoC Number of SigmaDelta channels
========= =============================
ESP32     8
ESP32-S2  8
ESP32-S3  8
ESP32-C3  4
ESP32-C6  4
ESP32-H2  4
========= =============================

Arduino-ESP32 SigmaDelta API
----------------------------

sigmaDeltaAttach
****************

This function is used to setup the SigmaDelta channel with selected frequency and attach it to selected pin.

.. code-block:: arduino

    bool sigmaDeltaAttach(uint8_t pin, uint32_t freq);

* ``pin`` select GPIO pin.
* ``freq`` select frequency.

  * range is 1-14 bits (1-20 bits for ESP32).
  
This function will return ``true`` if configuration is successful.
If ``false`` is returned, error occurs and the SigmaDelta channel was not configured.

sigmaDeltaWrite
***************

This function is used to set duty for the SigmaDelta pin.

.. code-block:: arduino

    bool sigmaDeltaWrite(uint8_t pin, uint8_t duty);

* ``pin`` select GPIO pin.
* ``duty`` select duty to be set for selected pin.

This function will return ``true`` if setting duty is successful.
If ``false`` is returned, error occurs and duty was not set.

sigmaDeltaDetach
****************

This function is used to detach pin from SigmaDelta and deinit the channel that was attached to the pin.

.. code-block:: arduino

    bool sigmaDeltaDetach(uint8_t pin);

* ``pin`` select GPIO pin.

This function will return ``true`` if detaching is successful.
If ``false`` is returned, error occurs and pin is not detached.

Example Applications
********************

Here is example use of SigmaDelta:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/SigmaDelta/SigmaDelta.ino
    :language: arduino
