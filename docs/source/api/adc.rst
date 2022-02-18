###
ADC
###

About
-----

ADC (analog to digital converter) is a very common peripheral used to convert an analogue signal such as voltage 
to a digital form so that it can be read and processed by a microcontroller. 

ADCs are very useful in control and monitoring applications since most sensors 
(e.g., temperature sensor, pressure sensor, force sensor) produce analogue output voltages.

.. note:: Each board have ADC different number of ADC channels and pins availible. Refer to datasheet of each board for more info.

Arduino-ESP32 ADC API
---------------------

ADC common API
**************

analogRead
^^^^^^^^^^

This function is used to get ADC value for pin.

.. code-block:: arduino

    uint16_t analogRead(uint8_t pin);

* ``pin`` GPIO pin to read analog value
  
This function will return analog value.

analogReadMillivolts
^^^^^^^^^^^^^^^^^^^^

This function is used to get ADC value for pin in millivolts.

.. code-block:: arduino

    uint32_t analogReadMilliVolts(uint8_t pin);

* ``pin`` GPIO pin to read analog value

This function will return analog value in millivolts.

analogReadResolution
^^^^^^^^^^^^^^^^^^^^

This function is used to set the resolution of analogRead return value. Default is 12 bits (range from 0 to 4096) 
for all chips except ESP32S3 where default is 13 bits (range from 0 to 8192). 
When different resolution is set, the values read will be shifted to match the given resolution.

Range is 1 - 16 .The default value will be used, if this function is not used.

.. note:: For ESP32 resolution between 9 and 12 will change ADC hardware resolution. Else value will be shifted.

.. code-block:: arduino

    void analogReadResolution(uint8_t bits);

* ``bits`` sets analog read resolution

analogSetClockDiv
^^^^^^^^^^^^^^^^^

This function is used to set the divider for the ADC clock.

Range is 1 - 255. Default value is 1.

.. code-block:: arduino

    void analogSetClockDiv(uint8_t clockDiv);

* ``clockDiv`` sets the divider for ADC clock.

analogSetAttenuation
^^^^^^^^^^^^^^^^^^^^

This function is used to set the attenuation for all channels.

Default is 11db.

.. code-block:: arduino

    void analogSetAttenuation(adc_attenuation_t attenuation);

* ``attenuation`` sets the attenuation.
  
  * can be set to ADC_0db, ADC_2_5db, ADC_6db or ADC_11db.

analogSetPinAttenuation
^^^^^^^^^^^^^^^^^^^^^^^

This function is used to set the attenuation for a specific pin.

Default is 11db.

.. code-block:: arduino

    void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation);

* ``pin`` selects specific pin for attenuation settings.
* ``attenuation`` sets the attenuation.

  * can be set to ADC_0db, ADC_2_5db, ADC_6db or ADC_11db.
      
adcAttachPin
^^^^^^^^^^^^

This function is used to attach pin to ADC (it will also clear any other analog mode that could be on)

.. code-block:: arduino

    bool adcAttachPin(uint8_t pin);

This function will return ``true`` if configuration is successful. Else returns ``false``.

ADC API specific for ESP32 chip
*******************************

analogSetWidth
^^^^^^^^^^^^^^

This function is used to set the hardware sample bits and read resolution.
Default is 12bit (0 - 4095).
Range is 9 - 12.

.. code-block:: arduino

    void analogSetWidth(uint8_t bits);
 
analogSetVRefPin
^^^^^^^^^^^^^^^^

This function is used to set pin to use for ADC calibration if the esp is not already calibrated (pins 25, 26 or 27).

.. code-block:: arduino

    void analogSetVRefPin(uint8_t pin);

* ``pin`` GPIO pin to set VRefPin for ADC calibration
  
hallRead
^^^^^^^^

This function is used to get ADC value of HALL sensor conneted to pins 36(SVP) and 39(SVN).

.. code-block:: arduino

    int hallRead();
    
This function will return hall sensors value.


Example Applications
********************

Here is an example of how to use the ADC.

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogRead/AnalogRead.ino
    :language: arduino

Or you can run Arduino example 01.Basics -> AnalogReadSerial.