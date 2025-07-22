#include "OThread.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

#include "IPAddress.h"
#include <vector>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_vfs_eventfd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_netif_net_stack.h"
#include "esp_openthread_netif_glue.h"
#include "lwip/netif.h"

static esp_openthread_platform_config_t ot_native_config;
static esp_netif_t *openthread_netif = NULL;

const char *otRoleString[] = {
  "Disabled",  ///< The Thread stack is disabled.
  "Detached",  ///< Not currently participating in a Thread network/partition.
  "Child",     ///< The Thread Child role.
  "Router",    ///< The Thread Router role.
  "Leader",    ///< The Thread Leader role.
  "Unknown",   ///< Unknown role, not initialized or not started.
};

static TaskHandle_t s_ot_task = NULL;
#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
static struct netif *ot_lwip_netif = NULL;
#endif

#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) {
  if (ot_lwip_netif && ot_lwip_netif == inp) {
    return 0;
  }
  if (ip6_addr_isany_val(inp->ip6_addr[0].u_addr.ip6)) {
    // We don't have an LL address -> eat this packet here, so it won't get accepted on input netif
    pbuf_free(p);
    return 1;
  }
  return 0;
}
#endif

static void ot_task_worker(void *aContext) {
  esp_vfs_eventfd_config_t eventfd_config = {
    .max_fds = 3,
  };
  bool err = false;
  if (ESP_OK != esp_event_loop_create_default()) {
    log_e("Failed to create OpentThread event loop");
    err = true;
  }
  if (!err && ESP_OK != esp_netif_init()) {
    log_e("Failed to initialize OpentThread netif");
    err = true;
  }
  if (!err && ESP_OK != esp_vfs_eventfd_register(&eventfd_config)) {
    log_e("Failed to register OpentThread eventfd");
    err = true;
  }

  // Initialize the OpenThread stack
  if (!err && ESP_OK != esp_openthread_init(&ot_native_config)) {
    log_e("Failed to initialize OpenThread stack");
    err = true;
  }
  if (!err) {
    // Initialize the esp_netif bindings
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    openthread_netif = esp_netif_new(&cfg);
#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
    // Get LwIP Netif
    if (openthread_netif != NULL) {
      ot_lwip_netif = (struct netif *)esp_netif_get_netif_impl(openthread_netif);
      if (ot_lwip_netif == NULL) {
        log_e("Failed to get OpenThread LwIP netif");
      }
    }
#endif
  }
  if (!err && openthread_netif == NULL) {
    log_e("Failed to create OpenThread esp_netif");
    err = true;
  }
  if (!err && ESP_OK != esp_netif_attach(openthread_netif, esp_openthread_netif_glue_init(&ot_native_config))) {
    log_e("Failed to attach OpenThread esp_netif");
    err = true;
  }
  if (!err && ESP_OK != esp_netif_set_default_netif(openthread_netif)) {
    log_e("Failed to set default OpenThread esp_netif");
    err = true;
  }
  if (!err) {
    // only returns in case there is an OpenThread Stack failure...
    esp_openthread_launch_mainloop();
  }
  // Clean up
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(openthread_netif);
  esp_vfs_eventfd_unregister();
  vTaskDelete(NULL);
}

// DataSet Implementation
DataSet::DataSet() {
  memset(&mDataset, 0, sizeof(mDataset));
}

void DataSet::clear() {
  memset(&mDataset, 0, sizeof(mDataset));
}

void DataSet::initNew() {
  otInstance *mInstance = esp_openthread_get_instance();
  if (!mInstance) {
    log_e("OpenThread not started. Please begin() it before initializing a new dataset.");
    return;
  }
  clear();
  otDatasetCreateNewNetwork(mInstance, &mDataset);
}

const otOperationalDataset &DataSet::getDataset() const {
  return mDataset;
}

void DataSet::setNetworkName(const char *name) {
  if (!name) {
    log_w("Network name is null");
    return;
  }
  // char m8[OT_NETWORK_KEY_SIZE + 1] bytes space by definition
  strncpy(mDataset.mNetworkName.m8, name, OT_NETWORK_KEY_SIZE);
  mDataset.mComponents.mIsNetworkNamePresent = true;
}

void DataSet::setExtendedPanId(const uint8_t *extPanId) {
  if (!extPanId) {
    log_w("Extended PAN ID is null");
    return;
  }
  memcpy(mDataset.mExtendedPanId.m8, extPanId, OT_EXT_PAN_ID_SIZE);
  mDataset.mComponents.mIsExtendedPanIdPresent = true;
}

void DataSet::setNetworkKey(const uint8_t *key) {
  if (!key) {
    log_w("Network key is null");
    return;
  }
  memcpy(mDataset.mNetworkKey.m8, key, OT_NETWORK_KEY_SIZE);
  mDataset.mComponents.mIsNetworkKeyPresent = true;
}

void DataSet::setChannel(uint8_t channel) {
  mDataset.mChannel = channel;
  mDataset.mComponents.mIsChannelPresent = true;
}

void DataSet::setPanId(uint16_t panId) {
  mDataset.mPanId = panId;
  mDataset.mComponents.mIsPanIdPresent = true;
}

const char *DataSet::getNetworkName() const {
  return mDataset.mNetworkName.m8;
}

const uint8_t *DataSet::getExtendedPanId() const {
  return mDataset.mExtendedPanId.m8;
}

const uint8_t *DataSet::getNetworkKey() const {
  return mDataset.mNetworkKey.m8;
}

uint8_t DataSet::getChannel() const {
  return mDataset.mChannel;
}

uint16_t DataSet::getPanId() const {
  return mDataset.mPanId;
}

void DataSet::apply(otInstance *instance) {
  otDatasetSetActive(instance, &mDataset);
}

// OpenThread Implementation
bool OpenThread::otStarted;
otInstance *OpenThread::mInstance;
DataSet OpenThread::mCurrentDataset;
otNetworkKey OpenThread::mNetworkKey;

OpenThread::OpenThread() {
  // static initialization (node data and stack starting information)
  otStarted = false;
  mCurrentDataset.clear();                       // Initialize the current dataset
  memset(&mNetworkKey, 0, sizeof(mNetworkKey));  // Initialize the network key
  mInstance = nullptr;
}

OpenThread::~OpenThread() {
  end();
}

OpenThread::operator bool() const {
  return otStarted;
}

void OpenThread::begin(bool OThreadAutoStart) {
  if (otStarted) {
    log_w("OpenThread already started");
    return;
  }

  memset(&ot_native_config, 0, sizeof(esp_openthread_platform_config_t));
  ot_native_config.radio_config.radio_mode = RADIO_MODE_NATIVE;
  ot_native_config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
  ot_native_config.port_config.storage_partition_name = "nvs";
  ot_native_config.port_config.netif_queue_size = 10;
  ot_native_config.port_config.task_queue_size = 10;

  // Initialize OpenThread stack
  xTaskCreate(ot_task_worker, "ot_main_loop", 10240, NULL, 20, &s_ot_task);
  if (s_ot_task == NULL) {
    log_e("Error: Failed to create OpenThread task");
    return;
  }
  log_d("OpenThread task created successfully");

  // starts Thread with default dataset from NVS or from IDF default settings
  if (OThreadAutoStart) {
    otOperationalDatasetTlvs dataset;
    otError error = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
    //    error = OT_ERROR_FAILED; // teste para forçar NULL dataset
    if (error != OT_ERROR_NONE) {
      log_i("Failed to get active NVS dataset from OpenThread");
    } else {
      log_i("Got active NVS dataset from OpenThread");
    }
    esp_err_t err = esp_openthread_auto_start((error == OT_ERROR_NONE) ? &dataset : NULL);
    if (err != ESP_OK) {
      log_i("Failed to AUTO start OpenThread");
    } else {
      log_i("AUTO start OpenThread done");
    }
  }

  // get the OpenThread instance that will be used for all operations
  mInstance = esp_openthread_get_instance();
  if (!mInstance) {
    log_e("Error: Failed to initialize OpenThread instance");
    end();
    return;
  }

  otStarted = true;
}

void OpenThread::end() {
  if (!otStarted) {
    log_w("OpenThread already stopped");
    return;
  }

  if (s_ot_task != NULL) {
    vTaskDelete(s_ot_task);
    s_ot_task = NULL;
  }

  // Clean up in reverse order of initialization
  if (openthread_netif != NULL) {
    esp_netif_destroy(openthread_netif);
    openthread_netif = NULL;
  }

  esp_openthread_netif_glue_deinit();
  esp_openthread_deinit();
  esp_vfs_eventfd_unregister();

#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
  ot_lwip_netif = NULL;
#endif

  mInstance = nullptr;
  otStarted = false;
  log_d("OpenThread ended successfully");
}

void OpenThread::start() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }
  clearAllAddressCache();  // Clear cache when starting network
  otThreadSetEnabled(mInstance, true);
  log_d("Thread network started");
}

void OpenThread::stop() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }
  clearAllAddressCache();  // Clear cache when stopping network
  otThreadSetEnabled(mInstance, false);
  log_d("Thread network stopped");
}

void OpenThread::networkInterfaceUp() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }
  // Enable the Thread interface (equivalent to CLI Command "ifconfig up")
  otError error = otIp6SetEnabled(mInstance, true);
  if (error != OT_ERROR_NONE) {
    log_e("Error: Failed to enable Thread interface (error code: %d)\n", error);
  }
  clearAllAddressCache();  // Clear cache when interface comes up
  log_d("OpenThread Network Interface is up");
}

void OpenThread::networkInterfaceDown() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }
  // Disable the Thread interface (equivalent to CLI Command "ifconfig down")
  otError error = otIp6SetEnabled(mInstance, false);
  if (error != OT_ERROR_NONE) {
    log_e("Error: Failed to disable Thread interface (error code: %d)\n", error);
  }
  log_d("OpenThread Network Interface is down");
}

void OpenThread::commitDataSet(const DataSet &dataset) {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }
  // Commit the dataset as the active dataset
  otError error = otDatasetSetActive(mInstance, &(dataset.getDataset()));
  if (error != OT_ERROR_NONE) {
    log_e("Error: Failed to commit dataset (error code: %d)\n", error);
    return;
  }
  clearAllAddressCache();  // Clear cache when dataset changes
  log_d("Dataset committed successfully");
}

ot_device_role_t OpenThread::otGetDeviceRole() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return OT_ROLE_DISABLED;
  }
  return (ot_device_role_t)otThreadGetDeviceRole(mInstance);
}

const char *OpenThread::otGetStringDeviceRole() {
  return otRoleString[otGetDeviceRole()];
}

void OpenThread::otPrintNetworkInformation(Stream &output) {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }

  output.printf("Role: %s", otGetStringDeviceRole());
  output.println();
  output.printf("RLOC16: 0x%04x", otThreadGetRloc16(mInstance));  // RLOC16
  output.println();
  output.printf("Network Name: %s", otThreadGetNetworkName(mInstance));
  output.println();
  output.printf("Channel: %d", otLinkGetChannel(mInstance));
  output.println();
  output.printf("PAN ID: 0x%04x", otLinkGetPanId(mInstance));
  output.println();

  const otExtendedPanId *extPanId = otThreadGetExtendedPanId(mInstance);
  output.print("Extended PAN ID: ");
  for (int i = 0; i < OT_EXT_PAN_ID_SIZE; i++) {
    output.printf("%02x", extPanId->m8[i]);
  }
  output.println();

  otNetworkKey networkKey;
  otThreadGetNetworkKey(mInstance, &networkKey);
  output.print("Network Key: ");
  for (int i = 0; i < OT_NETWORK_KEY_SIZE; i++) {
    output.printf("%02x", networkKey.m8[i]);
  }
  output.println();
}

// Get the Node Network Name
String OpenThread::getNetworkName() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return String();  // Return empty String, not nullptr
  }
  const char *networkName = otThreadGetNetworkName(mInstance);
  return networkName ? String(networkName) : String();
}

// Get the Node Extended PAN ID
const uint8_t *OpenThread::getExtendedPanId() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return nullptr;
  }
  const otExtendedPanId *extPanId = otThreadGetExtendedPanId(mInstance);
  return extPanId ? extPanId->m8 : nullptr;
}

// Get the Node Network Key
const uint8_t *OpenThread::getNetworkKey() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return nullptr;
  }
  otThreadGetNetworkKey(mInstance, &mNetworkKey);
  return mNetworkKey.m8;
}

// Get the Node Channel
uint8_t OpenThread::getChannel() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return 0;
  }
  return otLinkGetChannel(mInstance);
}

// Get the Node PAN ID
uint16_t OpenThread::getPanId() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return 0;
  }
  return otLinkGetPanId(mInstance);
}

// Get the OpenThread instance
otInstance *OpenThread::getInstance() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return nullptr;
  }
  return mInstance;
}

// Get the current dataset
const DataSet &OpenThread::getCurrentDataSet() const {

  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    mCurrentDataset.clear();
    return mCurrentDataset;
  }

  otOperationalDataset dataset;
  otError error = otDatasetGetActive(mInstance, &dataset);

  if (error == OT_ERROR_NONE) {
    mCurrentDataset.clear();

    if (dataset.mComponents.mIsNetworkNamePresent) {
      mCurrentDataset.setNetworkName(dataset.mNetworkName.m8);
    }
    if (dataset.mComponents.mIsExtendedPanIdPresent) {
      mCurrentDataset.setExtendedPanId(dataset.mExtendedPanId.m8);
    }
    if (dataset.mComponents.mIsNetworkKeyPresent) {
      mCurrentDataset.setNetworkKey(dataset.mNetworkKey.m8);
    }
    if (dataset.mComponents.mIsChannelPresent) {
      mCurrentDataset.setChannel(dataset.mChannel);
    }
    if (dataset.mComponents.mIsPanIdPresent) {
      mCurrentDataset.setPanId(dataset.mPanId);
    }
  } else {
    log_w("Failed to get active dataset (error: %d)", error);
    mCurrentDataset.clear();
  }

  return mCurrentDataset;
}

// Get the Mesh Local Prefix
const otMeshLocalPrefix *OpenThread::getMeshLocalPrefix() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return nullptr;
  }
  return otThreadGetMeshLocalPrefix(mInstance);
}

// Get the Mesh-Local EID
IPAddress OpenThread::getMeshLocalEid() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return IPAddress(IPv6);  // Return empty IPv6 address
  }
  const otIp6Address *otAddr = otThreadGetMeshLocalEid(mInstance);
  if (!otAddr) {
    log_w("Failed to get Mesh Local EID");
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, otAddr->mFields.m8);
}

// Get the Thread Leader RLOC
IPAddress OpenThread::getLeaderRloc() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return IPAddress(IPv6);  // Return empty IPv6 address
  }
  otIp6Address otAddr;
  otError error = otThreadGetLeaderRloc(mInstance, &otAddr);
  if (error != OT_ERROR_NONE) {
    log_w("Failed to get Leader RLOC");
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, otAddr.mFields.m8);
}

// Get the Node RLOC
IPAddress OpenThread::getRloc() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return IPAddress(IPv6);  // Return empty IPv6 address
  }
  const otIp6Address *otAddr = otThreadGetRloc(mInstance);
  if (!otAddr) {
    log_w("Failed to get Node RLOC");
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, otAddr->mFields.m8);
}

// Get the RLOC16 ID
uint16_t OpenThread::getRloc16() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return 0;
  }
  return otThreadGetRloc16(mInstance);
}

// Populate unicast address cache from OpenThread
void OpenThread::populateUnicastAddressCache() const {
  if (!mInstance) {
    return;
  }

  // Clear existing cache
  mCachedUnicastAddresses.clear();

  // Populate unicast addresses cache
  const otNetifAddress *addr = otIp6GetUnicastAddresses(mInstance);
  while (addr != nullptr) {
    mCachedUnicastAddresses.push_back(IPAddress(IPv6, addr->mAddress.mFields.m8));
    addr = addr->mNext;
  }

  log_d("Populated unicast address cache with %zu addresses", mCachedUnicastAddresses.size());
}

// Populate multicast address cache from OpenThread
void OpenThread::populateMulticastAddressCache() const {
  if (!mInstance) {
    return;
  }

  // Clear existing cache
  mCachedMulticastAddresses.clear();

  // Populate multicast addresses cache
  const otNetifMulticastAddress *mAddr = otIp6GetMulticastAddresses(mInstance);
  while (mAddr != nullptr) {
    mCachedMulticastAddresses.push_back(IPAddress(IPv6, mAddr->mAddress.mFields.m8));
    mAddr = mAddr->mNext;
  }

  log_d("Populated multicast address cache with %zu addresses", mCachedMulticastAddresses.size());
}

// Clear unicast address cache
void OpenThread::clearUnicastAddressCache() const {
  mCachedUnicastAddresses.clear();
  log_d("Cleared unicast address cache");
}

// Clear multicast address cache
void OpenThread::clearMulticastAddressCache() const {
  mCachedMulticastAddresses.clear();
  log_d("Cleared multicast address cache");
}

// Clear all address caches
void OpenThread::clearAllAddressCache() const {
  mCachedUnicastAddresses.clear();
  mCachedMulticastAddresses.clear();
  log_d("Cleared all address caches");
}

// Get count of unicast addresses
size_t OpenThread::getUnicastAddressCount() const {
  // Populate cache if empty
  if (mCachedUnicastAddresses.empty()) {
    populateUnicastAddressCache();
  }

  return mCachedUnicastAddresses.size();
}

// Get unicast address by index
IPAddress OpenThread::getUnicastAddress(size_t index) const {
  // Populate cache if empty
  if (mCachedUnicastAddresses.empty()) {
    populateUnicastAddressCache();
  }

  if (index >= mCachedUnicastAddresses.size()) {
    log_w("Unicast address index %zu out of range (max: %zu)", index, mCachedUnicastAddresses.size());
    return IPAddress(IPv6);
  }

  return mCachedUnicastAddresses[index];
}

// Get all unicast addresses
std::vector<IPAddress> OpenThread::getAllUnicastAddresses() const {
  // Populate cache if empty
  if (mCachedUnicastAddresses.empty()) {
    populateUnicastAddressCache();
  }

  return mCachedUnicastAddresses;  // Return copy of cached vector
}

// Get count of multicast addresses
size_t OpenThread::getMulticastAddressCount() const {
  // Populate cache if empty
  if (mCachedMulticastAddresses.empty()) {
    populateMulticastAddressCache();
  }

  return mCachedMulticastAddresses.size();
}

// Get multicast address by index
IPAddress OpenThread::getMulticastAddress(size_t index) const {
  // Populate cache if empty
  if (mCachedMulticastAddresses.empty()) {
    populateMulticastAddressCache();
  }

  if (index >= mCachedMulticastAddresses.size()) {
    log_w("Multicast address index %zu out of range (max: %zu)", index, mCachedMulticastAddresses.size());
    return IPAddress(IPv6);
  }

  return mCachedMulticastAddresses[index];
}

// Get all multicast addresses
std::vector<IPAddress> OpenThread::getAllMulticastAddresses() const {
  // Populate cache if empty
  if (mCachedMulticastAddresses.empty()) {
    populateMulticastAddressCache();
  }

  return mCachedMulticastAddresses;  // Return copy of cached vector
}

OpenThread OThread;

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
