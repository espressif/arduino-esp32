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

* Is the contribution entirely your own work, or is it already licensed under an LGPL 2.1 compatible Open Source License? If not, cannot accept it.

* Is the code adequately commented and can people understand how it is structured?

* Is there documentation or examples that go with code contributions?

* Are comments and documentation written in clear English, with no spelling or grammar errors?

* Example contributions are also welcome.

  * If you are contributing by adding a new example, please use the `Arduino style guide`_ and the example guideline below.

* If the contribution contains multiple commits, are they grouped together into logical changes (one major change per pull request)? Are any commits with names like "fixed typo" `squashed into previous commits <https://eli.thegreenplace.net/2014/02/19/squashing-github-pull-requests-into-a-single-commit/>`_?

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
* Add the `README.md` file
* Add inline comments if needed
* Test the example

Header
******

All the source files must include the header with the example name and license, if applicable. You can change this header as you wish, but it will be reviewed by the community and may not be accepted.

Ideally, you can add some description about the example, links to the documentation, or the author's name. Just have in mind to keep it simple and short.

**Header Example**

.. code-block:: arduino

    /* Wi-Fi FTM Initiator Arduino Example

      This example code is in the Public Domain (or CC0 licensed, at your option.)

      Unless required by applicable law or agreed to in writing, this
      software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
      CONDITIONS OF ANY KIND, either express or implied.
    */


README file
***********

The **README.md** file should contain the example details.

Please see the recommended **README.md** file in the `example template folder <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Template/ExampleTemplate>`_.

Inline Comments
***************

Inline comments are important if the example contains complex algorithms or specific configurations that the user needs to change.

Brief and clear inline comments are really helpful for the example understanding and it's fast usage.

**Example**

See the `FTM example <https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/FTM/FTM_Initiator/FTM_Initiator.ino>`_ as a reference.

.. code-block:: arduino

    // Number of FTM frames requested in terms of 4 or 8 bursts (allowed values - 0 (No pref), 16, 24, 32, 64)

and

.. code-block:: arduino

    const char * WIFI_FTM_SSID = "WiFi_FTM_Responder"; // SSID of AP that has FTM Enabled
    const char * WIFI_FTM_PASS = "ftm_responder"; // STA Password

Testing
*******

Be sure you have tested the example in all the supported targets. If the example works only with specific targets, add this information in the **README.md** file on the **Supported Targets** and in the example code as an inline comment.

**Example**

.. code-block:: arduino

    /*
      THIS FEATURE IS SUPPORTED ONLY BY ESP32-S2 AND ESP32-C3
    */

and

.. code-block:: markdown

    Currently, this example supports the following targets.

    | Supported Targets | ESP32 | ESP32-S2 | ESP32-C3 | ESP32-S3 |
    | ----------------- | ----- | -------- | -------- | -------- |

Example Template
****************

The example template can be found `here <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Template/ExampleTemplate>`_ and can be used as a reference.

Legal Part
----------

Before a contribution can be accepted, you will need to sign our contributor agreement. You will be prompted for this automatically as part of the Pull Request process.

.. _Arduino style guide: https://docs.arduino.cc/learn/contributions/arduino-writing-style-guide
