###############
Library Builder
###############

About
-----

Espressif provides a `tool <https://github.com/espressif/esp32-arduino-lib-builder>`_ to simplify building your own compiled libraries for use in Arduino IDE (or your favorite IDE).

This tool can be used to change the project or a specific configuration according to your needs.

Installing
----------

To install the Library Builder into your environment, please, follow the instructions below.

- Clone the ESP32 Arduino lib builder:

.. code-block:: bash

    git clone https://github.com/espressif/esp32-arduino-lib-builder

- Go to the ``esp32-arduino-lib-builder`` folder:

.. code-block:: bash

    cd esp32-arduino-lib-builder

- Build:

.. code-block:: bash

    ./build.sh

If everything works, you may see the following message: ``Successfully created esp32 image.``

Dependencies
************

To build the library you will need to install some dependencies. Maybe you already have installed it, but it is a good idea to check before building.

- Install all dependencies (**Ubuntu**):

.. code-block:: bash

    sudo apt-get install git wget curl libssl-dev libncurses-dev flex bison gperf cmake ninja-build ccache jq

- Install Python and upgrade pip:

.. code-block:: bash

    sudo apt-get install python3
    sudo pip install --upgrade pip

- Install all required packages:

.. code-block:: bash

    pip install --user setuptools pyserial click cryptography future pyparsing pyelftools

Building
--------

If you have all the dependencies met, it is time to build the libraries.

To build using the default configuration:

.. code-block:: bash

    ./build.sh

Custom Build
************

There are some options to help you create custom libraries. You can use the following options:

Usage
^^^^^

.. code-block:: bash

    build.sh [-s] [-A arduino_branch] [-I idf_branch] [-i idf_commit] [-c path] [-t <target>] [-b <build|menuconfig|idf_libs|copy_bootloader|mem_variant>] [config ...]

Skip Install/Update
^^^^^^^^^^^^^^^^^^^

Skip installing/updating of ESP-IDF and all components

.. code-block:: bash

    ./build.sh -s

This option can be used if you already have the ESP-IDF and all components already in your environment.

Set Arduino-ESP32 Branch
^^^^^^^^^^^^^^^^^^^^^^^^

Set which branch of arduino-esp32 to be used for compilation

.. code-block:: bash

    ./build.sh -A <arduino_branch>

Set ESP-IDF Branch
^^^^^^^^^^^^^^^^^^

Set which branch of ESP-IDF is to be used for compilation

.. code-block:: bash

    ./build.sh -I <idf_branch>

Set the ESP-IDF Commit
^^^^^^^^^^^^^^^^^^^^^^

Set which commit of ESP-IDF to be used for compilation

.. code-block:: bash

    ./build.sh -i <idf_commit>

Deploy
^^^^^^

Deploy the build to github arduino-esp32

.. code-block:: bash

    ./build.sh -d

Set the Arduino-ESP32 Destination Folder
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set the arduino-esp32 folder to copy the result to. ex. '$HOME/Arduino/hardware/espressif/esp32'

.. code-block:: bash

    ./build.sh -c <path>

This function is used to copy the compiled libraries to the Arduino folder.

Set the Target
^^^^^^^^^^^^^^

Set the build target(chip). ex. 'esp32s3'

.. code-block:: bash

    ./build.sh -t <target>

This build command will build for the ESP32-S3 target. You can specify other targets.

* esp32
* esp32s2
* esp32c3
* esp32s3

Set Build Type
^^^^^^^^^^^^^^

Set the build type. ex. 'build' to build the project and prepare for uploading to a board.

.. note:: This command depends on the ``-t`` argument.

.. code-block:: bash

    ./build.sh -t esp32 -b <build|menuconfig|idf_libs|copy_bootloader|mem_variant>

Additional Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

Specify additional configs to be applied. ex. 'qio 80m' to compile for QIO Flash@80MHz. Requires -b

.. note:: This command requires the ``-b`` to work properly.


.. code-block:: bash

    ./build.sh -t esp32 -b idf_libs qio 80m
