############
ESP Insights
############

About
-----

ESP Insights is a remote diagnostics solution that allows users to remotely monitor the health of Espressif devices in the field.

Developers normally prefer debugging issues by physically probing them using gdb or observing the logs. This surely helps debug issues, but there are often cases wherein issues are seen only in specific environments under specific conditions. Even things like casings and placement of the product can affect the behavior. A few examples are

- Wi-Fi disconnections for a smart switch concealed in a wall.
- Smart speakers crashing during some specific usage pattern.
- Appliance frequently rebooting due to power supply issues.

Additional information about ESP Insights can be found `here <https://insights.espressif.com/>`__.

ESP Insights Agent API
----------------------

Insights.begin
**************

This initializes the ESP Insights agent.

.. code-block:: arduino

    bool begin(const char *auth_key, const char *node_id = NULL, uint32_t log_type = 0xFFFFFFFF, bool alloc_ext_ram = false);

* ``auth_key`` Auth key generated using Insights dashboard
* ``log_type`` Type of logs to be captured (value can be a mask of ESP_DIAG_LOG_TYPE_ERROR,  ESP_DIAG_LOG_TYPE_WARNING and ESP_DIAG_LOG_TYPE_EVENT)

This function will return

1. true : On success
2. false in case of failure

Insights.send
*************

Read insights data from buffers and send it to the cloud. Call to this function is asynchronous, it may take some time to send the data.

.. code-block:: arduino

    bool sendData()

This function will return

1. true : On success
2. false in case of failure

Insights.end
************

Deinitialize ESP Insights.

.. code-block:: arduino

    void end();

Insights.disable
****************

Disable ESP Insights.

.. code-block:: arduino

    void disable();

ESP Insights Metrics API
------------------------

`metrics` object of `Insights` class expose API's for using metrics.

Insights.metrics.addX
*********************

Register a metric of type X, where X is one of: Bool, Int, Uint, Float, String, IPv4 or MAC

.. code-block:: arduino

    bool addX(const char *tag, const char *key, const char *label, const char *path);

* ``tag`` :  Tag of metrics
* ``key`` :  Unique key for the metrics
* ``label`` : Label for the metrics
* ``path`` : Hierarchical path for key, must be separated by '.' for more than one level

This function will return

1. true : On success
2. false in case of failure

Insights.metrics.remove
***********************

Unregister a diagnostics metrics

.. code-block:: arduino

    bool remove(const char *key);

* ``key`` : Key for the metrics

This function will return

1. true : On success
2. false in case of failure

Insights.metrics.removeAll
**************************

Unregister all previously registered metrics

.. code-block:: arduino

    bool removeAll();

This function will return

1. true : On success
2. false in case of failure

Insights.metrics.setX
*********************

Add metrics of type X to storage, where X is one of: Bool, Int, Uint, Float, String, IPv4 or MAC

.. code-block:: arduino

    bool setX(const char *key, const void val);

* ``key`` :       Key of metrics
* ``val`` :       Value of metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Insights.metrics.dumpHeap
*************************

Dumps the heap metrics and prints them to the console.
This API collects and reports metrics value at any give point in time.

.. code-block:: arduino

    bool dumpHeap();

This function will return

1. true : On success
2. false in case of failure

Insights.metrics.dumpWiFi
*************************

Dumps the Wi-Fi metrics and prints them to the console.
This API can be used to collect Wi-Fi metrics at any given point in time.

.. code-block:: arduino

    bool dumpWiFi();

This function will return

1. true : On success
2. false in case of failure

Insights.metrics.setHeapPeriod
******************************

Reset the periodic interval
By default, heap metrics are collected every 30 seconds, this function can be used to change the interval.
If the interval is set to 0, heap metrics collection disabled.

.. code-block:: arduino

    void setHeapPeriod(uint32_t period);

* ``period`` : Period interval in seconds

Insights.metrics.setWiFiPeriod
******************************

Reset the periodic interval
By default, Wi-Fi metrics are collected every 30 seconds, this function can be used to change the interval.
If the interval is set to 0, Wi-Fi metrics collection disabled.

.. code-block:: arduino

    void setHeapPeriod(uint32_t period);

* ``period`` : Period interval in seconds

ESP Insights Variables API
--------------------------

`variables` object of `Insights` class expose API's for using variables.

Insights.variables.addX
***********************

Register a variable of type X, where X is one of: Bool, Int, Uint, Float, String, IPv4 or MAC

.. code-block:: arduino

    bool addX(const char *tag, const char *key, const char *label, const char *path);

* ``tag`` :  Tag of variable
* ``key`` :  Unique key for the variable
* ``label`` : Label for the variable
* ``path`` : Hierarchical path for key, must be separated by '.' for more than one level

This function will return

1. true : On success
2. false in case of failure

Insights.variables.remove
*************************

Unregister a diagnostics variable

.. code-block:: arduino

    bool remove(const char *key);

* ``key`` : Key for the variable

This function will return

1. true : On success
2. false in case of failure

Insights.variables.removeAll
****************************

Unregister all previously registered variables

.. code-block:: arduino

    bool unregisterAll();

This function will return

1. true : On success
2. false in case of failure

Insights.variables.setX
***********************

Add variable of type X to storage, where X is one of: Bool, Int, Uint, Float, String, IPv4 or MAC

.. code-block:: arduino

    bool setX(const char *key, const void val);

* ``key`` :       Key of metrics
* ``val`` :       Value of metrics

This function will return

1. true : On success
2. false in case of failure

Example
-------

To get started with Insights, you can try:

.. literalinclude:: ../../../libraries/Insights/examples/MinimalDiagnostics/MinimalDiagnostics.ino
    :language: arduino
