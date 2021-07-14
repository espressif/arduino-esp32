##############
Basic Tutorial
##############

Introduction
------------

This is the basic tutorial and should be used as template for other tutorials.

Requirements
------------

* Arduino IDE
* ESP32 Board
* Good USB Cable

Steps
-----

Here are the steps for this tutorial.

1. Open the Arduino IDE

.. figure:: ../_static/tutorials/basic/tutorial_basic_ide.png
    :align: center
    :width: 600
    :alt: Arduino IDE (click to enlarge)
    :figclass: align-center

2. Build and Flash the `blink` project.

Code
----

.. code-block:: arduino
    :caption: Blink.ino

    /*
    Blink

    Turns an LED on for one second, then off for one second, repeatedly.

    Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
    it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
    the correct LED pin independent of which board is used.
    If you want to know what pin the on-board LED is connected to on your Arduino
    model, check the Technical Specs of your board at:
    https://www.arduino.cc/en/Main/Products

    modified 8 May 2014
    by Scott Fitzgerald
    modified 2 Sep 2016
    by Arturo Guadalupi
    modified 8 Sep 2016
    by Colby Newman

    This example code is in the public domain.

    http://www.arduino.cc/en/Tutorial/Blink
    */

    // the setup function runs once when you press reset or power the board
    void setup() {
    // initialize digital pin LED_BUILTIN as an output.
    pinMode(LED_BUILTIN, OUTPUT);
    }

    // the loop function runs over and over again forever
    void loop() {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    delay(1000);                       // wait for a second
    }

Log Output
----------

If the log output from the serial monitor is relevant, please add here:

.. code-block::

    I (0) cpu_start: App cpu up.
    I (418) cpu_start: Pro cpu start user code
    I (418) cpu_start: cpu freq: 160000000

Resources
---------

* `ESP32 Datasheet`_ (Datasheet)

.. _ESP32 Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
