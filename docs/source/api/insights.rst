############
ESP Insights
############

About
-----

ESP Insights is a remote diagnostics solution that allows users to remotely monitor the health of ESP devices in the field.

Developers normally prefer debugging issues by physically probing them using gdb or observing the logs. This surely helps debug issues, but there are often cases wherein issues are seen only in specific environments under specific conditions. Even things like casings and placement of the product can affect the behaviour. A few examples are

- Wi-Fi disconnections for a smart switch concealed in a wall.
- Smart speakers crashing during some specific usage pattern.
- Appliance frequently rebooting due to power supply issues.

Additional information about ESP Insights can be found `here <https://insights.espressif.com/>`__.

ESP Insights Agent API
----------------------

Insights.init
*************

This initializes the ESP Insights agent.

.. code-block:: arduino

    esp_err_t init(const char *auth_key, uint32_t log_type, const char *node_id = NULL, bool alloc_ext_ram = false);

* ``auth_key`` Auth key generated using Insights dashboard
* ``log_type`` Type of logs to be captured (value can be ESP_DIAG_LOG_TYPE_ERROR or ESP_DIAG_LOG_TYPE_EVENT)

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Insights.enable
***************

This enables the ESP Insights agent. 

.. code-block:: arduino

    esp_err_t enable(const char *auth_key, uint32_t log_type, const char *node_id = NULL, bool alloc_ext_ram = false);

* ``auth_key`` Auth key generated using Insights dashboard
* ``log_type`` Type of logs to be captured (value can be ESP_DIAG_LOG_TYPE_ERROR or ESP_DIAG_LOG_TYPE_EVENT)

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Insights.registerTransport
**************************

Register insights transport.

.. code-block:: arduino

    esp_err_t registerTransport(esp_insights_transport_config_t *config);

* ``config`` : Configurations of type esp_insights_transport_config_t

This function will return

1. `ESP_OK` : On success
2. Error in case of failure.

Insights.sendData
*****************

Read insights data from buffers and send it to the cloud. Call to this function is asynchronous, it may take some time to send the data.

.. code-block:: arduino

    esp_err_t sendData()

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Insights.deinit
***************

Deinitialize ESP Insights.

.. code-block:: arduino

    void deinit();

Insights.disable
****************

Disable ESP Insights.

.. code-block:: arduino

    void disable();

Insights.unregisterTransport
****************************

Unregister insights transport

.. code-block:: arduino

    void unregisterTransport();

ESP Insights Metrics API
------------------------

`Metrics` object of `MetricsClass` class expose API's for using metrics.
The class is defined under "Metrics.h" header file.

Metrics.init
************

Initialize the diagnostics metrics

.. code-block:: arduino

    esp_err_t init(esp_diag_metrics_config_t *config);

* ``config`` : Pointer to a config structure of type esp_diag_metrics_config_t

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.registerMetric
**********************

Register a metric

.. code-block:: arduino

    esp_err_t registerMetric(const char *tag, const char *key, const char *label, const char *path, esp_diag_data_type_t type);

* ``tag`` :  Tag of metrics
* ``key`` :  Unique key for the metrics
* ``label`` : Label for the metrics
* ``path`` : Hierarchical path for key, must be separated by '.' for more than one level
* ``type`` : Data type of metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.unregister
******************

Unregister a diagnostics metrics

.. code-block:: arduino

    esp_err_t unregister(const char *key);

* ``key`` : Key for the metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.unregisterAll
*********************

Unregister all previously registered metrics

.. code-block:: arduino

    esp_err_t unregisterAll();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.add
***********

Add metrics to storage

.. code-block:: arduino

    esp_err_t add(esp_diag_data_type_t data_type, const char *key, const void *val, size_t val_sz, uint64_t ts);

* ``data_type`` : Data type of metrics esp_diag_data_type_t
* ``key`` :       Key of metrics
* ``val`` :       Value of metrics
* ``val_sz`` :    Size of val
* ``ts`` :        Timestamp in microseconds, this should be the value at the time of data gathering

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.addBool
***************

Add the metrics of data type boolean

.. code-block:: arduino

    esp_err_t addBool(const char *key, bool b);

* ``key`` :       Key of metrics
* ``b`` :       Value of metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.addInt
**************

Add the metrics of data type integer

.. code-block:: arduino

    esp_err_t addInt(const char *key, int32_t i);

* ``key`` :       Key of metrics
* ``i`` :       Value of metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.addUint
***************

Add the metrics of data type unsigned integer

.. code-block:: arduino

    esp_err_t addUint(const char *key, uint32_t u);

* ``key`` :       Key of metrics
* ``u`` :       Value of metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.addFloat
****************

Add the metrics of data type float

.. code-block:: arduino

    esp_err_t addFloat(const char *key, float f);

* ``key`` :       Key of metrics
* ``f`` :       Value of metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.addIPv4
***************

Add the IPv4 address metrics

.. code-block:: arduino

    esp_err_t addIPv4(const char *key, uint32_t ip);

* ``key`` :       Key of metrics
* ``ip`` :       Value of metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.addMAC
**************

Add the MAC address metrics

.. code-block:: arduino

    esp_err_t addMAC(const char *key, uint8_t *mac);

* ``key`` : Key of the metric
* ``mac`` : Array of length 6 i.e 6 octets of mac address

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.addString
*****************

Add the metrics of data type string

.. code-block:: arduino

    esp_err_t addString(const char *key, const char *str);

* ``key`` : Key of the metrics
* ``str`` : Value of the metrics

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.initHeapMetrics
***********************

Initialize the heap metrics.
Free heap, largest free block, and all time minimum free heap values are collected periodically. 
Parameters are collected for RAM in internal memory and external memory (if device has PSRAM). 
Default periodic interval is 30 seconds and can be changed with Metrics.resetHeapMetricsInterval().

.. code-block:: arduino

    esp_err_t initHeapMetrics();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.dumpHeapMetrics
***********************

Dumps the heap metrics and prints them to the console.
This API collects and reports metrics value at any give point in time.

.. code-block:: arduino

    esp_err_t dumpHeapMetrics();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.initWiFiMetrics
***********************

Initialize the wifi metrics
Wi-Fi RSSI and minimum ever Wi-Fi RSSI values are collected periodically.
Default periodic interval is 30 seconds and can be changed with Metrics.resetWiFiMetricsInterval().

.. code-block:: arduino

    esp_err_t initWiFiMetrics();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.dumpWiFiMetrics
***********************

Dumps the wifi metrics and prints them to the console.
This API can be used to collect wifi metrics at any given point in time.

.. code-block:: arduino

    esp_err_t dumpWiFiMetrics();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.deinit
**************

Deinitialize the diagnostics metrics

.. code-block:: arduino

    esp_err_t deinit();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.resetHeapMetricsInterval
********************************

Reset the periodic interval
By default, heap metrics are collected every 30 seconds, this function can be used to change the interval.
If the interval is set to 0, heap metrics collection disabled.

.. code-block:: arduino

    void resetHeapMetricsInterval(uint32_t period);

* ``period`` : Period interval in seconds

Metrics.resetWiFiMetricsInterval
********************************

Reset the periodic interval
By default, wifi metrics are collected every 30 seconds, this function can be used to change the interval.
If the interval is set to 0, wifi metrics collection disabled.

.. code-block:: arduino

    void resetWiFiMetricsInterval(uint32_t period);

* ``period`` : Period interval in seconds

Metrics.deinitHeapMetrics
*************************

Deinitialize the heap metrics

.. code-block:: arduino

    esp_err_t deinitHeapMetrics();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Metrics.deinitWiFiMetrics
*************************

Deinitialize the wifi metrics

.. code-block:: arduino

    esp_err_t deinitWiFiMetrics();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

ESP Insights Variables API
--------------------------

`Variables` object of `VariablesClass` class expose API's for using variables.
This class is defined in "Variables.h" header file.

Variables.init
**************

Initialize the diagnostics metrics

.. code-block:: arduino

    esp_err_t init(esp_diag_variable_config_t *config);

* ``config`` : Pointer to a config structure of type esp_diag_variable_config_t

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.registerVariable
**************************

Register a variable

.. code-block:: arduino

    esp_err_t registerVariable(const char *tag, const char *key, const char *label, const char *path, esp_diag_data_type_t type);

* ``tag`` :  Tag of variable
* ``key`` :  Unique key for the variable
* ``label`` : Label for the variable
* ``path`` : Hierarchical path for key, must be separated by '.' for more than one level
* ``type`` : Data type of variable.

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.unregister
********************

Unregister a diagnostics variable

.. code-block:: arduino

    esp_err_t unregister(const char *key);

* ``key`` : Key for the variable

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.unregisterAll
***********************

Unregister all previously registered variables

.. code-block:: arduino

    esp_err_t unregisterAll();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.add
*************

Add variables to storage

.. code-block:: arduino

    esp_err_t add(esp_diag_data_type_t data_type, const char *key, const void *val, size_t val_sz, uint64_t ts);

* ``data_type`` : Data type of metrics \ref esp_diag_data_type_t
* ``key`` :       Key of metrics
* ``val`` :       Value of metrics
* ``val_sz`` :    Size of val
* ``ts`` :        Timestamp in microseconds, this should be the value at the time of data gathering

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.addBool
*****************

Add the variable of data type boolean

.. code-block:: arduino

    esp_err_t addBool(const char *key, bool b);

* ``key`` :       Key of variable
* ``b`` :       Value of variable

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.addInt
****************

Add the variable of data type integer

.. code-block:: arduino

    esp_err_t addInt(const char *key, int32_t i);

* ``key`` :       Key of variable
* ``i`` :       Value of variable

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.addUint
*****************

Add the variable of data type unsigned integer

.. code-block:: arduino

    esp_err_t addUint(const char *key, uint32_t u);

* ``key`` :       Key of variable
* ``u`` :       Value of variable

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.addFloat
******************

Add the variable of data type float

.. code-block:: arduino

    esp_err_t addFloat(const char *key, float f);

* ``key`` :       Key of variable
* ``f`` :       Value of variable

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.addIPv4
*****************

Add the IPv4 address variable

.. code-block:: arduino

    esp_err_t addIPv4(const char *key, uint32_t ip);

* ``key`` :       Key of variable
* ``ip`` :       Value of variable

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.addMAC
****************

Add the MAC address variable

.. code-block:: arduino

    esp_err_t addMAC(const char *key, uint8_t *mac);

* ``key`` : Key of the variable
* ``mac`` : Array of length 6 i.e 6 octets of mac address

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.addString
*******************

Add the variable of data type string

.. code-block:: arduino

    esp_err_t addString(const char *key, const char *str);

* ``key`` : Key of the variable
* ``str`` : Value of the variable

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.initNetworkVariables
******************************

Initialize the network variables
Below listed Wi-Fi and IP parameters are collected and reported to cloud on change.
Wi-Fi connection status, BSSID, SSID, channel, authentication mode,
Wi-Fi disconnection reason, IP address, netmask, and gateway.

.. code-block:: arduino

    esp_err_t initNetworkVariables();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.deinitNetworkVariables
********************************

Deinitialize the network variables

.. code-block:: arduino

    esp_err_t deinitNetworkVariables();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Variables.deinit
****************

Deinitialize the diagnostics variables

.. code-block:: arduino

    esp_err_t deinit();

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

ESP Insights Diagnostics API
----------------------------

`Diagnostics` object of `DiagnosticsClass` class exposes API's for reporting logs to the Insights Cloud
This class is defined in "Diagnostics.h" header file.

Diagnostics.initLogHook
***********************

Initialize diagnostics log hook

.. code-block:: arduino

    esp_err_t initLogHook(esp_diag_log_write_cb_t write_cb, void *cb_arg = NULL);

* ``write_cb`` : Callback function to write diagnostics data

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Diagnostics.enableLogHook
*************************

Enable the diagnostics log hook for provided log type

.. code-block:: arduino

    void enableLogHook(uint32_t type);

* ``type`` : Log type to enable, can be the bitwise OR of types from esp_diag_log_type_t

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Diagnostics.disableLogHook
**************************

Disable the diagnostics log hook for provided log type

.. code-block:: arduino

    void disableLogHook(uint32_t type);

* ``type`` : Log type to disable, can be the bitwise OR of types from esp_diag_log_type_t

This function will return

1. `ESP_OK` : On success
2. Error in case of failure

Example
-------

To get started with Insights, you can try:

.. literalinclude:: ../../../libraries/Insights/examples/MinimalDiagnostics/MinimalDiagnostics.ino
    :language: arduino
