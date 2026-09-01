// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <app/server/Server.h>
#if CONFIG_ENABLE_MATTER_OVER_THREAD
#include "esp_openthread_types.h"
#include "platform/ESP32/OpenthreadLauncher.h"
#if defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
#include <app/clusters/general-commissioning-server/BreadCrumbTracker.h>
#include <app/clusters/network-commissioning/NetworkCommissioningCluster.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h>
#endif
#endif

// This prevents initArduino() from releasing BLE memory before the
// Matter stack can use Bluetooth transport.
#if CONFIG_ENABLE_CHIPOBLE
#include "esp32-hal-alloc-ble-mem.h"
#include "esp32-hal-bt.h"
#include <platform/internal/BLEManager.h>
#endif

#if defined(SOC_WIFI_SUPPORTED) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
#include <esp_wifi.h>
#endif

#include <app/server/Dnssd.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <stdio.h>
#include <string.h>
#include <system/SystemLayer.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace esp_matter::identification;
using namespace chip::app::Clusters;

constexpr auto k_timeout_seconds = 300;

// Two-phase lifecycle. Endpoint begin() creates the node (NodeCreated).
// Matter.begin() starts the CHIP stack (StackStarted). These are not the same:
// identity setters are valid after the node exists and invalid after the stack starts.
enum class MatterLifecycle : uint8_t {
  Uninitialized,
  NodeCreated,
  StackStarted
};
static MatterLifecycle sLifecycle = MatterLifecycle::Uninitialized;
static node::config_t node_config;
static node_t *deviceNode = nullptr;
ArduinoMatter::matterEventCB ArduinoMatter::_matterEventCB = nullptr;
ArduinoMatter::bleMemoryReleasedCB ArduinoMatter::_bleMemoryReleasedCB = nullptr;
#if CONFIG_ENABLE_CHIPOBLE
static bool sBleCommissioningEnabled = true;
#else
static bool sBleCommissioningEnabled = false;
#endif
static bool sBleMemoryReleaseEnabled = true;
static matterNetwork_t sSelectedNetwork = MATTER_NETWORK_NONE;
static esp_event_handler_instance_t sIpEventHandler = nullptr;

extern bool matterWifiStackWrapRan();

static bool netifHasUsableIpv6(const char *ifkey) {
  if (ifkey == nullptr) {
    return false;
  }
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey(ifkey);
  if (netif == nullptr || !esp_netif_is_netif_up(netif)) {
    return false;
  }
  esp_ip6_addr_t addr;
  if (esp_netif_get_ip6_linklocal(netif, &addr) == ESP_OK) {
    return true;
  }
  if (esp_netif_get_ip6_global(netif, &addr) == ESP_OK) {
    return true;
  }
  return false;
}

static bool ethernetHasUsableIpv6() {
  if (netifHasUsableIpv6("ETH_DEF")) {
    return true;
  }
  char key[16];
  for (int i = 1; i <= 8; i++) {
    snprintf(key, sizeof(key), "ETH_%d", i);
    if (netifHasUsableIpv6(key)) {
      return true;
    }
  }
  return false;
}

static bool selectedHasUsableIpv6() {
  switch (sSelectedNetwork) {
    case MATTER_NETWORK_WIFI:     return netifHasUsableIpv6("WIFI_STA_DEF");
    case MATTER_NETWORK_THREAD:   return netifHasUsableIpv6("OT_DEF");
    case MATTER_NETWORK_ETHERNET: return ethernetHasUsableIpv6();
    default:
      return netifHasUsableIpv6("WIFI_STA_DEF") || netifHasUsableIpv6("OT_DEF") || ethernetHasUsableIpv6();
  }
}

static void startDnssdWork(intptr_t) {
  chip::app::DnssdServer::Instance().StartServer();
}

static void scheduleDnssdRestart() {
  if (sLifecycle != MatterLifecycle::StackStarted) {
    return;
  }
  chip::DeviceLayer::PlatformMgr().ScheduleWork(startDnssdWork, 0);
}

#if CONFIG_ENABLE_MATTER_OVER_THREAD
// Thread commissionable ads are OpenThread SRP (_matterc._udp), proxied as LAN
// mDNS by the border router. On ESP32-C6, ChipDnssdInit also runs Wi-Fi mDNS
// and posts kDnssdInitialized immediately — that is not SRP. StartServer()
// before the OTBR answers ClearSrpHost returns CHIP_ERROR_UNINITIALIZED and
// never publishes. After attach, retry for ~24 s so a late SRP server still
// gets advertised. Safe from the ESP-IDF IP task: work runs on the CHIP task.
static constexpr uint8_t kThreadAdvertiseMaxRetries = 8;
static uint8_t sThreadAdvertiseRetriesLeft = 0;
static bool sThreadAdvertiseTimerArmed = false;

static void threadAdvertiseRetryTimer(chip::System::Layer *, void *) {
  sThreadAdvertiseTimerArmed = false;
  if (sLifecycle != MatterLifecycle::StackStarted || sSelectedNetwork != MATTER_NETWORK_THREAD) {
    return;
  }
  chip::app::DnssdServer::Instance().StartServer();
  if (sThreadAdvertiseRetriesLeft == 0) {
    return;
  }
  sThreadAdvertiseRetriesLeft--;
  sThreadAdvertiseTimerArmed = true;
  chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(3), threadAdvertiseRetryTimer, nullptr);
}

static void startThreadAdvertiseRetriesWork(intptr_t) {
  if (sLifecycle != MatterLifecycle::StackStarted || sSelectedNetwork != MATTER_NETWORK_THREAD) {
    return;
  }
  chip::app::DnssdServer::Instance().StartServer();
  sThreadAdvertiseRetriesLeft = kThreadAdvertiseMaxRetries;
  if (!sThreadAdvertiseTimerArmed) {
    sThreadAdvertiseTimerArmed = true;
    chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(3), threadAdvertiseRetryTimer, nullptr);
  }
}

static void startThreadAdvertiseRetries() {
  if (sLifecycle != MatterLifecycle::StackStarted) {
    return;
  }
  chip::DeviceLayer::PlatformMgr().ScheduleWork(startThreadAdvertiseRetriesWork, 0);
}
#endif

static bool ifkeyMatchesSelection(esp_netif_t *netif) {
  if (netif == nullptr) {
    return false;
  }
  const char *key = esp_netif_get_ifkey(netif);
  if (key == nullptr) {
    return false;
  }
  switch (sSelectedNetwork) {
    case MATTER_NETWORK_WIFI:     return strcmp(key, "WIFI_STA_DEF") == 0;
    case MATTER_NETWORK_THREAD:   return strcmp(key, "OT_DEF") == 0;
    case MATTER_NETWORK_ETHERNET: return strncmp(key, "ETH_", 4) == 0;
    default:                      return true;
  }
}

static void matterIpEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  (void)event_base;
  if (event_id != IP_EVENT_GOT_IP6 || event_data == nullptr) {
    return;
  }
  const ip_event_got_ip6_t *event = static_cast<const ip_event_got_ip6_t *>(event_data);
  if (!ifkeyMatchesSelection(event->esp_netif)) {
    return;
  }
#if CONFIG_ENABLE_MATTER_OVER_THREAD
  if (sSelectedNetwork == MATTER_NETWORK_THREAD) {
    startThreadAdvertiseRetries();
    return;
  }
#endif
  scheduleDnssdRestart();
}

#if CONFIG_ENABLE_MATTER_OVER_THREAD && defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
// The C6 prebuild binds Wi-Fi NC to endpoint 0 and Thread NC to endpoint 2.
// Hubs that only talk to the root (Alexa) never send a Thread dataset.
// After start(), replace the Wi-Fi driver on endpoint 0 with the Thread driver.
class MatterBreadcrumbTracker : public chip::app::Clusters::BreadCrumbTracker {
  void SetBreadCrumb(uint64_t) override {}
};

static MatterBreadcrumbTracker sThreadNcBreadcrumb;
static chip::DeviceLayer::NetworkCommissioning::GenericThreadDriver sThreadNcDriver;
static chip::app::LazyRegisteredServerCluster<chip::app::Clusters::NetworkCommissioningCluster> sThreadNcOnRoot;

static void bindThreadNetworkCommissioningOnRoot() {
  if (sSelectedNetwork != MATTER_NETWORK_THREAD) {
    return;
  }
  lock::ScopedChipStackLock stackLock(portMAX_DELAY);
  auto &registry = esp_matter::data_model::provider::get_instance().registry();
  const chip::app::ConcreteClusterPath path(0, NetworkCommissioning::Id);
  chip::app::ServerClusterInterface *existing = registry.Get(path);
  if (existing != nullptr) {
    (void)registry.Unregister(existing);
    static_cast<chip::app::Clusters::NetworkCommissioningCluster *>(existing)->Deinit();
  }
  if (!sThreadNcOnRoot.IsConstructed()) {
    sThreadNcOnRoot.Create(0, &sThreadNcDriver, sThreadNcBreadcrumb);
  }
  CHIP_ERROR err = sThreadNcOnRoot.Cluster().Init();
  if (err != CHIP_NO_ERROR) {
    log_e("Thread Network Commissioning Init on endpoint 0 failed: %" CHIP_ERROR_FORMAT, err.Format());
    return;
  }
  err = registry.Register(sThreadNcOnRoot.Registration());
  if (err != CHIP_NO_ERROR) {
    log_e("Failed to register Thread Network Commissioning on endpoint 0: %" CHIP_ERROR_FORMAT, err.Format());
    return;
  }
  log_i("Thread Network Commissioning is on endpoint 0 (replaced Wi-Fi)");
}

static void hideRootWiFiDiagnostics() {
  endpoint_t *root = endpoint::get(node::get(), 0);
  if (root == nullptr) {
    return;
  }
  cluster_t *wifiDiag = cluster::get(root, WiFiNetworkDiagnostics::Id);
  if (wifiDiag != nullptr) {
    cluster::destroy(wifiDiag);
  }
}
#endif

static void registerMatterIpEventHandler() {
  if (sIpEventHandler != nullptr) {
    return;
  }
  const esp_err_t err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, matterIpEventHandler, nullptr, &sIpEventHandler);
  if (err != ESP_OK) {
    log_e("Failed to register IP event handler for Matter DNS-SD: %s", esp_err_to_name(err));
  }
}

// Reports the reclaim to the sketch exactly once, whether Arduino or CHIP did it.
static bool sBleMemoryReleasedNotified = false;

static void notifyBleMemoryReleased() {
  if (sBleMemoryReleasedNotified) {
    return;
  }
  sBleMemoryReleasedNotified = true;
  if (ArduinoMatter::_bleMemoryReleasedCB != nullptr) {
    ArduinoMatter::_bleMemoryReleasedCB();
  }
}

#if CONFIG_ENABLE_CHIPOBLE
// esp_bt_mem_release() corrupts the heap while the NimBLE host task is still running,
// so poll for its exit the way CHIP does. Bounded, so a host that never exits cannot
// leave a timer rearming forever.
static constexpr uint8_t kClaimBleMemoryMaxRetries = 15;  // 15 x 2 s
static uint8_t sClaimBleMemoryRetries = 0;

static void claimBleMemoryTimer(chip::System::Layer *, void *);

// BLEMgr().Shutdown() deinits the CHIPoBLE host but does not return the BLE RAM to the
// heap. That step is BLEManagerImpl::ClaimBLEMemory(), which CHIP compiles in only when
// CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING is set, and whose target list omits ESP32-C5 even
// then (still true in esp-matter 1.6 and CHIP master). Every prebuilt Arduino library is
// built with that option off, so CHIP neither reclaims nor posts kBLEDeinitialized.
// Finish the job here. btMemRelease() only acts on regions that are still reserved, so
// builds where CHIP does reclaim are unaffected.
static void claimBleMemory() {
  if (xTaskGetHandle("nimble_host") != nullptr) {
    if (++sClaimBleMemoryRetries > kClaimBleMemoryMaxRetries) {
      log_e("NimBLE host is still running; BLE memory was not reclaimed.");
      return;
    }
    chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds32(2), claimBleMemoryTimer, nullptr);
    return;
  }
#ifdef CONFIG_IDF_TARGET_ESP32
  const bt_mode releaseMode = BT_MODE_BTDM;  // NimBLE reserves both regions on the original ESP32
#else
  const bt_mode releaseMode = BT_MODE_BLE;
#endif
  if (!btMemReleased(releaseMode) && !btMemRelease(releaseMode)) {
    log_e("Failed to release BLE memory; it stays reserved.");
    return;
  }
  log_d("BLE memory reclaimed");
  // The RAM is on the heap now, so report it regardless of what the event queue does.
  notifyBleMemoryReleased();
  // CHIP posts kBLEDeinitialized only from its own ClaimBLEMemory(), which this build
  // does not compile in. Post it here so onEvent(MATTER_BLE_DEINITIALIZED) stays in
  // step with the callback above. notifyBleMemoryReleased() is one-shot, so neither
  // this dispatch nor a build where CHIP also posts reports a second time.
  chip::DeviceLayer::ChipDeviceEvent event = {};
  event.Type = chip::DeviceLayer::DeviceEventType::kBLEDeinitialized;
  if (chip::DeviceLayer::PlatformMgr().PostEvent(&event) != CHIP_NO_ERROR) {
    log_e("Failed to post BLE deinit event; onEvent(MATTER_BLE_DEINITIALIZED) will not fire.");
  }
}

// CHIP timers require the System::TimerCompleteCallback signature.
static void claimBleMemoryTimer(chip::System::Layer *, void *) {
  claimBleMemory();
}

// Runs on the CHIP task: _Shutdown() must not run from setup().
static void shutdownChipBleWork(intptr_t) {
  chip::DeviceLayer::Internal::BLEMgr().Shutdown();
  // Shutdown() only schedules the state machine that deinits the host, so arm the
  // reclaim poll rather than releasing here.
  sClaimBleMemoryRetries = 0;
  claimBleMemory();
}
#endif

static void scheduleShutdownChipBle() {
#if CONFIG_ENABLE_CHIPOBLE
  chip::DeviceLayer::PlatformMgr().ScheduleWork(shutdownChipBleWork, 0);
#endif
}

#if defined(SOC_WIFI_SUPPORTED) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
// Matter exchanges little data with a controller. Init Wi-Fi here with smaller
// buffers so CHIP's InitWiFiStack() (WIFI_INIT_CONFIG_DEFAULT from sdkconfig)
// becomes a no-op: the first esp_wifi_init() owns the counts.
// IDF requires rx_ba_win <= dynamic_rx_buf_num and <= 2 * static_rx_buf_num.
// 6 is the IDF default and the Memory saving rank for Matter-sized traffic.
static constexpr int kMatterWifiStaticRxBufNum = 4;
static constexpr int kMatterWifiDynamicRxBufNum = 8;
static constexpr int kMatterWifiDynamicTxBufNum = 8;
static constexpr int kMatterWifiCacheTxBufNum = 4;
static constexpr int kMatterWifiRxBaWin = 6;

static void initMatterWiFiStack() {
  if (sSelectedNetwork == MATTER_NETWORK_THREAD || sSelectedNetwork == MATTER_NETWORK_ETHERNET) {
    return;
  }
  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) != ESP_ERR_WIFI_NOT_INIT) {
    log_d("Wi-Fi already initialized; Matter buffer limits were not applied.");
    return;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.static_rx_buf_num = kMatterWifiStaticRxBufNum;
  cfg.dynamic_rx_buf_num = kMatterWifiDynamicRxBufNum;
  cfg.tx_buf_type = 1;
  cfg.static_tx_buf_num = 0;
  cfg.dynamic_tx_buf_num = kMatterWifiDynamicTxBufNum;
  if (cfg.cache_tx_buf_num < kMatterWifiCacheTxBufNum) {
    cfg.cache_tx_buf_num = kMatterWifiCacheTxBufNum;
  }
  cfg.rx_ba_win = kMatterWifiRxBaWin;

  const esp_err_t err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    log_e("esp_wifi_init failed: %s", esp_err_to_name(err));
    return;
  }
  log_d(
    "Wi-Fi initialized with Matter buffer limits (static_rx=%d, dynamic_rx=%d, dynamic_tx=%d, rx_ba_win=%d)", kMatterWifiStaticRxBufNum,
    kMatterWifiDynamicRxBufNum, kMatterWifiDynamicTxBufNum, cfg.rx_ba_win
  );
}
#endif

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(
  attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data
) {
  log_d(
    "Attribute update callback: type: %u, endpoint: %u, cluster: %" PRIu32 ", attribute: %" PRIu32 ", val: %u", type, endpoint_id, cluster_id, attribute_id,
    val != nullptr ? val->val.u32 : 0u
  );
  esp_err_t err = ESP_OK;
  MatterEndPoint *ep = (MatterEndPoint *)priv_data;  // endpoint pointer to base class
  switch (type) {
    case PRE_UPDATE:  // Callback before updating the value in the database
      log_v("Attribute update callback: PRE_UPDATE");
      if (ep != nullptr) {
        err = ep->attributeChangeCB(endpoint_id, cluster_id, attribute_id, val) ? ESP_OK : ESP_FAIL;
      }
      break;
    case POST_UPDATE:  // Callback after updating the value in the database
      log_v("Attribute update callback: POST_UPDATE");
      break;
    case READ:  // Callback for reading the attribute value. This is used when the `ATTRIBUTE_FLAG_OVERRIDE` is set.
      log_v("Attribute update callback: READ");
      break;
    case WRITE:  // Callback for writing the attribute value. This is used when the `ATTRIBUTE_FLAG_OVERRIDE` is set.
      log_v("Attribute update callback: WRITE");
      break;
    default: log_v("Attribute update callback: Unknown type %d", type);
  }
  return err;
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
  log_d("Identification callback to endpoint %u: type: %u, effect: %u, variant: %u", endpoint_id, type, effect_id, effect_variant);
  esp_err_t err = ESP_OK;
  MatterEndPoint *ep = (MatterEndPoint *)priv_data;  // endpoint pointer to base class
  // Identify the endpoint sending a counter to the application
  bool identifyIsActive = false;

  if (type == identification::callback_type_t::START) {
    log_v("Identification callback: START");
    identifyIsActive = true;
  } else if (type == identification::callback_type_t::EFFECT) {
    log_v("Identification callback: EFFECT");
  } else if (type == identification::callback_type_t::STOP) {
    identifyIsActive = false;
    log_v("Identification callback: STOP");
  }
  if (ep != nullptr) {
    err = ep->endpointIdentifyCB(endpoint_id, identifyIsActive) ? ESP_OK : ESP_FAIL;
  }

  return err;
}

// This callback is invoked for all Matter events. The application can handle the events as required.
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
  switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
      log_d(
        "Interface %s Address changed", event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Assigned ? "IPv4" : "IPV6"
      );
      break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
      log_d("Commissioning complete");
      if (ArduinoMatter::isBLEMemoryReleaseEnabled()) {
        scheduleShutdownChipBle();
      }
      break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:        log_d("Commissioning failed, fail safe timer expired"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted: log_d("Commissioning session started"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped: log_d("Commissioning session stopped"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:   log_d("Commissioning window opened"); break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:   log_d("Commissioning window closed"); break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
    {
      log_d("Fabric removed successfully");
      if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
        log_d("No fabric left, opening commissioning window");
        chip::CommissioningWindowManager &commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
        constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
        if (!commissionMgr.IsCommissioningWindowOpen()) {
          // After removing last fabric, it does not remove the Wi-Fi credentials and still has IP connectivity so, only advertising on DNS-SD.
          CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds, chip::CommissioningWindowAdvertisement::kDnssdOnly);
          if (err != CHIP_NO_ERROR) {
            log_e("Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
          }
        }
      }
      break;
    }
    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved: log_d("Fabric will be removed"); break;
    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:       log_d("Fabric is updated"); break;
    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:     log_d("Fabric is committed"); break;
    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
      log_d("BLE deinitialized and memory reclaimed");
      notifyBleMemoryReleased();
      break;
    case chip::DeviceLayer::DeviceEventType::kThreadConnectivityChange:
#if CONFIG_ENABLE_MATTER_OVER_THREAD
      if (sSelectedNetwork == MATTER_NETWORK_THREAD &&
          event->ThreadConnectivityChange.Result == chip::DeviceLayer::kConnectivity_Established) {
        startThreadAdvertiseRetries();
      }
#endif
      break;
    case chip::DeviceLayer::DeviceEventType::kDnssdInitialized:
#if CONFIG_ENABLE_MATTER_OVER_THREAD
      if (sSelectedNetwork == MATTER_NETWORK_THREAD) {
#if defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
        // ESP32-C6: EspDnssdInit posts this as soon as Wi-Fi mDNS starts. Thread
        // SRP posts the same event later, after attach + OTBR ClearSrpHost.
        if (!chip::DeviceLayer::ConnectivityMgr().IsThreadAttached()) {
          log_d("Matter DNS-SD initialized (Wi-Fi mDNS); Thread SRP is not ready yet");
          break;
        }
#endif
        log_i("Matter DNS-SD initialized (Thread SRP ready)");
        startThreadAdvertiseRetries();
        break;
      }
#endif
      log_i("Matter DNS-SD initialized");
      break;
    default:                                                       break;
  }
  // Check if the user-defined callback is set
  if (ArduinoMatter::_matterEventCB != nullptr) {
    ArduinoMatter::_matterEventCB(static_cast<matterEvent_t>(event->Type), event);
  }
}

bool ArduinoMatter::isStackStarted() {
  return sLifecycle == MatterLifecycle::StackStarted;
}

void ArduinoMatter::_init() {
  if (sLifecycle != MatterLifecycle::Uninitialized) {
    return;
  }

#if CONFIG_ENABLE_MATTER_OVER_THREAD && defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
#ifndef CONFIG_CUSTOM_NETWORK_CONFIG
  if (sSelectedNetwork == MATTER_NETWORK_THREAD) {
    node_config.root_node.network_commissioning.feature_map =
      chip::to_underlying(NetworkCommissioning::Feature::kThreadNetworkInterface);
  }
#endif
#endif

  // Create a Matter node and add the mandatory Root Node device type on endpoint 0
  // node handle can be used to add/modify other endpoints.
  deviceNode = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
  if (deviceNode == nullptr) {
    log_e("Failed to create Matter node");
    return;
  }

#if CONFIG_ENABLE_MATTER_OVER_THREAD && defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  if (sSelectedNetwork == MATTER_NETWORK_THREAD) {
    hideRootWiFiDiagnostics();
  }
#endif

  sLifecycle = MatterLifecycle::NodeCreated;
}

void ArduinoMatter::begin() {
  if (sLifecycle == MatterLifecycle::StackStarted) {
    return;
  }
  if (sLifecycle != MatterLifecycle::NodeCreated) {
    log_e("No Matter endpoint has been created. Please create an endpoint first.");
    return;
  }

#if defined(CONFIG_BT_CONTROLLER_ENABLED) && CONFIG_ENABLE_CHIPOBLE
  if (isBLECommissioningEnabled() && btMemReleased(BT_MODE_BLE)) {
    log_w("BLE memory has been released; commissioning will use the IP network.");
  }
#endif

#if defined(SOC_WIFI_SUPPORTED) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  initMatterWiFiStack();
#endif

  if (sSelectedNetwork == MATTER_NETWORK_ETHERNET && !ethernetHasUsableIpv6()) {
    log_e("Ethernet selected but no usable IPv6 address. Call ETH.begin(), enableIPv6(), and waitForNetwork() before Matter.begin().");
    return;
  }

#if CONFIG_ENABLE_MATTER_OVER_THREAD
  // Set OpenThread platform config
  esp_openthread_platform_config_t config;
  memset(&config, 0, sizeof(esp_openthread_platform_config_t));
  config.radio_config.radio_mode = RADIO_MODE_NATIVE;
  config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
  config.port_config.storage_partition_name = "nvs";
  config.port_config.netif_queue_size = 10;
  config.port_config.task_queue_size = 10;
  set_openthread_platform_config(&config);
#endif

  applyIdentityBeforeStart();

  /* Matter start */
  esp_err_t err = esp_matter::start(app_event_cb);
  if (err != ESP_OK) {
    log_e("Failed to start Matter, err:%d", err);
    return;
  }
  sLifecycle = MatterLifecycle::StackStarted;
#if CONFIG_ENABLE_MATTER_OVER_THREAD && defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
  bindThreadNetworkCommissioningOnRoot();
#endif
#ifdef CONFIG_ENABLE_WIFI_STATION
  if ((sSelectedNetwork == MATTER_NETWORK_THREAD || sSelectedNetwork == MATTER_NETWORK_ETHERNET) && !matterWifiStackWrapRan()) {
    log_e("InitWiFiStack wrap did not run; Wi-Fi may still start. Check the --wrap flag in platform.txt.");
  }
#endif
  applyIdentityAfterStart();
  applyBlePolicyAfterStart();
  registerMatterIpEventHandler();
  if (selectedHasUsableIpv6()) {
#if CONFIG_ENABLE_MATTER_OVER_THREAD
    if (sSelectedNetwork == MATTER_NETWORK_THREAD) {
      startThreadAdvertiseRetries();
    } else
#endif
    {
      scheduleDnssdRestart();
    }
  }
}

// Network and Commissioning Capability Queries
bool ArduinoMatter::isWiFiStationEnabled() {
#ifdef SOC_WIFI_SUPPORTED
#ifdef CONFIG_ENABLE_WIFI_STATION
  return true;
#endif
#endif
  return false;
}

bool ArduinoMatter::isWiFiAccessPointEnabled() {
  // Check hardware support (SOC capabilities) AND Matter configuration
#ifdef SOC_WIFI_SUPPORTED
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_AP
  return true;
#else
  return false;
#endif
#else
  return false;
#endif
}

bool ArduinoMatter::isThreadEnabled() {
#ifdef CONFIG_ENABLE_MATTER_OVER_THREAD
  return true;
#else
  return false;
#endif
}

bool ArduinoMatter::isEthernetEnabled() {
#ifdef CONFIG_ETH_ENABLED
  return true;
#else
  return false;
#endif
}

bool ArduinoMatter::isNetworkSupported(matterNetwork_t network) {
  switch (network) {
    case MATTER_NETWORK_WIFI:     return isWiFiStationEnabled();
    case MATTER_NETWORK_THREAD:   return isThreadEnabled();
    case MATTER_NETWORK_ETHERNET: return isEthernetEnabled();
    default:                      return false;
  }
}

bool ArduinoMatter::selectNetwork(matterNetwork_t network) {
  const bool disableBle = (network == MATTER_NETWORK_ETHERNET);
  return selectNetwork(network, disableBle);
}

bool ArduinoMatter::selectNetwork(matterNetwork_t network, bool disableBLECommissioning) {
  if (sLifecycle != MatterLifecycle::Uninitialized) {
    log_e("Matter.selectNetwork() must be called before any accessory begin().");
    return false;
  }
  if (network == MATTER_NETWORK_NONE) {
    sSelectedNetwork = MATTER_NETWORK_NONE;
    return true;
  }
  if (!isNetworkSupported(network)) {
    log_e("Matter.selectNetwork(): network %d is not supported on this target.", static_cast<int>(network));
    return false;
  }
#if !CONFIG_ENABLE_CHIPOBLE
  if (network == MATTER_NETWORK_THREAD) {
    log_e("Thread selected but CHIPoBLE is not compiled in. Commission only if the sketch provisions a Thread dataset.");
  }
#endif
  sSelectedNetwork = network;
  if (disableBLECommissioning) {
    if (!setBLECommissioningEnabled(false)) {
      log_w("Matter.selectNetwork(): could not disable CHIPoBLE.");
    }
  }
  return true;
}

matterNetwork_t ArduinoMatter::getSelectedNetwork() {
  return sSelectedNetwork;
}

matterNetwork_t ArduinoMatter::getActiveNetwork() {
  if (sSelectedNetwork != MATTER_NETWORK_NONE && selectedHasUsableIpv6()) {
    return sSelectedNetwork;
  }
  if (netifHasUsableIpv6("WIFI_STA_DEF")) {
    return MATTER_NETWORK_WIFI;
  }
  if (netifHasUsableIpv6("OT_DEF")) {
    return MATTER_NETWORK_THREAD;
  }
  if (ethernetHasUsableIpv6()) {
    return MATTER_NETWORK_ETHERNET;
  }
  return MATTER_NETWORK_NONE;
}

uint16_t ArduinoMatter::getNetworkEndPointId(matterNetwork_t network) {
  switch (network) {
    case MATTER_NETWORK_WIFI:
      if (!isWiFiStationEnabled()) {
        return 0xFFFF;
      }
#if defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
      if (sSelectedNetwork == MATTER_NETWORK_THREAD) {
        return 0xFFFF;
      }
#endif
      return 0;
    case MATTER_NETWORK_THREAD:
      if (!isThreadEnabled()) {
        return 0xFFFF;
      }
#if defined(CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION) && CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
      if (sSelectedNetwork == MATTER_NETWORK_THREAD) {
        return 0;
      }
#endif
#ifdef CONFIG_THREAD_NETWORK_ENDPOINT_ID
      return CONFIG_THREAD_NETWORK_ENDPOINT_ID;
#else
      return 0;
#endif
    case MATTER_NETWORK_ETHERNET:
    default: return 0xFFFF;
  }
}

bool ArduinoMatter::waitForNetwork(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (true) {
    if (selectedHasUsableIpv6()) {
      return true;
    }
    if ((millis() - start) >= timeoutMs) {
      return false;
    }
    delay(50);
  }
}

bool ArduinoMatter::setBLECommissioningEnabled(bool enabled) {
  if (!ensureSetBeforeBegin("setBLECommissioningEnabled")) {
    return false;
  }
#if !CONFIG_ENABLE_CHIPOBLE
  if (enabled) {
    log_e("Matter.setBLECommissioningEnabled(true) is not supported; CHIPoBLE is not compiled in.");
    return false;
  }
  sBleCommissioningEnabled = false;
  return true;
#else
  sBleCommissioningEnabled = enabled;
  return true;
#endif
}

bool ArduinoMatter::isBLECommissioningEnabled() {
#if CONFIG_ENABLE_CHIPOBLE
  return sBleCommissioningEnabled;
#else
  return false;
#endif
}

bool ArduinoMatter::setBLEMemoryReleaseEnabled(bool enabled) {
  if (!ensureSetBeforeBegin("setBLEMemoryReleaseEnabled")) {
    return false;
  }
#if !CONFIG_ENABLE_CHIPOBLE
  if (!enabled) {
    log_e("Matter.setBLEMemoryReleaseEnabled(false) has no effect; CHIPoBLE is not compiled in.");
    return false;
  }
  return true;
#else
  sBleMemoryReleaseEnabled = enabled;
  return true;
#endif
}

bool ArduinoMatter::isBLEMemoryReleaseEnabled() {
  return isBLECommissioningEnabled() && sBleMemoryReleaseEnabled;
}

void ArduinoMatter::applyBlePolicyAfterStart() {
  // CHIPoBLE off: free BLE at begin() (on-network). CHIPoBLE on and already
  // commissioned: honor setBLEMemoryReleaseEnabled(). First-time CHIPoBLE
  // commission uses kCommissioningComplete instead.
  if (!isBLECommissioningEnabled() || (isDeviceCommissioned() && isBLEMemoryReleaseEnabled())) {
    scheduleShutdownChipBle();
  }
}

bool ArduinoMatter::isDeviceCommissioned() {
  return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

bool ArduinoMatter::isWiFiConnected() {
  return chip::DeviceLayer::ConnectivityMgr().IsWiFiStationConnected();
}

bool ArduinoMatter::isThreadConnected() {
  return chip::DeviceLayer::ConnectivityMgr().IsThreadAttached();
}

bool ArduinoMatter::isDeviceConnected() {
  return ArduinoMatter::isWiFiConnected() || ArduinoMatter::isThreadConnected() || ethernetHasUsableIpv6();
}

void ArduinoMatter::decommission() {
  esp_matter::factory_reset();
}

// Global Matter Object
ArduinoMatter Matter;

#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
