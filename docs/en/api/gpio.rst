####
GPIO
####

About
-----

One of the most used and versatile peripheral in a microcontroller is the GPIO. The GPIO is commonly used to write and read the pin state.

GPIO stands to General Purpose Input Output, and is responsible to control or read the state of a specific pin in the digital world. For example, this peripheral is widely used to create the LED blinking or to read a simple button.

.. note:: There are some GPIOs with special restrictions, and not all GPIOs are accessible through the developemnt board. For more information about it, see the corresponding board pin layout information.

GPIOs Modes
***********

There are two different modes in the GPIO configuration:

- **Input Mode**

In this mode, the GPIO will receive the digital state from a specific device. This device could be a button or a switch.

- **Output Mode**

For the output mode, the GPIO will change the GPIO digital state to a specific device. You can drive an LED for example.

GPIO API
--------

Here is the common functions used for the GPIO peripheral.

pinMode
*******

The ``pinMode`` function is used to define the GPIO operation mode for a specific pin.

.. code-block:: arduino

    void pinMode(uint8_t pin, uint8_t mode);

* ``pin``   defines the GPIO pin number.
* ``mode``  sets operation mode.

The following modes are supported for the basic `input` and `output`:

* **INPUT** sets the GPIO as input without pullup or pulldown (high impedance).
* **OUTPUT** sets the GPIO as output/read mode.
* **INPUT_PULLDOWN** sets the GPIO as input with the internal pulldown.
* **INPUT_PULLUP** sets the GPIO as input with the internal pullup.

Internal Pullup and Pulldown
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ESP32 SoC families supports the internal pullup and pulldown through a 45kR resistor, that can be enabled when configuring the GPIO mode as ``INPUT`` mode.
If the pullup or pulldown mode is not defined, the pin will stay in the high impedance mode.

digitalWrite
*************

The function ``digitalWrite`` sets the state of the selected GPIO to ``HIGH`` or ``LOW``. This function is only used if the ``pinMode`` was configured as ``OUTPUT``.

.. code-block:: arduino

    void digitalWrite(uint8_t pin, uint8_t val);

* ``pin``  defines the GPIO pin number.
* ``val``  set the output digital state to ``HIGH`` or ``LOW``.

digitalRead
***********

To read the state of a given pin configured as ``INPUT``, the function ``digitalRead`` is used.

.. code-block:: arduino

    int digitalRead(uint8_t pin);

* ``pin`` select GPIO

This function will return the logical state of the selected pin as ``HIGH`` or ``LOW``.

Interrupts
----------

The GPIO peripheral on the ESP32 supports interruptions.

attachInterrupt
***************

The function ``attachInterrupt`` is used to attach the interrupt to the defined pin.

.. code-block:: arduino

  attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode);

* ``pin``  defines the GPIO pin number.
* ``handler``  set the handler function.
* ``mode``  set the interrupt mode.

Here are the supported interrupt modes:

* **DISABLED**
* **RISING**
* **FALLING**
* **CHANGE**
* **ONLOW**
* **ONHIGH**
* **ONLOW_WE**
* **ONHIGH_WE**

attachInterruptArg
******************

The function ``attachInterruptArg`` is used to attach the interrupt to the defined pin using arguments.

.. code-block:: arduino

  attachInterruptArg(uint8_t pin, voidFuncPtrArg handler, void * arg, int mode);

* ``pin``  defines the GPIO pin number.
* ``handler``  set the handler function.
* ``arg`` pointer to the interrupt arguments.
* ``mode``  set the interrupt mode.

detachInterrupt
***************

To detach the interruption from a specific pin, use the ``detachInterrupt`` function giving the GPIO to be detached.

.. code-block:: arduino

  detachInterrupt(uint8_t pin);

* ``pin``  defines the GPIO pin number.

.. _gpio_example_code:

Example Code
------------

GPIO Input and Output Modes
***************************

.. code-block:: arduino

  #define LED    12
  #define BUTTON 2

  uint8_t stateLED = 0;

    void setup() {
        pinMode(LED, OUTPUT);
        pinMode(BUTTON,INPUT_PULLUP);
    }

    void loop() {

       if(!digitalRead(BUTTON)){
         stateLED = stateLED^1;
        digitalWrite(LED,stateLED);
      }
    }

GPIO Interrupt
**************

.. literalinclude:: ../../../libraries/ESP32/examples/GPIO/GPIOInterrupt/GPIOInterrupt.ino
    :language: arduino

.. _datasheet: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
