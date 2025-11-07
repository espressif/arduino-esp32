#############
DataSet Class
#############

About
-----

The ``DataSet`` class provides a convenient way to create, configure, and manage Thread operational datasets. An operational dataset contains all the parameters needed to join or form a Thread network, including network name, channel, PAN ID, network key, and extended PAN ID.

**Key Features:**
* Create new operational datasets
* Configure dataset parameters
* Apply datasets to the Thread network
* Retrieve dataset parameters
* Clear and reset datasets

**Use Cases:**
* Creating new Thread networks
* Joining existing Thread networks
* Configuring network parameters
* Network migration and reconfiguration

API Reference
-------------

Constructor
***********

DataSet
^^^^^^^

Creates a new DataSet object.

.. code-block:: arduino

    DataSet();

This constructor creates an empty dataset. You must either call ``initNew()`` to create a new dataset or configure parameters manually.

Dataset Management
******************

clear
^^^^^

Clears all dataset parameters.

.. code-block:: arduino

    void clear();

This function clears all dataset parameters, resetting the dataset to an empty state.

initNew
^^^^^^^

Initializes a new operational dataset.

.. code-block:: arduino

    void initNew();

This function creates a new operational dataset with randomly generated values for network key, extended PAN ID, and other parameters. The dataset will be ready to form a new Thread network.

**Note:** OpenThread must be started (``OpenThread::begin()``) before calling this function.

**Example:**

.. code-block:: arduino

    DataSet dataset;
    dataset.initNew();  // Creates new dataset with random values
    dataset.setNetworkName("MyNewNetwork");
    dataset.setChannel(15);

getDataset
^^^^^^^^^^

Gets the underlying OpenThread dataset structure.

.. code-block:: arduino

    const otOperationalDataset &getDataset() const;

This function returns a reference to the underlying ``otOperationalDataset`` structure. This is used internally when applying the dataset to the Thread network.

**Note:** This function is typically used internally by ``OpenThread::commitDataSet()``.

Setters
*******

setNetworkName
^^^^^^^^^^^^^^

Sets the network name.

.. code-block:: arduino

    void setNetworkName(const char *name);

* ``name`` - The network name string (maximum 16 characters)

This function sets the network name for the Thread network. The network name is a human-readable identifier for the network.

**Example:**

.. code-block:: arduino

    dataset.setNetworkName("MyThreadNetwork");

setExtendedPanId
^^^^^^^^^^^^^^^^

Sets the extended PAN ID.

.. code-block:: arduino

    void setExtendedPanId(const uint8_t *extPanId);

* ``extPanId`` - Pointer to an 8-byte array containing the extended PAN ID

This function sets the extended PAN ID, which uniquely identifies the Thread network partition.

**Example:**

.. code-block:: arduino

    uint8_t extPanId[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    dataset.setExtendedPanId(extPanId);

setNetworkKey
^^^^^^^^^^^^^

Sets the network key.

.. code-block:: arduino

    void setNetworkKey(const uint8_t *key);

* ``key`` - Pointer to a 16-byte array containing the network key

This function sets the network key, which is used for encryption and authentication in the Thread network.

**Example:**

.. code-block:: arduino

    uint8_t networkKey[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                               0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    dataset.setNetworkKey(networkKey);

setChannel
^^^^^^^^^^

Sets the Thread channel.

.. code-block:: arduino

    void setChannel(uint8_t channel);

* ``channel`` - The Thread channel number (11-26)

This function sets the IEEE 802.15.4 channel used by the Thread network. Valid channels are 11 through 26.

**Example:**

.. code-block:: arduino

    dataset.setChannel(15);  // Use channel 15

setPanId
^^^^^^^^

Sets the PAN ID.

.. code-block:: arduino

    void setPanId(uint16_t panId);

* ``panId`` - The PAN ID (16-bit value)

This function sets the PAN (Personal Area Network) ID for the Thread network.

**Example:**

.. code-block:: arduino

    dataset.setPanId(0x1234);  // Set PAN ID to 0x1234

Getters
*******

getNetworkName
^^^^^^^^^^^^^^

Gets the network name.

.. code-block:: arduino

    const char *getNetworkName() const;

This function returns a pointer to the network name string.

**Returns:** Pointer to the network name, or ``NULL`` if not set.

getExtendedPanId
^^^^^^^^^^^^^^^^

Gets the extended PAN ID.

.. code-block:: arduino

    const uint8_t *getExtendedPanId() const;

This function returns a pointer to the 8-byte extended PAN ID array.

**Returns:** Pointer to the extended PAN ID array, or ``NULL`` if not set.

getNetworkKey
^^^^^^^^^^^^^

Gets the network key.

.. code-block:: arduino

    const uint8_t *getNetworkKey() const;

This function returns a pointer to the 16-byte network key array.

**Returns:** Pointer to the network key array, or ``NULL`` if not set.

getChannel
^^^^^^^^^^

Gets the Thread channel.

.. code-block:: arduino

    uint8_t getChannel() const;

This function returns the Thread channel number.

**Returns:** The channel number (11-26), or 0 if not set.

getPanId
^^^^^^^^

Gets the PAN ID.

.. code-block:: arduino

    uint16_t getPanId() const;

This function returns the PAN ID.

**Returns:** The PAN ID, or 0 if not set.

Dataset Application
*******************

apply
^^^^^

Applies the dataset to an OpenThread instance.

.. code-block:: arduino

    void apply(otInstance *instance);

* ``instance`` - Pointer to the OpenThread instance

This function applies the dataset to the specified OpenThread instance, making it the active operational dataset.

**Note:** This function is typically used internally. For normal use, use ``OpenThread::commitDataSet()`` instead.

**Example:**

.. code-block:: arduino

    otInstance *instance = OThread.getInstance();
    dataset.apply(instance);

Example
-------

Creating a New Network
**********************

.. code-block:: arduino

    #include <OThread.h>

    void setup() {
        Serial.begin(115200);
        
        // Initialize OpenThread
        OpenThread::begin();
        while (!OThread) {
            delay(100);
        }
        
        // Create a new dataset
        DataSet dataset;
        dataset.initNew();  // Generate random values
        
        // Configure network parameters
        dataset.setNetworkName("MyNewThreadNetwork");
        dataset.setChannel(15);
        
        // Set network key (16 bytes)
        uint8_t networkKey[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        dataset.setNetworkKey(networkKey);
        
        // Set extended PAN ID (8 bytes)
        uint8_t extPanId[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
        dataset.setExtendedPanId(extPanId);
        
        // Set PAN ID
        dataset.setPanId(0x1234);
        
        // Apply dataset and start network
        OThread.commitDataSet(dataset);
        OThread.start();
        OThread.networkInterfaceUp();
        
        Serial.println("New Thread network created");
    }

Joining an Existing Network
***************************

.. code-block:: arduino

    #include <OThread.h>

    void setup() {
        Serial.begin(115200);
        
        // Initialize OpenThread
        OpenThread::begin();
        while (!OThread) {
            delay(100);
        }
        
        // Create dataset for existing network
        DataSet dataset;
        
        // Configure with existing network parameters
        dataset.setNetworkName("ExistingThreadNetwork");
        dataset.setChannel(15);
        
        // Set the network key from the existing network
        uint8_t networkKey[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        dataset.setNetworkKey(networkKey);
        
        // Set the extended PAN ID from the existing network
        uint8_t extPanId[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
        dataset.setExtendedPanId(extPanId);
        
        // Set PAN ID
        dataset.setPanId(0x1234);
        
        // Apply dataset and start network
        OThread.commitDataSet(dataset);
        OThread.start();
        OThread.networkInterfaceUp();
        
        Serial.println("Joining existing Thread network");
    }

Reading Current Dataset
***********************

.. code-block:: arduino

    void printCurrentDataset() {
        // Get current dataset from OpenThread
        const DataSet &currentDataset = OThread.getCurrentDataSet();
        
        // Print dataset parameters
        Serial.println("Current Thread Dataset:");
        Serial.printf("  Network Name: %s\r\n", currentDataset.getNetworkName());
        Serial.printf("  Channel: %d\r\n", currentDataset.getChannel());
        Serial.printf("  PAN ID: 0x%04x\r\n", currentDataset.getPanId());
        
        // Print extended PAN ID
        const uint8_t *extPanId = currentDataset.getExtendedPanId();
        if (extPanId) {
            Serial.print("  Extended PAN ID: ");
            for (int i = 0; i < 8; i++) {
                Serial.printf("%02x", extPanId[i]);
            }
            Serial.println();
        }
        
        // Print network key (first 4 bytes for security)
        const uint8_t *networkKey = currentDataset.getNetworkKey();
        if (networkKey) {
            Serial.print("  Network Key: ");
            for (int i = 0; i < 4; i++) {
                Serial.printf("%02x", networkKey[i]);
            }
            Serial.println("...");
        }
    }

Modifying Dataset Parameters
****************************

.. code-block:: arduino

    void modifyNetworkChannel() {
        // Get current dataset
        DataSet dataset = OThread.getCurrentDataSet();
        
        // Modify channel
        dataset.setChannel(20);  // Change to channel 20
        
        // Apply modified dataset
        OThread.commitDataSet(dataset);
        
        Serial.println("Network channel changed to 20");
    }

Best Practices
--------------

Dataset Security
****************

* **Network Key**: Keep the network key secure and never expose it in logs or serial output
* **Extended PAN ID**: Use unique extended PAN IDs to avoid network conflicts
* **Channel Selection**: Choose channels that avoid interference with Wi-Fi networks (channels 11, 15, 20, 25 are often good choices)

Required Parameters
*******************

For a dataset to be valid and usable, you typically need:

* **Network Name**: Required for human identification
* **Network Key**: Required for security (16 bytes)
* **Channel**: Required for radio communication (11-26)
* **Extended PAN ID**: Recommended for network identification (8 bytes)
* **PAN ID**: Optional, will be assigned if not set

Parameter Validation
********************

The DataSet class performs basic validation:

* Network name length is checked (maximum 16 characters)
* Channel range is validated (11-26)
* Null pointer checks for array parameters

However, it's your responsibility to ensure:

* Network key is properly generated (use ``initNew()`` for random generation)
* Extended PAN ID is unique within your network environment
* All required parameters are set before applying the dataset

