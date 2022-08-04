###
ADC
###

About
-----

ADC (analog to digital converter) is a very common peripheral used to convert an analog signal such as voltage 
to a digital form so that it can be read and processed by a microcontroller. 

ADCs are very useful in control and monitoring applications since most sensors 
(e.g., temperature, pressure, force) produce analogue output voltages.

.. note:: Each SoC or module has a different number of ADC's with a different number of channels and pins availible. Refer to datasheet of each board for more info.

Arduino-ESP32 ADC API
---------------------

ADC common API
**************

analogRead
^^^^^^^^^^

This function is used to get the ADC raw value for a given pin/ADC channel.

.. code-block:: arduino

    uint16_t analogRead(uint8_t pin);

* ``pin`` GPIO pin to read analog value
  
This function will return analog raw value.

analogReadMillivolts
^^^^^^^^^^^^^^^^^^^^

This function is used to get ADC value for a given pin/ADC channel in millivolts.

.. code-block:: arduino

    uint32_t analogReadMilliVolts(uint8_t pin);

* ``pin`` GPIO pin to read analog value

This function will return analog value in millivolts.

analogReadResolution
^^^^^^^^^^^^^^^^^^^^

This function is used to set the resolution of ``analogRead`` return value. Default is 12 bits (range from 0 to 4096) 
for all chips except ESP32S3 where default is 13 bits (range from 0 to 8192). 
When different resolution is set, the values read will be shifted to match the given resolution.

Range is 1 - 16 .The default value will be used, if this function is not used.

.. note:: For the ESP32, the resolution is between 9 to12 and it will change the ADC hardware resolution. Else value will be shifted.

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

Input voltages can be attenuated before being input to the ADCs.
There are 4 available attenuation options, the higher the attenuation is, the higher the measurable input voltage could be.

The measurable input voltage differs for each chip, see table below for detailed information.

.. tabs::

   .. tab:: ESP32

      =====================  ===========================================
      Attenuation            Measurable input voltage range
      =====================  ===========================================
      ``ADC_ATTEN_DB_0``     100 mV ~ 950 mV
      ``ADC_ATTEN_DB_2_5``   100 mV ~ 1250 mV
      ``ADC_ATTEN_DB_6``     150 mV ~ 1750 mV
      ``ADC_ATTEN_DB_11``    150 mV ~ 2450 mV
      =====================  ===========================================

   .. tab:: ESP32-S2

      =====================  ===========================================
      Attenuation            Measurable input voltage range
      =====================  ===========================================
      ``ADC_ATTEN_DB_0``     0 mV ~ 750 mV
      ``ADC_ATTEN_DB_2_5``   0 mV ~ 1050 mV
      ``ADC_ATTEN_DB_6``     0 mV ~ 1300 mV
      ``ADC_ATTEN_DB_11``    0 mV ~ 2500 mV
      =====================  ===========================================

   .. tab:: ESP32-C3

      =====================  ===========================================
      Attenuation            Measurable input voltage range
      =====================  ===========================================
      ``ADC_ATTEN_DB_0``     0 mV ~ 750 mV
      ``ADC_ATTEN_DB_2_5``   0 mV ~ 1050 mV
      ``ADC_ATTEN_DB_6``     0 mV ~ 1300 mV
      ``ADC_ATTEN_DB_11``    0 mV ~ 2500 mV
      =====================  ===========================================

   .. tab:: ESP32-S3

      =====================  ===========================================
      Attenuation            Measurable input voltage range
      =====================  ===========================================
      ``ADC_ATTEN_DB_0``     0 mV ~ 950 mV
      ``ADC_ATTEN_DB_2_5``   0 mV ~ 1250 mV
      ``ADC_ATTEN_DB_6``     0 mV ~ 1750 mV
      ``ADC_ATTEN_DB_11``    0 mV ~ 3100 mV
      =====================  ===========================================

.. code-block:: arduino

    void analogSetAttenuation(adc_attenuation_t attenuation);

* ``attenuation`` sets the attenuation.

analogSetPinAttenuation
^^^^^^^^^^^^^^^^^^^^^^^

This function is used to set the attenuation for a specific pin/ADC channel. For more information refer to `analogSetAttenuation`_.

.. code-block:: arduino

    void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation);

* ``pin`` selects specific pin for attenuation settings.
* ``attenuation`` sets the attenuation.
      
adcAttachPin
^^^^^^^^^^^^

This function is used to attach the pin to ADC (it will also clear any other analog mode that could be on)

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

This function is used to get the ADC value of the HALL sensor conneted to pins 36(SVP) and 39(SVN).

.. code-block:: arduino

    int hallRead();
    
This function will return the hall sensor value.


Example Applications
********************

Here is an example of how to use the ADC.

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogRead/AnalogRead.ino
    :language: arduino

Or you can run Arduino example 01.Basics -> AnalogReadSerial.