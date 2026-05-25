##############
OpenThread CLI
##############

About
-----

The OpenThread CLI (Command-Line Interface) provides two ways to interact with the OpenThread stack through CLI commands:

* **CLI Helper Functions API**: Utility functions that execute CLI commands and parse responses
* **OpenThreadCLI Class**: Stream-based interface for interactive CLI access

The CLI Helper Functions API is useful for programmatic control using OpenThread CLI commands, while the ``OpenThreadCLI`` class provides a Stream interface for interactive console access.

CLI Helper Functions API
************************

The CLI Helper Functions API consists of utility functions that execute OpenThread CLI commands and handle responses. These functions interact with the OpenThread CLI through the ``OpenThreadCLI`` interface.

otGetRespCmd
^^^^^^^^^^^^

Executes a CLI command and gets the response.

.. code-block:: arduino

    bool otGetRespCmd(const char *cmd, char *resp = NULL, uint32_t respTimeout = 5000);

* ``cmd`` - The CLI command to execute (e.g., ``"state"``, ``"networkname"``)
* ``resp`` - Buffer to store the response (optional, can be ``NULL``)
* ``respTimeout`` - Timeout in milliseconds for waiting for response (default: 5000 ms)

This function executes a CLI command and collects all response lines until "Done" or "Error" is received. If ``resp`` is not ``NULL``, the response is stored in the buffer.

**Returns:** ``true`` if command executed successfully, ``false`` on error or timeout.

**Example:**

.. code-block:: arduino

    char response[256];
    if (otGetRespCmd("state", response)) {
        Serial.printf("Thread state: %s\r\n", response);
    }

    if (otGetRespCmd("networkname", response)) {
        Serial.printf("Network name: %s\r\n", response);
    }

otExecCommand
^^^^^^^^^^^^^

Executes a CLI command with arguments.

.. code-block:: arduino

    bool otExecCommand(const char *cmd, const char *arg, ot_cmd_return_t *returnCode = NULL);

* ``cmd`` - The CLI command to execute (e.g., ``"networkname"``, ``"channel"``)
* ``arg`` - The command argument (can be ``NULL`` for commands without arguments)
* ``returnCode`` - Pointer to ``ot_cmd_return_t`` structure to receive error information (optional)

This function executes a CLI command with an optional argument and returns the success status. If ``returnCode`` is provided, it will be populated with error information on failure.

**Returns:** ``true`` if command executed successfully, ``false`` on error.

**Error Structure:**

.. code-block:: arduino

    typedef struct {
        int errorCode;        // OpenThread error code
        String errorMessage;  // Error message string
    } ot_cmd_return_t;

**Example:**

.. code-block:: arduino

    ot_cmd_return_t errorInfo;

    // Set network name
    if (otExecCommand("networkname", "MyThreadNetwork", &errorInfo)) {
        Serial.println("Network name set successfully");
    } else {
        Serial.printf("Error %d: %s\r\n", errorInfo.errorCode, errorInfo.errorMessage.c_str());
    }

    // Set channel
    if (otExecCommand("channel", "15", &errorInfo)) {
        Serial.println("Channel set successfully");
    }

    // Bring interface up
    if (otExecCommand("ifconfig", "up", NULL)) {
        Serial.println("Interface is up");
    }

otPrintRespCLI
^^^^^^^^^^^^^^

Executes a CLI command and prints the response to a Stream.

.. code-block:: arduino

    bool otPrintRespCLI(const char *cmd, Stream &output, uint32_t respTimeout);

* ``cmd`` - The CLI command to execute
* ``output`` - The Stream object to print responses to (e.g., ``Serial``)
* ``respTimeout`` - Timeout in milliseconds per response line (default: 5000 ms)

This function executes a CLI command and prints all response lines to the specified Stream until "Done" or "Error" is received.

**Returns:** ``true`` if command executed successfully, ``false`` on error or timeout.

**Example:**

.. code-block:: arduino

    // Print all IP addresses
    if (otPrintRespCLI("ipaddr", Serial, 5000)) {
        Serial.println("IP addresses printed");
    }

    // Print all multicast addresses
    if (otPrintRespCLI("ipmaddr", Serial, 5000)) {
        Serial.println("Multicast addresses printed");
    }

OpenThreadCLI Class
*******************

The ``OpenThreadCLI`` class provides a Stream-based interface for interacting with the OpenThread CLI. It allows you to send CLI commands and receive responses programmatically or through an interactive console.

Initialization
**************

begin
^^^^^

Initializes the OpenThread CLI.

.. code-block:: arduino

    void begin();

This function initializes the OpenThread CLI interface. It must be called after ``OpenThread::begin()`` and before using any CLI functions.

**Note:** The OpenThread stack must be started before initializing the CLI.

end
^^^

Stops and cleans up the OpenThread CLI.

.. code-block:: arduino

    void end();

This function stops the CLI interface and cleans up all CLI resources.

Console Management
******************

startConsole
^^^^^^^^^^^^

Starts an interactive console for CLI access.

.. code-block:: arduino

    void startConsole(Stream &otStream, bool echoback = true, const char *prompt = "ot> ");

* ``otStream`` - The Stream object for console I/O (e.g., ``Serial``)
* ``echoback`` - If ``true``, echo characters back to the console (default: ``true``)
* ``prompt`` - The console prompt string (default: ``"ot> "``, can be ``NULL`` for no prompt)

This function starts an interactive console task that allows you to type CLI commands directly. The console will echo input and display responses.

**Example:**

.. code-block:: arduino

    OThreadCLI.startConsole(Serial, true, "ot> ");

stopConsole
^^^^^^^^^^^

Stops the interactive console.

.. code-block:: arduino

    void stopConsole();

This function stops the interactive console task.

setStream
^^^^^^^^^

Changes the console Stream object.

.. code-block:: arduino

    void setStream(Stream &otStream);

* ``otStream`` - The new Stream object for console I/O

This function changes the Stream object used by the console.

setEchoBack
^^^^^^^^^^^

Changes the echo back setting.

.. code-block:: arduino

    void setEchoBack(bool echoback);

* ``echoback`` - If ``true``, echo characters back to the console

This function changes whether characters are echoed back to the console.

setPrompt
^^^^^^^^^

Changes the console prompt.

.. code-block:: arduino

    void setPrompt(char *prompt);

* ``prompt`` - The new prompt string (can be ``NULL`` for no prompt)

This function changes the console prompt string.

onReceive
^^^^^^^^^

Sets a callback function for CLI responses.

.. code-block:: arduino

    void onReceive(OnReceiveCb_t func);

* ``func`` - Callback function to call when a complete line of output is received

The callback function is called whenever a complete line of output is received from the OpenThread CLI. This allows you to process CLI responses asynchronously.

**Callback Signature:**

.. code-block:: arduino

    typedef std::function<void(void)> OnReceiveCb_t;

**Example:**

.. code-block:: arduino

    void handleCLIResponse() {
        while (OThreadCLI.available() > 0) {
            char c = OThreadCLI.read();
            // Process response character
        }
    }

    OThreadCLI.onReceive(handleCLIResponse);

Buffer Management
*****************

setTxBufferSize
^^^^^^^^^^^^^^^

Sets the transmit buffer size.

.. code-block:: arduino

    size_t setTxBufferSize(size_t tx_queue_len);

* ``tx_queue_len`` - The size of the transmit buffer in bytes (default: 256)

This function sets the size of the transmit buffer used for sending CLI commands.

**Returns:** The actual buffer size set, or 0 on error.

setRxBufferSize
^^^^^^^^^^^^^^^

Sets the receive buffer size.

.. code-block:: arduino

    size_t setRxBufferSize(size_t rx_queue_len);

* ``rx_queue_len`` - The size of the receive buffer in bytes (default: 1024)

This function sets the size of the receive buffer used for receiving CLI responses.

**Returns:** The actual buffer size set, or 0 on error.

Stream Interface
****************

The ``OpenThreadCLI`` class implements the Arduino ``Stream`` interface, allowing you to use it like any other Stream object.

write
^^^^^

Writes a byte to the CLI.

.. code-block:: arduino

    size_t write(uint8_t c);

* ``c`` - The byte to write

This function writes a single byte to the CLI transmit buffer.

**Returns:** The number of bytes written (1 on success, 0 on failure).

available
^^^^^^^^^

Checks if data is available to read.

.. code-block:: arduino

    int available();

This function returns the number of bytes available in the receive buffer.

**Returns:** Number of bytes available, or -1 if CLI is not initialized.

read
^^^^

Reads a byte from the CLI.

.. code-block:: arduino

    int read();

This function reads a single byte from the CLI receive buffer.

**Returns:** The byte read, or -1 if no data is available.

peek
^^^^

Peeks at the next byte without removing it.

.. code-block:: arduino

    int peek();

This function returns the next byte in the receive buffer without removing it.

**Returns:** The byte, or -1 if no data is available.

flush
^^^^^

Flushes the transmit buffer.

.. code-block:: arduino

    void flush();

This function waits for all data in the transmit buffer to be sent.

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns whether the CLI is started.

.. code-block:: arduino

    operator bool() const;

This operator returns ``true`` if the CLI is started and ready, ``false`` otherwise.

**Example:**

.. code-block:: arduino

    if (OThreadCLI) {
        Serial.println("CLI is ready");
    }

Example
-------

Using CLI Helper Functions API
******************************

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadCLI.h>
    #include <OThreadCLI_Util.h>

    void setup() {
        Serial.begin(115200);

        // Initialize OpenThread
        OpenThread::begin();
        while (!OThread) {
            delay(100);
        }

        // Initialize CLI
        OThreadCLI.begin();
        while (!OThreadCLI) {
            delay(100);
        }

        // Get network state
        char resp[256];
        if (otGetRespCmd("state", resp)) {
            Serial.printf("Thread state: %s\r\n", resp);
        }

        // Set network name
        ot_cmd_return_t errorInfo;
        if (otExecCommand("networkname", "MyThreadNetwork", &errorInfo)) {
            Serial.println("Network name set");
        } else {
            Serial.printf("Error: %s\r\n", errorInfo.errorMessage.c_str());
        }

        // Set channel
        if (otExecCommand("channel", "15", NULL)) {
            Serial.println("Channel set");
        }

        // Bring interface up
        if (otExecCommand("ifconfig", "up", NULL)) {
            Serial.println("Interface up");
        }

        // Print IP addresses
        otPrintRespCLI("ipaddr", Serial, 5000);
    }

Using OpenThreadCLI Class
*************************

Interactive Console
^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadCLI.h>

    void setup() {
        Serial.begin(115200);

        // Initialize OpenThread
        OpenThread::begin();
        while (!OThread) {
            delay(100);
        }

        // Initialize and start CLI console
        OThreadCLI.begin();
        OThreadCLI.startConsole(Serial, true, "ot> ");

        Serial.println("OpenThread CLI Console Ready");
        Serial.println("Type OpenThread CLI commands (e.g., 'state', 'networkname')");
    }

Programmatic CLI Access
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadCLI.h>

    void setup() {
        Serial.begin(115200);

        // Initialize OpenThread
        OpenThread::begin();
        while (!OThread) {
            delay(100);
        }

        // Initialize CLI
        OThreadCLI.begin();
        while (!OThreadCLI) {
            delay(100);
        }

        // Send CLI commands programmatically
        OThreadCLI.println("state");
        delay(100);
        while (OThreadCLI.available() > 0) {
            char c = OThreadCLI.read();
            Serial.write(c);
        }

        // Send command with argument
        OThreadCLI.print("networkname ");
        OThreadCLI.println("MyThreadNetwork");
        delay(100);
        while (OThreadCLI.available() > 0) {
            char c = OThreadCLI.read();
            Serial.write(c);
        }
    }

Using Callback for Responses
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadCLI.h>

    void handleCLIResponse() {
        String response = "";
        while (OThreadCLI.available() > 0) {
            char c = OThreadCLI.read();
            if (c == '\n' || c == '\r') {
                if (response.length() > 0) {
                    Serial.printf("CLI Response: %s\r\n", response.c_str());
                    response = "";
                }
            } else {
                response += c;
            }
        }
    }

    void setup() {
        Serial.begin(115200);

        // Initialize OpenThread
        OpenThread::begin();
        while (!OThread) {
            delay(100);
        }

        // Initialize CLI with callback
        OThreadCLI.begin();
        OThreadCLI.onReceive(handleCLIResponse);

        // Send commands
        OThreadCLI.println("state");
        delay(500);
        OThreadCLI.println("networkname");
        delay(500);
    }

Common OpenThread CLI Commands
******************************

Here are some commonly used OpenThread CLI commands:

* ``state`` - Get the current Thread state
* ``networkname <name>`` - Set or get the network name
* ``channel <channel>`` - Set or get the channel (11-26)
* ``panid <panid>`` - Set or get the PAN ID
* ``extpanid <extpanid>`` - Set or get the extended PAN ID
* ``networkkey <key>`` - Set or get the network key
* ``ifconfig up`` - Bring the network interface up
* ``ifconfig down`` - Bring the network interface down
* ``ipaddr`` - List all IPv6 addresses
* ``ipmaddr`` - List all multicast addresses
* ``rloc16`` - Get the RLOC16
* ``leaderdata`` - Get leader data
* ``router table`` - Get router table
* ``child table`` - Get child table

For a complete list of OpenThread CLI commands, refer to the `OpenThread CLI Reference <https://openthread.io/reference/cli>`_.
