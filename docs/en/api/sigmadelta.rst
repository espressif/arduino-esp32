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
ESP32-C3  4
ESP32-S3  8
========= =============================

Arduino-ESP32 SigmaDelta API
----------------------------

sigmaDeltaSetup
***************

This function is used to setup the SigmaDelta channel frequency and resolution.

.. code-block:: arduino

    uint32_t sigmaDeltaSetup(uint8_t pin, uint8_t channel, uint32_t freq);

* ``pin`` select GPIO pin.
* ``channel`` select SigmaDelta channel.
* ``freq`` select frequency.

  * range is 1-14 bits (1-20 bits for ESP32).
  
This function will return ``frequency`` configured for the SigmaDelta channel.
If ``0`` is returned, error occurs and the SigmaDelta channel was not configured.

sigmaDeltaWrite
***************

This function is used to set duty for the SigmaDelta channel.

.. code-block:: arduino

    void sigmaDeltaWrite(uint8_t channel, uint8_t duty);

* ``channel`` select SigmaDelta channel.
* ``duty`` select duty to be set for selected channel.

sigmaDeltaRead
**************

This function is used to get configured duty for the SigmaDelta channel.

.. code-block:: arduino

    uint8_t sigmaDeltaRead(uint8_t channel)

* ``channnel`` select SigmaDelta channel.

This function will return ``duty`` configured for the selected SigmaDelta channel.

sigmaDeltaDetachPin
*******************

This function is used to detach pin from SigmaDelta.

.. code-block:: arduino

    void sigmaDeltaDetachPin(uint8_t pin);

* ``pin`` select GPIO pin.

Example Applications
********************

Here is example use of SigmaDelta:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/SigmaDelta/SigmaDelta.ino
    :language: arduino
