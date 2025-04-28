###################
Contributions Guide
###################

We welcome contributions to the Arduino ESP32 project!

How to Contribute
-----------------

Contributions to the Arduino ESP32 (fixing bugs, adding features, adding documentation) are welcome.
We accept contributions via `Github Pull Requests <https://help.github.com/en/github/collaborating-with-issues-and-pull-requests/about-pull-requests>`_.

Before Contributing
-------------------

Before sending us a Pull Request, please consider this:

* All contributions must be written in English to ensure effective communication and support.
  Pull Requests written in other languages will be closed, with a request to rewrite them in English.

* Is the contribution entirely your own work, or is it already licensed under an LGPL 2.1 compatible Open Source License?
  If not, cannot accept it.

* Is the code adequately commented and can people understand how it is structured?

* Is there documentation or examples that go with code contributions?

* Are comments and documentation written in clear English, with no spelling or grammar errors?

* Example contributions are also welcome.

  * If you are contributing by adding a new example, please use the `Arduino style guide`_ and the example guideline below.

* If the contribution contains multiple commits, are they grouped together into logical changes (one major change per pull request)?
  Are any commits with names like "fixed typo" `squashed into previous commits <https://eli.thegreenplace.net/2014/02/19/squashing-github-pull-requests-into-a-single-commit/>`_?

If you're unsure about any of these points, please open the Pull Request anyhow and then ask us for feedback.

Pull Request Process
--------------------

After you open the Pull Request, there will probably be some discussion in the comments field of the request itself.

Once the Pull Request is ready to merge, it will first be merged into our internal git system for "in-house" automated testing.

If this process passes, it will be merged into the public GitHub repository.

Example Contribution Guideline
------------------------------

Checklist
*********

* Check if your example proposal has no similarities to the project (**already existing examples**)
* Use the `Arduino style guide`_
* Add the header to all source files
* Add the ``README.md`` file
* Add inline comments if needed
* Test the example

Header
******

All the source files must include the header with the example name and license, if applicable. You can change this header as you wish,
but it will be reviewed by the community and may not be accepted.

Ideally, you can add some description about the example, links to the documentation, or the author's name.
Just have in mind to keep it simple and short.

**Header Example**

.. code-block:: arduino

    /* Wi-Fi FTM Initiator Arduino Example

      This example code is in the Public Domain (or CC0 licensed, at your option.)

      Unless required by applicable law or agreed to in writing, this
      software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
      CONDITIONS OF ANY KIND, either express or implied.
    */


README file
***********

The ``README.md`` file should contain the example details.

Please see the recommended ``README.md`` file in the `example template folder <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Template/ExampleTemplate>`_.

Inline Comments
***************

Inline comments are important if the example contains complex algorithms or specific configurations that the user needs to change.

Brief and clear inline comments are really helpful for the example understanding and it's fast usage.

See the `FTM example <https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/FTM/FTM_Initiator/FTM_Initiator.ino>`_
as a reference:

.. code-block:: arduino

    // Number of FTM frames requested in terms of 4 or 8 bursts (allowed values - 0 (No pref), 16, 24, 32, 64)

Also:

.. code-block:: arduino

    const char * WIFI_FTM_SSID = "WiFi_FTM_Responder"; // SSID of AP that has FTM Enabled
    const char * WIFI_FTM_PASS = "ftm_responder"; // STA Password

Testing
*******

Be sure you have tested the example in all the supported targets. If the example some specific hardware requirements,
edit/add the ``ci.json`` in the same folder as the sketch to specify the regular expression for the
required configurations from ``sdkconfig``.
This will ensure that the CI system will run the test only on the targets that have the required configurations.

You can check the available configurations in the ``sdkconfig`` file in the ``tools/esp32-arduino-libs/<target>`` folder.

Here is an example of the ``ci.json`` file where the example requires Wi-Fi to work properly:

.. code-block:: json

    {
      "requires": [
        "CONFIG_SOC_WIFI_SUPPORTED=y"
      ]
    }

.. note::

    The list of configurations will be checked against the ``sdkconfig`` file in the target folder. If the configuration is not present in the ``sdkconfig``,
    the test will be skipped for that target. That means that the test will only run on the targets that have **ALL** the required configurations.

    Also, by default, the "match start of line" character (``^``) will be added to the beginning of each configuration.
    That means that the configuration must be at the beginning of the line in the ``sdkconfig`` file.

Sometimes, the example might not be supported by some target, even if the target has the required configurations
(like resources limitations or requiring a specific SoC). To avoid compilation errors, you can add the target to the ``ci.json``
file so the CI system will force to skip the test on that target.

Here is an example of the ``ci.json`` file where the example is requires Wi-Fi to work properly but is also not supported by the ESP32-S2 target:

.. code-block:: json

    {
      "requires": [
        "CONFIG_SOC_WIFI_SUPPORTED=y"
      ],
      "targets": {
        "esp32s2": false
      }
    }

You also need to add this information in the ``README.md`` file, on the **Supported Targets**, and in the example code as an inline comment.
For example, in the sketch:

.. code-block:: arduino

    /*
      THIS FEATURE REQUIRES WI-FI SUPPORT AND IS NOT AVAILABLE FOR ESP32-S2 AS IT DOES NOT HAVE ENOUGH RAM.
    */

And in the ``README.md`` file:

.. code-block:: markdown

    Currently, this example requires Wi-Fi and supports the following targets.

    | Supported Targets | ESP32 | ESP32-S3 | ESP32-C3 | ESP32-C6 |
    | ----------------- | ----- | -------- | -------- | -------- |

By default, the CI system will use the FQBNs specified in the ``.github/scripts/sketch_utils.sh`` file to compile the sketches.
Currently, the default FQBNs are:

* ``espressif:esp32:esp32:PSRAM=enabled``
* ``espressif:esp32:esp32s2:PSRAM=enabled``
* ``espressif:esp32:esp32s3:PSRAM=opi,USBMode=default``
* ``espressif:esp32:esp32c3``
* ``espressif:esp32:esp32c6``
* ``espressif:esp32:esp32h2``
* ``espressif:esp32:esp32p4:USBMode=default``

There are two ways to alter the FQBNs used to compile the sketches: by using the ``fqbn`` or ``fqbn_append`` fields in the ``ci.json`` file.

If you just want to append a string to the default FQBNs, you can use the ``fqbn_append`` field. For example, to add the ``DebugLevel=debug`` to the FQBNs, you would use:

.. code-block:: json

    {
      "fqbn_append": "DebugLevel=debug"
    }

If you want to override the default FQBNs, you can use the ``fqbn`` field. It is a dictionary where the key is the target name and the value is a list of FQBNs.
The FQBNs in the list will be used in sequence to compile the sketch. For example, to compile a sketch for ESP32-S2 with and without PSRAM enabled, you would use:

.. code-block:: json

    {
      "fqbn": {
        "esp32s2": [
          "espressif:esp32:esp32s2:PSRAM=enabled,FlashMode=dio",
          "espressif:esp32:esp32s2:PSRAM=disabled,FlashMode=dio"
        ]
      }
    }

.. note::

    The FQBNs specified in the ``fqbn`` field will also override the options specified in the ``fqbn_append`` field.
    That means that if the ``fqbn`` field is specified, the ``fqbn_append`` field will be ignored and will have no effect.

Example Template
****************

The example template can be found `here <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Template/ExampleTemplate>`_
and can be used as a reference.

Documentation
-------------

If you are contributing to the documentation, please follow the instructions described in the
`documentation guidelines <guides/docs_contributing>`_ to properly format and test your changes.

Testing and CI
--------------

After you have made your changes, you should test them. You can do this in different ways depending on the type of change you have made.

Examples
********

The easiest way to test an example is to load it into the Arduino IDE and run it on your board. If you have multiple boards,
you should test it on all of them to ensure that the example works on all supported targets.

You can refer to the `Example Contribution Guideline`_ section for more information on how to write and test examples.

Library Changes
***************

If you have made changes to a library, you should test it on all supported targets. You can do this by loading the library examples (or creating new ones)
into the Arduino IDE and running them on your board. If you have multiple boards, you should test it on all of them to ensure that the library
works as expected on all targets.

You can also add a new test suite to automatically check the library. You can refer to the `Adding a New Test Suite`_ section for more information.

Core Changes
************

If you have made changes to the core, it is important to ensure that the changes do not break the existing functionality. You can do this by running the
tests on all supported targets. You can refer to the `Runtime Tests`_ section for more information.

CI
**

In our repository, we have a Continuous Integration (CI) system that runs tests and fixes on every Pull Request. This system will run the tests
on all supported targets and check if everything is working as expected.

We have many types of tests and checks, including:

* Compilation tests;
* Runtime tests;
* Documentation checks;
* Code style checks;
* And more.

Let's go deeper into each type of test in the CI system:

Compilation Tests
^^^^^^^^^^^^^^^^^

The compilation tests are the first type of tests in the CI system. They check if the code compiles on all supported targets.
If the code does not compile, the CI system will fail the test and the Pull Request will not be merged.
This is important to ensure that the code is compatible with all supported targets and no broken code is merged.

It will go through all the sketches in the repository and check if they compile on all supported targets. This process is automated and controlled
by GitHub Actions. The CI system will run the ``arduino-cli`` tool to compile the sketches on all supported targets.

Testing it locally using the CI scripts would be too time consuming, so we recommend running the tests locally using the Arduino IDE with
a sketch that uses the changes you made (you can also add the sketch as an example if your change is not covered by the existing ones).
Make sure to set ``Compiler Warnings`` to ``All`` in the Arduino IDE to catch any potential issues.

Runtime Tests
^^^^^^^^^^^^^

Another type of test is the runtime tests. They check if the code runs and behaves as expected on all supported targets. If the
code does not run as expected, the CI system will fail the test and the Pull Request will not be merged. This is important to ensure that the code
works as expected on all supported targets and no unintended crashes or bugs are introduced.

These tests are specialized sketches that run on the target board and check if the code behaves as expected. This process is automated and
controlled by the ``pytest_embedded`` tool. You can read more about this tool in its
`documentation <https://docs.espressif.com/projects/pytest-embedded/en/latest/>`_.

The tests are divided into two categories inside the ``tests`` folder:

* ``validation``: These tests are used to validate the behavior of the Arduino core and libraries. They are used to check if the core and libraries
  are working as expected;
* ``performance``: These tests are used to check the performance of the Arduino core and libraries. They are used to check if the changes made
  to the core and libraries have any big impact on the performance. These tests usually run for a longer time than the validation tests and include
  common benchmark tools like `CoreMark <https://github.com/eembc/coremark>`_.

To run the runtime tests locally, first install the required dependencies by running:

.. code-block:: bash

    pip install -U -r tests/requirements.txt

Before running the test, we need to build it by running:

.. code-block:: bash

    ./.github/scripts/tests_build.sh -s <test_name> -t <target>

The ``<test_name>`` is the name of the test you want to run (you can check the available tests in the ``tests/validation`` and
``tests/performance`` folders), and the ``<target>`` is the target board you want to run the test on. For example, to run the ``uart`` test on the
ESP32-C3 target, you would run:

.. code-block:: bash

    ./.github/scripts/tests_build.sh -s uart -t esp32c3

You should see the output of the build process and the test binary should be generated in the ``~/.arduino/tests/<target_chip>/<test_name>/build.tmp`` folder.

Now that the test is built, you can run it in the target board. Connect the target board to your computer and run:

.. code-block:: bash

    ./.github/scripts/tests_run.sh -s <test_name> -t <target>

For example, to run the ``uart`` test on the ESP32-C3 target, you would run:

.. code-block:: bash

    ./.github/scripts/tests_run.sh -s uart -t esp32c3

The test will run on the target board and you should see the output of the test in the terminal:

.. code-block:: bash

    lucassvaz@Lucas--MacBook-Pro esp32 % ./.github/scripts/tests_run.sh -s uart -t esp32c3
    Sketch uart test type: validation
    Running test: uart -- Config: Default
    pytest tests --build-dir /Users/lucassvaz/.arduino/tests/esp32c3/uart/build.tmp -k test_uart --junit-xml=/Users/lucassvaz/Espressif/Arduino/hardware/espressif/esp32/tests/validation/uart/esp32c3/uart.xml --embedded-services esp,arduino
    =============================================================================================== test session starts ================================================================================================
    platform darwin -- Python 3.12.3, pytest-8.2.2, pluggy-1.5.0
    rootdir: /Users/lucassvaz/Espressif/Arduino/hardware/espressif/esp32/tests
    configfile: pytest.ini
    plugins: cov-5.0.0, embedded-1.11.5, anyio-4.4.0
    collected 15 items / 14 deselected / 1 selected

    tests/validation/uart/test_uart.py::test_uart
    -------------------------------------------------------------------------------------------------- live log setup --------------------------------------------------------------------------------------------------
    2024-08-22 11:49:30 INFO Target: esp32c3, Port: /dev/cu.usbserial-2120
    PASSED                                                                                                                                                                                                       [100%]
    ------------------------------------------------------------------------------------------------ live log teardown -------------------------------------------------------------------------------------------------
    2024-08-22 11:49:52 INFO Created unity output junit report: /private/var/folders/vp/g9wctsvn7b91k3pv_7cwpt_h0000gn/T/pytest-embedded/2024-08-22_14-49-30-392993/test_uart/dut.xml


    ---------------------------------------------- generated xml file: /Users/lucassvaz/Espressif/Arduino/hardware/espressif/esp32/tests/validation/uart/esp32c3/uart.xml ----------------------------------------------
    ======================================================================================== 1 passed, 14 deselected in 22.18s =========================================================================================

After the test is finished, you can check the output in the terminal and the generated XML file in the test folder.
Additionally, for performance tests, you can check the generated JSON file in the same folder.

You can also run the tests in `Wokwi <https://docs.wokwi.com/>`_ or `Espressif's QEMU <https://github.com/espressif/esp-toolchain-docs/tree/main/qemu>`_
by using the ``-W <timeout_in_ms>`` and ``-Q`` flags respectively. You will need to have the Wokwi and/or QEMU installed in your system
and set the ``WOKWI_CLI_TOKEN`` and/or ``QEMU_PATH`` environment variables. The ``WOKWI_CLI_TOKEN`` is the CI token that can be obtained from the
`Wokwi website <https://wokwi.com/dashboard/ci>`_ and the ``QEMU_PATH`` is the path to the QEMU binary.

For example, to run the ``uart`` test using Wokwi, you would run:

.. code-block:: bash

    WOKWI_CLI_TOKEN=<your_wokwi_token> ./.github/scripts/tests_run.sh -s uart -t esp32c3 -W <timeout_in_ms>

And to run the ``uart`` test using QEMU, you would run:

.. code-block:: bash

    QEMU_PATH=<path_to_qemu_binary> ./.github/scripts/tests_run.sh -s uart -t esp32c3 -Q

.. note::

    Not all tests are supported by Wokwi and QEMU. QEMU is only supported for ESP32 and ESP32-C3 targets.
    Wokwi support depends on the `currently implemented peripherals <https://docs.wokwi.com/guides/esp32#simulation-features>`_.

Adding a New Test Suite
#######################

If you want to add a new test suite, you can create a new folder inside ``tests/validation`` or ``tests/performance`` with the name of the test suite.
You can use the ``hello_world`` test suite as a starting point and the other test suites as a reference.

A test suite contains the following files:

* ``test_<test_name>.py``: The test file that contains the test cases. Required.
* ``<test_name>.ino``: The sketch that will be tested. Required.
* ``ci.json``: The file that specifies how the test suite will be run in the CI system. Optional.
* ``diagram.<target>.json``: The diagram file that specifies the connections between the components in Wokwi. Optional.
* ``scenario.yaml``: The scenario file that specifies how Wokwi will interact with the components. Optional.
* Any other files that are needed for the test suite.

You can read more about the test python API in the `pytest-embedded documentation <https://docs.espressif.com/projects/pytest-embedded/en/latest/usages/expecting.html>`_.
For more information about the Unity testing framework, you can check the `Unity documentation <https://github.com/ThrowTheSwitch/Unity>`_.

After creating the test suite, make sure to test it locally and run it in the CI system to ensure that it works as expected.

CI JSON File
############

The ``ci.json`` file is used to specify how the test suite and sketches will handled by the CI system. It can contain the following fields:

* ``requires``: A list of configurations in ``sdkconfig`` that are required to run the test suite. The test suite will only run on the targets
  that have **ALL** the required configurations. By default, no configurations are required.
* ``requires_any``: A list of configurations in ``sdkconfig`` that are required to run the test suite. The test suite will only run on the targets
  that have **ANY** of the required configurations. By default, no configurations are required.
* ``targets``: A dictionary that specifies the targets for which the tests will be run. The key is the target name and the value is a boolean
  that specifies if the test should be run for that target. By default, all targets are enabled as long as they have the required configurations
  specified in the ``requires`` field. This field is also valid for examples.
* ``platforms``: A dictionary that specifies the supported platforms. The key is the platform name and the value is a boolean that specifies if
  the platform is supported. By default, all platforms are assumed to be supported.
* ``extra_tags``: A list of extra tags that the runner will require when running the test suite in hardware. By default, no extra tags are required.
* ``fqbn_append``: A string to be appended to the default FQBNs. By default, no string is appended. This has no effect if ``fqbn`` is specified.
* ``fqbn``: A dictionary that specifies the FQBNs that will be used to compile the sketch. The key is the target name and the value is a list
  of FQBNs. The `default FQBNs <https://github.com/espressif/arduino-esp32/blob/a31a5fca1739993173caba995f7785b8eed6b30e/.github/scripts/sketch_utils.sh#L86-L91>`_
  are used if this field is not specified. This overrides the default FQBNs and the ``fqbn_append`` field.

The ``wifi`` test suite is a good example of how to use the ``ci.json`` file:

.. literalinclude:: ../../tests/validation/wifi/ci.json
    :language: json

Documentation Checks
^^^^^^^^^^^^^^^^^^^^

The CI also checks the documentation for any compilation errors. This is important to ensure that the documentation layout is not broken.
To build the documentation locally, please refer to the `documentation guidelines <guides/docs_contributing>`_.

Code Style Checks
^^^^^^^^^^^^^^^^^

For checking the code style and other code quality checks, we use pre-commit hooks.
These hooks will be automatically run by the CI when a Pull Request is marked as ``Status: Pending Merge``.
You can check which hooks are being run in the ``.pre-commit-config.yaml`` file.

Currently, we have hooks for the following tasks:

* Formatters for C, C++, Python, Bash, JSON, Markdown and ReStructuredText files;
* Linters for Python, Bash and prose (spoken language);
* Checking for spelling errors in the code and documentation;
* Removing trailing whitespaces and tabs in the code;
* Checking for the presence of private keys and other sensitive information in the code;
* Fixing the line endings and end of files (EOF) in the code;
* And more.

You can read more about the pre-commit hooks in the `pre-commit documentation <https://pre-commit.com/>`_.

If you want to run the pre-commit hooks locally, you first need to install the required dependencies by running:

.. code-block:: bash

    pip install -U -r tools/pre-commit/requirements.txt

Then, you can run the pre-commit hooks staging your changes and running:

.. code-block:: bash

    pre-commit run

To run a specific hook, you can use the hook name as an argument. For example, to run the ``codespell`` hook, you would run:

.. code-block:: bash

    pre-commit run codespell

If you want to run the pre-commit hooks automatically against the changed files on every ``git commit``,
you can install the pre-commit hooks by running:

.. code-block:: bash

    pre-commit install

Legal Part
----------

Before a contribution can be accepted, you will need to sign our contributor agreement. You will be prompted for this automatically as part of
the Pull Request process.

.. _Arduino style guide: https://docs.arduino.cc/learn/contributions/arduino-writing-style-guide
