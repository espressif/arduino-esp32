##########################
Blink Interactive Tutorial
##########################

Introduction
------------

This is the interactive blink tutorial using `Wokwi`_. For this tutorial, you don't need the ESP32 board or the Arduino toolchain.

.. note:: If you don't want to use this tutorial with the simulation, you can copy and paste the :ref:`blink_example_code` from `Wokwi`_ editor and use it on the `Arduino IDE`.

About this Tutorial
-------------------

This tutorial is the most basic for any get started. In this tutorial, we will show how to set a GPIO pin as an output to drive a LED to blink each 1 second.

Step by step
------------

In order to make this simple blink tutorial, you'll need to do the following steps.

1. **Define the GPIO for the LED.**

.. code-block::

    #define LED 2

This ``#define LED 2`` will be used to set the GPIO2 as the ``LED`` output pin.

2. **Setup.**

Inside the ``setup()`` function, we need to add all things we want to run once during the startup.
Here we'll add the ``pinMode`` function to set the pin as output.

.. code-block::

    void setup() {
        pinMode(LED, OUTPUT);
    }

The first argument is the GPIO number, already defined and the second is the mode, here defined as an output.

3. **Main Loop.**

After the ``setup``, the code runs the ``loop`` function infinitely. Here we will handle the GPIO in order to get the LED blinking.

.. code-block::

    void loop() {
        digitalWrite(LED, HIGH);
        delay(100);
        digitalWrite(LED, LOW);
        delay(100);
    }

The first function is the ``digitalWrite()`` with two arguments:

* GPIO: Set the GPIO pin. Here defined by our ``LED`` connected to the GPIO2.
* State: Set the GPIO state as HIGH (ON) or LOW (OFF).

This first ``digitalWrite`` we will set the LED ON.

After the ``digitalWrite``, we will set a ``delay`` function in order to wait for some time, defined in milliseconds.

Now we can set the GPIO to ``LOW`` to turn the LED off and ``delay`` for more few milliseconds to get the LED blinking.

4. **Run the code.**

To run this code, you'll need a development board and the Arduino toolchain installed on your computer. If you don't have both, you can use the simulator to test and edit the code.

Simulation
----------

This simulator is provided by `Wokwi`_ and you can test the blink code and play with some modifications to learn more about this example.

.. raw:: html

    <iframe src="https://wokwi.com/arduino/projects/305566932847821378?embed=1" width="100%" height="400" border="0"></iframe>

Change the parameters, like the delay period, to test the code right on your browser. You can add more LEDs, change the GPIO, and more.

.. _blink_example_code:

Example Code
------------

Here is the full blink code.

.. code-block::

    #define LED 2

    void setup() {
        pinMode(LED, OUTPUT);
    }

    void loop() {
        digitalWrite(LED, HIGH);
        delay(100);
        digitalWrite(LED, LOW);
        delay(100);
    }

Resources
---------

* `ESP32 Datasheet`_ (Datasheet)
* `Wokwi`_ (Wokwi Website)

.. _ESP32 Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
.. _Wokwi: https://wokwi.com/
.. _Arduino IDE: https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html#installing-using-boards-manager
