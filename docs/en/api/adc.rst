###
ADC
###

About
-----

ADC (analog to digital converter) is a very common peripheral used to convert an analog signal such as voltage
to a digital form so that it can be read and processed by a microcontroller.

ADCs are very useful in control and monitoring applications since most sensors
(e.g., temperature, pressure, force) produce analog output voltages.

.. note:: Each SoC or module has a different number of ADC's with a different number of channels and pins available. Refer to datasheet of each board for more info.

Arduino-ESP32 ADC API
---------------------

ADC OneShot mode
****************


The ADC OneShot mode API is fully compatible with Arduino's ``analogRead`` function.
When you call the ``analogRead`` or ``analogReadMillivolts`` function, it returns the result of a single conversion on the requested pin.

analogRead
^^^^^^^^^^

This function is used to get the ADC raw value for a given pin/ADC channel.

.. code-block:: arduino

    uint16_t analogRead(uint8_t pin);

* ``pin`` GPIO pin to read analog value

This function will return analog raw value (non-calibrated).

analogReadMillivolts
^^^^^^^^^^^^^^^^^^^^

This function is used to get ADC raw value for a given pin/ADC channel and convert it to calibrated result in millivolts.

.. code-block:: arduino

    uint32_t analogReadMilliVolts(uint8_t pin);

* ``pin`` GPIO pin to read analog value

This function will return analog value in millivolts (calibrated).

analogReadResolution
^^^^^^^^^^^^^^^^^^^^

This function is used to set the resolution of ``analogRead`` return value. Default is 12 bits (range from 0 to 4095)
for all chips except ESP32S3 where default is 13 bits (range from 0 to 8191).
When different resolution is set, the values read will be shifted to match the given resolution.

Range is 1 - 16 .The default value will be used, if this function is not used.

.. note:: For the ESP32, the resolution is between 9 to12 and it will change the ADC hardware resolution. Else value will be shifted.

.. code-block:: arduino

    void analogReadResolution(uint8_t bits);

* ``bits`` sets analog read resolution

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
      ``ADC_ATTEN_DB_11``    150 mV ~ 3100 mV
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

analogSetWidth
^^^^^^^^^^^^^^

.. note:: This function is only available for ESP32 chip.

This function is used to set the hardware sample bits and read resolution.
Default is 12bit (0 - 4095).
Range is 9 - 12.

.. code-block:: arduino

    void analogSetWidth(uint8_t bits);

ADC Continuous mode
*******************

ADC Continuous mode is an API designed for performing analog conversions on multiple pins in the background,
with the feature of receiving a callback upon completion of these conversions to access the results.

This API allows you to specify the desired number of conversions per pin within a single cycle, along with its corresponding sampling rate.
The outcome of the ``analogContinuousRead`` function is an array of ``adc_continuous_data_t`` structures.
These structures hold both the raw average value and the average value in millivolts for each pin.

analogContinuous
^^^^^^^^^^^^^^^^

This function is used to configure ADC continuous peripheral on selected pins.

.. code-block:: arduino

    bool analogContinuous(uint8_t pins[], size_t pins_count, uint32_t conversions_per_pin, uint32_t sampling_freq_hz, void (*userFunc)(void));

* ``pins[]`` array of pins to be set up
* ``pins_count`` count of pins in array
* ``conversions_per_pin`` sets how many conversions per pin will run each ADC cycle
* ``sampling_freq_hz`` sets sampling frequency of ADC in Hz
* ``userFunc`` sets callback function to be called after adc conversion is done (can be set to ``NULL``)

This function will return ``true`` if configuration is successful.
If ``false`` is returned, error occurs and ADC continuous was not configured.

analogContinuousRead
^^^^^^^^^^^^^^^^^^^^

This function is used to read ADC continuous data to the result buffer. The result buffer is an array of ``adc_continuos_data_t``.

.. code-block:: arduino

    typedef struct {
        uint8_t pin;           /*!<ADC pin */
        uint8_t channel;       /*!<ADC channel */
        int avg_read_raw;      /*!<ADC average raw data */
        int avg_read_mvolts;   /*!<ADC average voltage in mV */
    } adc_continuos_data_t;

.. code-block:: arduino

    bool analogContinuousRead(adc_continuos_data_t ** buffer, uint32_t timeout_ms);

* ``buffer`` conversion result buffer to read from ADC in adc_continuos_data_t format.
* ``timeout_ms`` time to wait for data in milliseconds.

This function will return ``true`` if reading is successful and ``buffer`` is filled with data.
If ``false`` is returned, reading has failed and ``buffer`` is set to NULL.

analogContinuousStart
^^^^^^^^^^^^^^^^^^^^^

This function is used to start ADC continuous conversions.

.. code-block:: arduino

    bool analogContinuousStart();

This function will return ``true`` if ADC continuous is successfully started.
If ``false`` is returned, starting ADC continuous has failed.

analogContinuousStop
^^^^^^^^^^^^^^^^^^^^

This function is used to stop ADC continuous conversions.

.. code-block:: arduino

    bool analogContinuousStop();

This function will return ``true`` if ADC continuous is successfully stopped.
If ``false`` is returned, stopping ADC continuous has failed.

analogContinuousDeinit
^^^^^^^^^^^^^^^^^^^^^^

This function is used to deinitialize ADC continuous peripheral.

.. code-block:: arduino

    bool analogContinuousDeinit();

This function will return ``true`` if ADC continuous is successfully deinitialized.
If ``false`` is returned, deinitilization of ADC continuous has failed.

analogContinuousSetAtten
^^^^^^^^^^^^^^^^^^^^^^^^

This function is used to set the attenuation for ADC continuous peripheral. For more information refer to `analogSetAttenuation`_.

.. code-block:: arduino

    void analogContinuousSetAtten(adc_attenuation_t attenuation);

* ``attenuation`` sets the attenuation (default is 11db).

analogContinuousSetWidth
^^^^^^^^^^^^^^^^^^^^^^^^

This function is used to set the hardware resolution bits.
Default value for all chips is 12bit (0 - 4095).

.. note:: This function will take effect only for ESP32 chip, as it allows to set resolution in range 9-12 bits.

.. code-block:: arduino

    void analogContinuousSetWidth(uint8_t bits);

* ``bits`` sets resolution bits.


Example Applications
********************

Here is an example of how to use the ADC in OneShot mode or you can run Arduino example 01.Basics -> AnalogReadSerial.

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogRead/AnalogRead.ino
    :language: arduino

Here is an example of how to use the ADC in Continuous mode.

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogReadContinuous/AnalogReadContinuous.ino
    :language: arduino
