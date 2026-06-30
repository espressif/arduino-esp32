############################
CLI Helper Functions (guide)
############################

About
-----

This page mirrors `helper_functions.md <https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/helper_functions.md>`_ — a walkthrough of
helper types and functions used with the OpenThread Arduino library. For the
full CLI class reference see :doc:`openthread_cli`.

Enumerated type: ``ot_device_role_t``
*************************************

Roles of a Thread device:

* ``OT_ROLE_DISABLED`` — stack disabled.
* ``OT_ROLE_DETACHED`` — not participating in a partition.
* ``OT_ROLE_CHILD`` — Thread Child.
* ``OT_ROLE_ROUTER`` — Thread Router.
* ``OT_ROLE_LEADER`` — Thread Leader.

Struct: ``ot_cmd_return_t``
***************************

Return status of a CLI command executed via ``otExecCommand()``:

* ``errorCode`` — OpenThread error code (0 on success).
* ``errorMessage`` — human-readable text.

``OpenThread`` instance methods
*******************************

These are **not** free helpers in ``OThreadCLI_Util.h``:

``otGetDeviceRole()``
^^^^^^^^^^^^^^^^^^^^^

Returns ``ot_device_role_t``. Call as ``OThread.otGetDeviceRole()``.

``otGetStringDeviceRole()``
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Returns a role string (e.g. ``"Child"``). Call as ``OThread.otGetStringDeviceRole()``.

``OpenThread::otPrintNetworkInformation(Stream &output)``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Static method that prints role, network name, channel, PAN ID, keys, and IP
addresses. Example: ``OThread.otPrintNetworkInformation(Serial);``

Requires ``OThread.begin()``. Do **not** call ``otPrintNetworkInformation()``
without the ``OThread.`` prefix.

CLI helper functions (``OThreadCLI_Util.h``)
********************************************

Free functions for programmatic CLI automation:

otGetRespCmd
^^^^^^^^^^^^

.. code-block:: arduino

    bool otGetRespCmd(const char *cmd, char *resp = NULL, size_t respBufSize = 0, uint32_t respTimeout = 5000);

When ``resp`` is a fixed-size stack array, pass only ``cmd`` and ``resp`` (buffer size is deduced; default timeout 5000 ms). Use ``otGetRespCmd(cmd, resp, timeoutMs)`` for a custom timeout. For a raw ``char*``, pass ``respBufSize`` explicitly; responses longer than ``respBufSize - 1`` bytes are truncated with a warning.

otExecCommand
^^^^^^^^^^^^^

.. code-block:: arduino

    bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode = NULL, uint32_t respTimeout = 5000);

otPrintRespCLI
^^^^^^^^^^^^^^

.. code-block:: arduino

    bool otPrintRespCLI(const char *cmd, Stream &output, uint32_t respTimeout);

See :doc:`openthread_cli` for full parameter descriptions and examples.
