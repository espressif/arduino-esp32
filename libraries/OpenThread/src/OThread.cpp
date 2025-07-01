#include "OThread.h"
#if SOC_IEEE802154_SUPPORTED
#if CONFIG_OPENTHREAD_ENABLED

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
  strncpy(mDataset.mNetworkName.m8, name, sizeof(mDataset.mNetworkName.m8));
  mDataset.mComponents.mIsNetworkNamePresent = true;
}

void DataSet::setExtendedPanId(const uint8_t *extPanId) {
  memcpy(mDataset.mExtendedPanId.m8, extPanId, OT_EXT_PAN_ID_SIZE);
  mDataset.mComponents.mIsExtendedPanIdPresent = true;
}

void DataSet::setNetworkKey(const uint8_t *key) {
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
bool OpenThread::otStarted = false;

otInstance *OpenThread::mInstance = nullptr;
OpenThread::OpenThread() {}

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
  // get the OpenThread instance that will be used for all operations
  mInstance = esp_openthread_get_instance();
  if (!mInstance) {
    log_e("Error: Failed to initialize OpenThread instance");
    end();
    return;
  }
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
  otStarted = true;
}

void OpenThread::end() {
  if (s_ot_task != NULL) {
    vTaskDelete(s_ot_task);
    s_ot_task = NULL;
    // Clean up
    esp_openthread_deinit();
    esp_openthread_netif_glue_deinit();
#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
    ot_lwip_netif = NULL;
#endif
    esp_netif_destroy(openthread_netif);
    esp_vfs_eventfd_unregister();
  }
  otStarted = false;
}

void OpenThread::start() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }
  otThreadSetEnabled(mInstance, true);
  log_d("Thread network started");
}

void OpenThread::stop() {
  if (!mInstance) {
    log_w("Error: OpenThread instance not initialized");
    return;
  }
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

OpenThread OThread;

#endif /* CONFIG_OPENTHREAD_ENABLED */
#endif /* SOC_IEEE802154_SUPPORTED */
