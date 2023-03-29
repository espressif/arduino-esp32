##########################
External Libraries Testing
##########################

About
-----

External libraries testing is a compilation test for listed libraries on arduino-esp32 master branch. The test was made for users, so they can check which libraries are compiling without errors on which SoC.
External libraries test is running periodicaly (once a week) agains master branch and can also run on PR by adding a label ``lib_test``.
The test is running on all supported ESP32 chips.

.. note:: 
  As the test is just a compilation of example, that does not guarrantee that the library/sketch will run without any problems after flashing it on your device.

How to Add Library to Test
--------------------------

To add a library to the CI test you need to add your library to the `lib.json`_. file located in ``./github/workflows/``.

List of parameters:
*******************

Where the library will be installed from (use only 1 option):

* ``name`` - Name of the Library in Arduino Library Manager.
* ``source-url`` - URL to the library github repository (example: "https://github.com/Arduino-IRremote/Arduino-IRremote.git"). Use when your Library is not listed in Arduino Library Manager.

Required:

* ``exclude_targets`` - List of targets to be excluded from testing. Use only when the SoC dont support used peripheral.
* ``sketch_path`` - Path / paths to the sketch / sketches to be tested.
  
Optional:

* ``version`` - Version of the library
* ``destination-name`` - Folder name used for the installation of library (use only when needed)


Example of library addition from Arduino Library Manager:
*********************************************************

  .. code-block:: json

    {
        "name": "ArduinoBLE",
        "exclude_targets": [
            "esp32s2"
        ],
        "sketch_path": [
            "~/Arduino/libraries/ArduinoBLE/examples/Central/Scan/Scan.ino"
        ]
    }

Example of library addition from Github URL:
********************************************

  .. code-block:: json

    {
        "name": "IRremote",
        "source-url": "https://github.com/Arduino-IRremote/Arduino-IRremote.git",
        "version": "latest",
        "exclude_targets": [],
        "sketch_path": [
            "~/Arduino/libraries/IRremote/examples/SendDemo/SendDemo.ino"
        ]
    }

Sumbit a PR
***********

* Open a PR with the changes and someone from Espressif team will add a label ``lib_test`` to the PR and CI will run the test to check, if the addition is fine and library / example are compiling.

* After merging your PR, the next scheduled test will test your library and add the results to the `LIBRARIES_TEST.md`_.
  
Test Results
------------

Icons meaning
*************

* |success| - Compilation was successful.

* |warning| - Compilation was successful, but some warnings occurs.

* |fail| - Compilation failed.

* ``N/A`` - Not tested (target is in exclude_targets list).

Scheduled test result
*********************

You can check the results in `LIBRARIES_TEST.md`_.

The results file example:

.. image:: _static/external_library_test_schedule.png
  :width: 600

Pull Request test result
************************

If the test run on Pull Request, it will compile all libraries and sketches 2 times (before/after changes in PR) to see, if the PR is breaking/fixing libraries.
In the table the results are in order ``BEFORE -> AFTER``.

.. image:: _static/external_library_test_pr.png
  :width: 600

.. |success| image:: _static/green_checkmark.png
   :height: 2ex
   :class: no-scaled-link

.. |warning| image:: _static/warning.png
   :height: 2ex
   :class: no-scaled-link

.. |fail| image:: _static/cross.png
   :height: 2ex
   :class: no-scaled-link

.. _LIBRARIES_TEST.md: https://github.com/espressif/arduino-esp32/LIBRARIES_TEST.md
.. _lib.json: https://github.com/espressif/arduino-esp32/.github/workflow/lib.json