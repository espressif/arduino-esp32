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

#include <openthread/instance.h>

#include "esp_openthread_lock.h"

static esp_openthread_platform_config_t ot_native_config;
static esp_netif_t *openthread_netif = NULL;

// RAII helper for the OpenThread stack lock. The mainloop runs on the worker
// task (ot_task_worker), so any otXxx()/esp_openthread_* call made from another
// task races that mainloop and must be serialized with this lock. Acquire on
// construction, release on destruction; evaluate as bool to check success.
//
// Policy for this file: every OpenThread/DataSet method that calls into the
// stack from the caller's task (all the start/stop/interface/dataset calls and
// every getter below) takes an OtLock for the duration of the ot* call. The
// underlying mutex is recursive, so methods that call one another (or call into
// helpers that also lock) are safe on the same task and will not self-deadlock.
// Do NOT hold an OtLock across a call to end(), which deinitializes the stack
// and destroys this very mutex.
struct OtLock {
  bool mLocked;
  OtLock() : mLocked(esp_openthread_lock_acquire(portMAX_DELAY)) {}
  ~OtLock() {
    if (mLocked) {
      esp_openthread_lock_release();
    }
  }
  explicit operator bool() const {
    return mLocked;
  }
  OtLock(const OtLock &) = delete;
  OtLock &operator=(const OtLock &) = delete;
};

const char *otRoleString[] = {
  "Disabled",  ///< The Thread stack is disabled.
  "Detached",  ///< Not currently participating in a Thread network/partition.
  "Child",     ///< The Thread Child role.
  "Router",    ///< The Thread Router role.
  "Leader",    ///< The Thread Leader role.
  "Unknown",   ///< Unknown role, not initialized or not started.
};

static TaskHandle_t s_ot_task = NULL;
// Signaled by ot_task_worker() once the stack finished (or failed) initializing.
// Mirrors the s_ot_syn_semaphore handshake done by IDF's esp_openthread_start().
static SemaphoreHandle_t s_ot_init_done = NULL;
// Result of that initialization, written by ot_task_worker() BEFORE it gives
// s_ot_init_done. The semaphore only says "init finished"; this says whether it
// succeeded, so begin() can abort if a later init step failed (in which case the
// worker never launches the mainloop and just suspends itself).
static volatile bool s_ot_init_ok = false;
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
  // Publish the init result, then tell begin() that initialization is finished
  // (whether it succeeded or not) BEFORE entering the mainloop, which never
  // returns. This matches IDF's esp_openthread_start(), where the worker task
  // releases the sync semaphore only after esp_openthread_init() + netif setup
  // are complete, so the caller can safely touch the instance/dataset (NVS
  // already loaded) afterwards. s_ot_init_ok MUST be set before the give so
  // begin() reads a stable value once xSemaphoreTake() returns.
  s_ot_init_ok = !err;
  if (s_ot_init_done != NULL) {
    xSemaphoreGive(s_ot_init_done);
  }
  if (!err) {
    // only returns in case there is an OpenThread Stack failure...
    esp_openthread_launch_mainloop();
  }

  // We only get here on an init failure, or if the mainloop returned because of
  // a stack failure. Do NOT tear down the netif/glue/eventfd or self-delete from
  // this task: teardown and deletion of this task handle are owned exclusively
  // by end() (which begin() also invokes when a start fails). Cleaning up here
  // would race end() and leave openthread_netif / s_ot_task pointing at freed
  // resources, causing a double esp_netif_destroy() or a vTaskDelete() on an
  // already-deleted task. Park instead and let end() release everything and
  // delete this (still valid) task handle.
  for (;;) {
    vTaskSuspend(NULL);
  }
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock for dataset init");
    return;
  }
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock for dataset apply");
    return;
  }
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

  // Handshake semaphore so begin() can block until the worker task has finished
  // initializing the stack (mirrors IDF's esp_openthread_start()).
  if (s_ot_init_done == NULL) {
    s_ot_init_done = xSemaphoreCreateBinary();
    if (s_ot_init_done == NULL) {
      log_e("Error: Failed to create OpenThread init semaphore");
      return;
    }
  }

  // Initialize OpenThread stack
  xTaskCreate(ot_task_worker, "ot_main_loop", 10240, NULL, 20, &s_ot_task);
  if (s_ot_task == NULL) {
    log_e("Error: Failed to create OpenThread task");
    // otStarted is still false, so end() will not run to clean this up. Delete
    // the handshake semaphore here so its handle is not leaked and a later
    // begin() starts from a clean state.
    vSemaphoreDelete(s_ot_init_done);
    s_ot_init_done = NULL;
    return;
  }
  log_d("OpenThread task created successfully");

  // Wait for the worker task to finish initializing the stack before touching
  // the instance or any otXxx() API. We must NOT hold the OpenThread lock here:
  // esp_openthread_init() acquires that same lock on the worker task, so holding
  // it would deadlock. This is exactly why the IDF uses a separate semaphore for
  // the init handshake and the mutex only for protecting API calls.
  if (xSemaphoreTake(s_ot_init_done, pdMS_TO_TICKS(5000)) != pdTRUE) {
    log_e("Error: Timed out waiting for OpenThread stack initialization");
    otStarted = true;  // force end() to run the full teardown below
    end();
    return;
  }

  // The handshake only tells us init finished, not whether it succeeded. If a
  // later init step failed (e.g. netif attach/set-default), the worker never
  // launched the mainloop and is now suspended, so we must NOT proceed even
  // though the instance may look initialized. Tear down instead.
  if (!s_ot_init_ok) {
    log_e("Error: OpenThread stack initialization failed in the worker task");
    otStarted = true;  // force end() to run the full teardown below
    end();
    return;
  }

  // get the OpenThread instance that will be used for all operations. After the
  // handshake above the stack is fully up and the persisted dataset (if any) has
  // already been loaded from NVS, so this check is now reliable.
  mInstance = esp_openthread_get_instance();
  if (mInstance == nullptr || !otInstanceIsInitialized(mInstance)) {
    log_e("Error: Failed to initialize OpenThread instance");
    otStarted = true;  // force end() to run the full teardown below
    end();
    return;
  }

  // starts Thread with default dataset from NVS or from IDF default settings
  if (OThreadAutoStart) {
    // The mainloop is already running on the worker task, so every otXxx() call
    // from this (non-OT) task must be made while holding the stack lock.
    OtLock lock;
    if (!lock) {
      log_e("Failed to acquire OpenThread lock for auto start");
    } else {
      otOperationalDatasetTlvs dataset;
      otError error = otDatasetGetActiveTlvs(mInstance, &dataset);
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

  // Delete the init-handshake semaphore only AFTER the worker task is gone, so
  // it can no longer xSemaphoreGive() a freed handle. Setting it back to NULL
  // lets a subsequent begin() recreate it cleanly.
  if (s_ot_init_done != NULL) {
    vSemaphoreDelete(s_ot_init_done);
    s_ot_init_done = NULL;
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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

bool OpenThread::hasActiveDataset() const {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return false;
  }

  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
    return false;
  }

  otOperationalDatasetTlvs datasetTlvs;
  return otDatasetGetActiveTlvs(mInstance, &datasetTlvs) == OT_ERROR_NONE;
}

ot_device_role_t OpenThread::otGetDeviceRole() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return OT_ROLE_DISABLED;
  }
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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

  // Snapshot all OpenThread state under the lock, then release it before doing
  // any Stream output. Stream writes can block on backpressure (e.g. Serial),
  // and holding the stack lock across them would stall the OpenThread mainloop.
  otDeviceRole role = OT_DEVICE_ROLE_DISABLED;
  uint16_t rloc16 = 0;
  char networkName[OT_NETWORK_NAME_MAX_SIZE + 1] = {0};
  uint8_t channel = 0;
  uint16_t panId = 0;
  otExtendedPanId extPanId = {0};
  otNetworkKey networkKey = {0};
  {
    OtLock lock;
    if (!lock) {
      log_e("Error: Failed to acquire OpenThread lock");
      return;
    }
    role = otThreadGetDeviceRole(mInstance);
    rloc16 = otThreadGetRloc16(mInstance);
    const char *name = otThreadGetNetworkName(mInstance);
    if (name) {
      strncpy(networkName, name, sizeof(networkName) - 1);
    }
    channel = otLinkGetChannel(mInstance);
    panId = otLinkGetPanId(mInstance);
    const otExtendedPanId *extPanIdPtr = otThreadGetExtendedPanId(mInstance);
    if (extPanIdPtr) {
      extPanId = *extPanIdPtr;
    }
    otThreadGetNetworkKey(mInstance, &networkKey);
  }

  output.printf("Role: %s", (role <= OT_DEVICE_ROLE_LEADER) ? otRoleString[role] : otRoleString[5]);
  output.println();
  output.printf("RLOC16: 0x%04x", rloc16);
  output.println();
  output.printf("Network Name: %s", networkName);
  output.println();
  output.printf("Channel: %u", channel);
  output.println();
  output.printf("PAN ID: 0x%04x", panId);
  output.println();

  output.print("Extended PAN ID: ");
  for (int i = 0; i < OT_EXT_PAN_ID_SIZE; i++) {
    output.printf("%02x", extPanId.m8[i]);
  }
  output.println();

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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
    return String();
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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

  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
    return IPAddress(IPv6);
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
    return IPAddress(IPv6);
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
    return IPAddress(IPv6);
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
  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
    return 0;
  }
  return otThreadGetRloc16(mInstance);
}

// Populate unicast address cache from OpenThread
void OpenThread::populateUnicastAddressCache() const {
  if (!mInstance) {
    return;
  }

  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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

  log_d("Populated unicast address cache with %lu addresses", (unsigned long)mCachedUnicastAddresses.size());
}

// Populate multicast address cache from OpenThread
void OpenThread::populateMulticastAddressCache() const {
  if (!mInstance) {
    return;
  }

  OtLock lock;
  if (!lock) {
    log_e("Error: Failed to acquire OpenThread lock");
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

  log_d("Populated multicast address cache with %lu addresses", (unsigned long)mCachedMulticastAddresses.size());
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
    log_w("Unicast address index %lu out of range (max: %lu)", (unsigned long)index, (unsigned long)mCachedUnicastAddresses.size());
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
    log_w("Multicast address index %lu out of range (max: %lu)", (unsigned long)index, (unsigned long)mCachedMulticastAddresses.size());
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
