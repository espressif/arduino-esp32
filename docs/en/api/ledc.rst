##################
LED Control (LEDC)
##################

About
-----
The LED control (LEDC) peripheral is primarily designed to control the intensity of LEDs,
although it can also be used to generate PWM signals for other purposes.

ESP32 SoCs have from 6 to 16 LEDC channels (see the table below) which can
generate independent waveforms, for example to drive RGB LED devices.

================= ======== ====== ===================
ESP32 SoC         Channels Groups Timers (per group)
================= ======== ====== ===================
ESP32             16       2      4
ESP32-S2          8        1      4
ESP32-S3          8        1      4
ESP32-C3          6        1      4
ESP32-C5          6        1      4
ESP32-C6          6        1      4
ESP32-H2          6        1      4
ESP32-P4          8        1      4
================= ======== ====== ===================

One group does **not** mean one timer. Each group has its own pool of timers
(4 on all current SoCs), so you can have up to 4 independent frequencies per
group. Channels only share a timer when they are configured with the same
frequency and resolution (see below).

Channels, timers and frequency sharing
**************************************

LEDC separates **channels** from **timers**:

* A **channel** drives one or more GPIOs and owns the duty cycle (and fade).
* A **timer** owns the PWM frequency and resolution.
* Several channels in the same group can share one timer.

``ledcAttach()`` / ``ledcAttachChannel()`` reuse an existing timer when another
channel in the **same group** is already configured with the same frequency and
resolution. That saves limited hardware timers, but means those pins stay locked
to the same frequency until they use different timers.

**Important:** choosing different channel numbers does **not** by itself give
independent frequencies. If two pins are attached with the same ``freq`` and
``resolution``, they typically share one timer. Calling
``ledcChangeFrequency()`` on either pin then changes **all** channels on that
timer.

On ESP32 (2 groups) the channel ranges are:

* channels ``0``–``7`` (group 0)
* channels ``8``–``15`` (group 1)

Other SoCs have a single group (all channels share one timer pool of 4).

To keep frequencies independent when you will change them at runtime:

1. Attach the pins with **different** frequency and/or resolution from the start
   (you can change them later with ``ledcChangeFrequency()``), or
2. On ESP32, put them in **different channel groups** (for example channel ``0``
   and channel ``8``).

Attaching both pins at the same frequency/resolution and only diverging later
with ``ledcChangeFrequency()`` will not give independent outputs if they already
share a timer.

Frequency and resolution limits
*******************************

Frequency and duty resolution are coupled. The PWM frequency is approximately:

``freq ≈ ledc_clock / (clock_divider × 2^resolution)``

The clock divider has a limited range, so for a given LEDC clock source and
resolution there is both a **minimum** and a **maximum** achievable frequency.
Asking for a frequency that is too low for the chosen resolution fails the same
way as asking for one that is too high.

When LEDC uses the XTAL clock (Arduino default on SoCs that support it), the
crystal is typically **40 MHz** (check ``getXtalFrequencyMhz()`` for the value
on your board). At 40 MHz with 8-bit resolution, the minimum is about **153 Hz**.
Original ESP32 does not support XTAL as an LEDC source and defaults to
``LEDC_AUTO_CLK`` instead.

To get lower frequencies, **increase** the resolution (for example 9–14 bits),
or change the LEDC clock source with ``ledcSetClockSource()`` before attaching
channels. Reducing resolution makes the minimum frequency higher, not lower.

Exact limits depend on the SoC, clock source, and board configuration. Run the
:ref:`LEDC frequency range example <ledc-frequency-example>` to print the
achievable min/max for the current setup.


Arduino-ESP32 LEDC API
----------------------

ledcSetCLockSource
******************

This function is used to set the LEDC peripheral clock source. Must be called before any LEDC channel is used.
The default clock source is XTAL clock (``LEDC_USE_XTAL_CLK``) if supported by the SoC, otherwise it is AUTO clock (``LEDC_AUTO_CLK``).

.. code-block:: arduino

    bool ledcSetClockSource(ledc_clk_cfg_t source);

* ``source`` select the clock source for LEDC peripheral.

  * ``LEDC_APB_CLK`` - APB clock.
  * ``LEDC_REF_CLK`` - REF clock.

This function will return ``true`` if setting the clock source is successful, otherwise it will return ``false``.

ledcGetClockSource
******************

This function is used to get the LEDC peripheral clock source.

.. code-block:: arduino

    ledc_clk_cfg_t ledcGetClockSource(void);

This function will return the clock source for the LEDC peripheral.

ledcAttach
**********

This function is used to setup LEDC pin with given frequency and resolution. LEDC channel will be selected automatically.

Timer reuse for matching frequency/resolution applies here as well
(see `Channels, timers and frequency sharing`_).

.. code-block:: arduino

    bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution);

* ``pin`` select LEDC pin.
* ``freq`` select frequency of pwm.
* ``resolution`` select resolution for LEDC channel.

  * range is 1-14 bits (1-20 bits for ESP32).

This function will return ``true`` if configuration is successful.
If ``false`` is returned, error occurs and LEDC channel was not configured.

ledcAttachChannel
*****************

This function is used to setup LEDC pin with given frequency, resolution and channel.
Attaching multiple pins to the same channel will make them share the same duty cycle. Given frequency, resolution will be ignored if channel is already configured.

If another channel in the same group already uses the same frequency and resolution,
that timer is reused (see `Channels, timers and frequency sharing`_). Different
channel numbers alone do not guarantee independent frequencies.

.. code-block:: arduino

    bool ledcAttachChannel(uint8_t pin, uint32_t freq, uint8_t resolution, int8_t channel);

* ``pin`` select LEDC pin.
* ``freq`` select frequency of pwm.
* ``resolution`` select resolution for LEDC channel.

  * range is 1-14 bits (1-20 bits for ESP32).

* ``channel`` select LEDC channel.

This function will return ``true`` if configuration is successful.
If ``false`` is returned, error occurs and LEDC channel was not configured.

ledcWrite
*********

This function is used to set duty for the LEDC pin.

.. code-block:: arduino

    bool ledcWrite(uint8_t pin, uint32_t duty);

* ``pin`` select LEDC pin.
* ``duty`` select duty to be set for selected LEDC pin.

This function will return ``true`` if setting duty is successful.
If ``false`` is returned, error occurs and duty was not set.

ledcWriteChannel
****************

This function is used to set duty for the LEDC channel.

.. code-block:: arduino

    bool ledcWriteChannel(uint8_t channel, uint32_t duty);

* ``channel`` select LEDC channel.
* ``duty`` select duty to be set for selected LEDC channel.

This function will return ``true`` if setting duty is successful.
If ``false`` is returned, error occurs and duty was not set.

ledcRead
********

This function is used to get configured duty for the LEDC pin.

.. code-block:: arduino

    uint32_t ledcRead(uint8_t pin);

* ``pin`` select LEDC pin to read the configured LEDC duty.

This function will return ``duty`` set for selected LEDC pin.

ledcReadFreq
************

This function is used to get configured frequency for the LEDC channel.

.. code-block:: arduino

    uint32_t ledcReadFreq(uint8_t pin);

* ``pin`` select LEDC pin to read the configured frequency.

This function will return ``frequency`` configured for selected LEDC pin.

ledcWriteTone
*************

This function is used to setup the LEDC pin to 50 % PWM tone on selected frequency.

.. code-block:: arduino

    uint32_t ledcWriteTone(uint8_t pin, uint32_t freq);

* ``pin`` select LEDC pin.
* ``freq`` select frequency of pwm signal. If frequency is ``0``, duty will be set to 0.

This function will return ``frequency`` set for LEDC pin.
If ``0`` is returned, error occurs and LEDC pin was not configured.

ledcWriteNote
*************

This function is used to setup the LEDC pin to specific note.

.. code-block:: arduino

    uint32_t ledcWriteNote(uint8_t pin, note_t note, uint8_t octave);

* ``pin`` select LEDC pin.
* ``note`` select note to be set.

======= ======= ======= ======= ======= ======
NOTE_C  NOTE_Cs NOTE_D  NOTE_Eb NOTE_E  NOTE_F
NOTE_Fs NOTE_G  NOTE_Gs NOTE_A  NOTE_Bb NOTE_B
======= ======= ======= ======= ======= ======

* ``octave`` select octave for note.

This function will return ``frequency`` configured for the LEDC pin according to note and octave inputs.
If ``0`` is returned, error occurs and the LEDC channel was not configured.

ledcDetach
**********

This function is used to detach the pin from LEDC.

.. code-block:: arduino

    bool ledcDetach(uint8_t pin);

* ``pin`` select LEDC pin.

This function returns ``true`` if detaching was successful.
If ``false`` is returned, an error occurred and the pin was not detached.

ledcChangeFrequency
*******************

This function is used to set frequency for the LEDC pin.

This updates the **timer** used by the pin. Every channel that shares that timer
gets the new frequency and resolution. See `Channels, timers and frequency sharing`_.

.. code-block:: arduino

    uint32_t ledcChangeFrequency(uint8_t pin, uint32_t freq, uint8_t resolution);

* ``pin`` select LEDC pin.
* ``freq`` select frequency of pwm.
* ``resolution`` select resolution for LEDC channel.

  * range is 1-14 bits (1-20 bits for ESP32).

This function will return ``frequency`` configured for the LEDC channel.
If ``0`` is returned, error occurs and the LEDC channel frequency was not set.

ledcOutputInvert
****************

This function is used to set inverting output for the LEDC pin.

.. code-block:: arduino

    bool ledcOutputInvert(uint8_t pin, bool out_invert);

* ``pin`` select LEDC pin.
* ``out_invert`` select, if output should be inverted (true = inverting output).

This function returns ``true`` if setting inverting output was successful.
If ``false`` is returned, an error occurred and the inverting output was not set.

ledcFade
********

This function is used to setup and start fade for the LEDC pin.

.. code-block:: arduino

    bool ledcFade(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms);

* ``pin`` select LEDC pin.
* ``start_duty`` select starting duty of fade.
* ``target_duty`` select target duty of fade.
* ``max_fade_time_ms`` select maximum time for fade.

This function will return ``true`` if configuration is successful.
If ``false`` is returned, error occurs and LEDC fade was not configured / started.

ledcFadeWithInterrupt
*********************

This function is used to setup and start fade for the LEDC pin with interrupt.

.. code-block:: arduino

    bool ledcFadeWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void));

* ``pin`` select LEDC pin.
* ``start_duty`` select starting duty of fade.
* ``target_duty`` select target duty of fade.
* ``max_fade_time_ms`` select maximum time for fade.
* ``userFunc`` function to be called when interrupt is triggered.

This function will return ``true`` if configuration is successful and fade start.
If ``false`` is returned, error occurs and LEDC fade was not configured / started.

ledcFadeWithInterruptArg
************************

This function is used to setup and start fade for the LEDC pin with interrupt using arguments.

.. code-block:: arduino

    bool ledcFadeWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void*), void * arg);

* ``pin`` select LEDC pin.
* ``start_duty`` select starting duty of fade.
* ``target_duty`` select target duty of fade.
* ``max_fade_time_ms`` select maximum time for fade.
* ``userFunc`` function to be called when interrupt is triggered.
* ``arg`` pointer to the interrupt arguments.

This function will return ``true`` if configuration is successful and fade start.
If ``false`` is returned, error occurs and LEDC fade was not configured / started.

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

This function is used to set resolution for selected analogWrite pin.

.. code-block:: arduino

    void analogWriteResolution(uint8_t pin, uint8_t resolution);

* ``pin`` select the GPIO pin.
* ``resolution`` select resolution for analog channel.

analogWriteFrequency
********************

This function is used to set frequency for selected analogWrite pin.

.. code-block:: arduino

    void analogWriteFrequency(uint8_t pin, uint32_t freq);

* ``pin`` select the GPIO pin.
* ``freq`` select frequency of pwm.

Example Applications
********************

.. _ledc-frequency-example:

LEDC frequency range example:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/ledcFrequency/ledcFrequency.ino
    :language: arduino

LEDC fade example:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/LEDCFade/LEDCFade.ino
    :language: arduino

LEDC software fade example:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/LEDCSoftwareFade/LEDCSoftwareFade.ino
    :language: arduino

LEDC Write RGB example:

.. literalinclude:: ../../../libraries/ESP32/examples/AnalogOut/ledcWrite_RGB/ledcWrite_RGB.ino
    :language: arduino
