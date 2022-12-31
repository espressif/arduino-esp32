##################
LED Control (LEDC)
##################

About
-----
The LED control (LEDC) peripheral is primarly designed to control the intensity of LEDs, 
although it can also be used to generate PWM signals for other purposes. 

ESP32 SoCs has from 6 to 16 channels (variates on socs, see table below) which can generate independent waveforms, that can be used for example to drive RGB LED devices.

========= =======================
ESP32 SoC Number of LEDC channels
========= =======================
ESP32     16
ESP32-S2  8
ESP32-C3  6
ESP32-S3  8
========= =======================

Arduino-ESP32 LEDC API
----------------------

ledcSetup
*********

This function is used to setup the LEDC channel frequency and resolution.

.. code-block:: arduino

    uint32_t ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolution_bits);

* ``channel`` select LEDC channel to config.
* ``freq`` select frequency of pwm.
* ``resolution_bits`` select resolution for ledc channel. 
 
  * range is 1-14 bits (1-20 bits for ESP32).
  
This function will return ``frequency`` configured for LEDC channel.
If ``0`` is returned, error occurs and ledc channel was not configured.

ledcWrite
*********

This function is used to set duty for the LEDC channel.

.. code-block:: arduino

    void ledcWrite(uint8_t chan, uint32_t duty);

* ``chan`` select the LEDC channel for writing duty.
* ``duty`` select duty to be set for selected channel.

ledcRead
********

This function is used to get configured duty for the LEDC channel.

.. code-block:: arduino

    uint32_t ledcRead(uint8_t chan);

* ``chan`` select LEDC channel to read the configured duty.

This function will return ``duty`` set for selected LEDC channel.

ledcReadFreq
************

This function is used to get configured frequency for the LEDC channel.

.. code-block:: arduino

    uint32_t ledcReadFreq(uint8_t chan);

* ``chan`` select the LEDC channel to read the configured frequency.

This function will return ``frequency`` configured for selected LEDC channel.

ledcWriteTone
*************

This function is used to setup the LEDC channel to 50 % PWM tone on selected frequency.

.. code-block:: arduino

    uint32_t ledcWriteTone(uint8_t chan, uint32_t freq);

* ``chan`` select LEDC channel.
* ``freq`` select frequency of pwm signal.

This function will return ``frequency`` set for channel.
If ``0`` is returned, error occurs and ledc cahnnel was not configured.

ledcWriteNote
*************

This function is used to setup the LEDC channel to specific note.

.. code-block:: arduino

    uint32_t ledcWriteNote(uint8_t chan, note_t note, uint8_t octave);

* ``chan`` select LEDC channel.
* ``note`` select note to be set.
  
======= ======= ======= ======= ======= ======
NOTE_C  NOTE_Cs NOTE_D  NOTE_Eb NOTE_E  NOTE_F
NOTE_Fs NOTE_G  NOTE_Gs NOTE_A  NOTE_Bb NOTE_B
======= ======= ======= ======= ======= ======

* ``octave`` select octave for note.

This function will return ``frequency`` configured for the LEDC channel according to note and octave inputs.
If ``0`` is returned, error occurs and the LEDC channel was not configured.
      
ledcAttachPin
*************

This function is used to attach the pin to the LEDC channel.

.. code-block:: arduino

    void ledcAttachPin(uint8_t pin, uint8_t chan);

* ``pin`` select GPIO pin.
* ``chan`` select LEDC channel.

ledcDetachPin
*************

This function is used to detach the pin from LEDC.

.. code-block:: arduino

    void ledcDetachPin(uint8_t pin);

* ``pin`` select GPIO pin.

ledcChangeFrequency
*******************

This function is used to set frequency for the LEDC channel.

.. code-block:: arduino

    uint32_t ledcChangeFrequency(uint8_t chan, uint32_t freq, uint8_t bit_num);

* ``channel`` select LEDC channel.
* ``freq`` select frequency of pwm.
* ``bit_num`` select resolution for LEDC channel. 
 
  * range is 1-14 bits (1-20 bits for ESP32).
  
This function will return ``frequency`` configured for the LEDC channel.
If ``0`` is returned, error occurs and the LEDC channel frequency was not set.

analogWrite
***********

This function is used to write an analog value (PWM wave) on the pin.
It is compatible with Arduinos analogWrite function.

.. code-block:: arduino

    void analogWrite(uint8_t pin, int value);

* ``pin`` select the GPIO pin.
* ``value`` select the duty cycle of pwm.
  * range is from 0 (always off) to 255 (always on).

analogWriteResolution
*********************

This function is used to set resolution for all analogWrite channels.

.. code-block:: arduino

    void analogWriteResolution(uint8_t bits);
   
* ``bits`` select resolution for analog channels. 

analogWriteFrequency
********************

This function is used to set frequency for all analogWrite channels.

.. code-block:: arduino

    void analogWriteFrequency(uint32_t freq);

* ``freq`` select frequency of pwm.

Example Applications
********************

LEDC software fade example:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/LEDCSoftwareFade/LEDCSoftwareFade.ino
    :language: arduino

LEDC Write RGB example:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/ledcWrite_RGB/ledcWrite_RGB.ino
    :language: arduino
