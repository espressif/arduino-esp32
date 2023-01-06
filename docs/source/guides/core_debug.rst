##################################
Compile Arduino libs with ESP_LOGx
##################################

There are 2 primary approaches and both of them involve editing file ``configs/defconfig.common``.
Edit the file directly and then build.
Later you can ``git restore configs/defconfig.common`` to go back.
Copy the file ``cp configs/defconfig.common configs/defconfig.debug`` and edit the debug version.

``vim configs/defconfig.common`` or ``vim configs/defconfig.debug``

Edit **line 44** containing by default ``CONFIG_LOG_DEFAULT_LEVEL_ERROR=y`` to one of the following lines depending on your desired log level:

.. code-block:: bash

   CONFIG_LOG_DEFAULT_LEVEL_NONE=y # No output
   CONFIG_LOG_DEFAULT_LEVEL_ERROR=y # Errors - default
   CONFIG_LOG_DEFAULT_LEVEL_WARN=y # Warnings
   CONFIG_LOG_DEFAULT_LEVEL_INFO=y # Info
   CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y # Debug
   CONFIG_LOG_DEFAULT_LEVEL_VERBOSE=y # Verbose

Then simply build the libs for all SoCs or one specific SoC. Note that building for all SoCs takes a lot of time, so if you are working only with specific SoC(s), build only for those.

.. note::
   If you have copied the ``defconfig`` file and the debug settings are in file ``configs/defconfig.debug`` add flag ``debug`` to compilation command.
   Example : ``./build.sh debug``

   - **Option 1**: Build for all SoCs: ``./build.sh``
   - **Option 2**: Build for one SoC: ``./build.sh -t <soc>``. The exact text to choose the SoC:

      - ``esp32``
      - ``esp32s2``
      - ``esp32c3``
      - ``esp32s3``
      - Example: ``./build.sh -t esp32``
      - A wrong format or non-existing SoC will result in the error sed: can't read sdkconfig: No such file or directory

