/*
 * BLEService.cpp
 *
 *  Created on: Mar 25, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

// A service is identified by a UUID.  A service is also the container for one or more characteristics.

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include <esp_err.h>
#include <iomanip>
#include <sstream>
#include <string>

#include "BLEServer.h"
#include "BLEService.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

/*****************************************************************************
 *                            Bluedroid includes                             *
 *****************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatts_api.h>
#endif

/*****************************************************************************
 *                             Common definitions                            *
 *****************************************************************************/

#define NULL_HANDLE (0xffff)

/*****************************************************************************
 *                             Common functions                              *
 *****************************************************************************/

/**
 * @brief Construct an instance of the BLEService
 * @param [in] uuid The UUID of the service.
 * @param [in] numHandles The maximum number of handles associated with the service.
 */
BLEService::BLEService(const char *uuid, uint16_t numHandles) : BLEService(BLEUUID(uuid), numHandles) {}

/**
 * @brief Construct an instance of the BLEService
 * @param [in] uuid The UUID of the service.
 * @param [in] numHandles The maximum number of handles associated with the service.
 */
BLEService::BLEService(BLEUUID uuid, uint16_t numHandles) {
  m_uuid = uuid;
  m_handle = NULL_HANDLE;
  m_pServer = nullptr;
  //m_serializeMutex.setName("BLEService");
  m_numHandles = numHandles;
#ifdef CONFIG_BLUEDROID_ENABLED
  m_lastCreatedCharacteristic = nullptr;
#endif
#if defined(CONFIG_NIMBLE_ENABLED)
  m_pSvcDef = nullptr;
#endif
}  // BLEService

BLEService::~BLEService() {
#if defined(CONFIG_NIMBLE_ENABLED)
  if (m_pSvcDef != nullptr) {
    if (m_pSvcDef->characteristics != nullptr) {
      for (int i = 0; m_pSvcDef->characteristics[i].uuid != NULL; ++i) {
        if (m_pSvcDef->characteristics[i].descriptors) {
          delete (m_pSvcDef->characteristics[i].descriptors);
        }
      }
      delete (m_pSvcDef->characteristics);
    }

    delete (m_pSvcDef);
  }
#endif
}

/**
 * @brief Create the service.
 * Create the service.
 * @param [in] gatts_if The handle of the GATT server interface.
 * @return N/A.
 */

void BLEService::executeCreate(BLEServer *pServer) {
  log_v(">> executeCreate() - Creating service with uuid: %s", getUUID().toString().c_str());
  m_pServer = pServer;
#if defined(CONFIG_BLUEDROID_ENABLED)
  m_semaphoreCreateEvt.take("executeCreate");  // Take the mutex and release at event ESP_GATTS_CREATE_EVT

  esp_gatt_srvc_id_t srvc_id;
  srvc_id.is_primary = true;
  srvc_id.id.inst_id = m_instId;
  srvc_id.id.uuid = *m_uuid.getNative();
  esp_err_t errRc =
    ::esp_ble_gatts_create_service(getServer()->getGattsIf(), &srvc_id, m_numHandles);  // The maximum number of handles associated with the service.

  if (errRc != ESP_OK) {
    log_e("esp_ble_gatts_create_service: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return;
  }

  m_semaphoreCreateEvt.wait("executeCreate");
#endif
  log_v("<< executeCreate");
}  // executeCreate

/**
 * @brief Delete the service.
 * Delete the service.
 * @return N/A.
 */

void BLEService::executeDelete() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  log_v(">> executeDelete()");
  m_semaphoreDeleteEvt.take("executeDelete");  // Take the mutex and release at event ESP_GATTS_DELETE_EVT

  esp_err_t errRc = ::esp_ble_gatts_delete_service(getHandle());

  if (errRc != ESP_OK) {
    log_e("esp_ble_gatts_delete_service: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return;
  }

  m_semaphoreDeleteEvt.wait("executeDelete");
  log_v("<< executeDelete");
#endif

#if defined(CONFIG_NIMBLE_ENABLED) && CONFIG_BT_NIMBLE_DYNAMIC_SERVICE
  ble_gatts_delete_svc(&m_uuid.getNative()->u);
#endif
}  // executeDelete

/**
 * @brief Dump details of this BLE GATT service.
 * @return N/A.
 */
void BLEService::dump() {
  log_d("Service: uuid:%s, handle: 0x%.2x", m_uuid.toString().c_str(), m_handle);
  log_d("Characteristics:\n%s", m_characteristicMap.toString().c_str());
}  // dump

/**
 * @brief Get the UUID of the service.
 * @return the UUID of the service.
 */
BLEUUID BLEService::getUUID() {
  return m_uuid;
}  // getUUID

/**
 * @brief Stop the service.
 */
void BLEService::stop() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  // We ask the BLE runtime to start the service and then create each of the characteristics.
  // We start the service through its local handle which was returned in the ESP_GATTS_CREATE_EVT event
  // obtained as a result of calling esp_ble_gatts_create_service().
  log_v(">> stop(): Stopping service (esp_ble_gatts_stop_service): %s", toString().c_str());
  if (m_handle == NULL_HANDLE) {
    log_e("<< !!! We attempted to stop a service but don't know its handle!");
    return;
  }

  m_semaphoreStopEvt.take("stop");
  esp_err_t errRc = ::esp_ble_gatts_stop_service(m_handle);

  if (errRc != ESP_OK) {
    log_e("<< esp_ble_gatts_stop_service: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return;
  }
  m_semaphoreStopEvt.wait("stop");

  log_v("<< stop()");
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  log_w("NimBLE does not support stopping a service. Ignoring request.");
#endif
}  // stop

/**
 * @brief Set the handle associated with this service.
 * @param [in] handle The handle associated with the service.
 */
void BLEService::setHandle(uint16_t handle) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  log_v(">> setHandle - Handle=0x%.2x, service UUID=%s)", handle, getUUID().toString().c_str());
  if (m_handle != NULL_HANDLE) {
    log_e("!!! Handle is already set %.2x", m_handle);
    return;
  }
  m_handle = handle;
  log_v("<< setHandle");
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  log_w("NimBLE does not support manually setting the handle of a service. Ignoring request.");
#endif
}  // setHandle

/**
 * @brief Get the handle associated with this service.
 * @return The handle associated with this service.
 */
uint16_t BLEService::getHandle() {
  return m_handle;
}  // getHandle

/**
 * @brief Add a characteristic to the service.
 * @param [in] pCharacteristic A pointer to the characteristic to be added.
 */
void BLEService::addCharacteristic(BLECharacteristic *pCharacteristic) {
  // We maintain a mapping of characteristics owned by this service.  These are managed by the
  // BLECharacteristicMap class instance found in m_characteristicMap.  We add the characteristic
  // to the map and then ask the service to add the characteristic at the BLE level (ESP-IDF).

  log_v(">> addCharacteristic()");
  log_d("Adding characteristic: uuid=%s to service: %s", pCharacteristic->getUUID().toString().c_str(), toString().c_str());

  // Check that we don't add the same characteristic twice.
  BLECharacteristic *pExisting = m_characteristicMap.getByUUID(pCharacteristic->getUUID());
  if (pExisting != nullptr) {
    log_w("<< Adding a new characteristic with the same UUID as a previous one");
  }

#if defined(CONFIG_NIMBLE_ENABLED)
  if (pExisting != nullptr) {
    pExisting->m_removed = 0;
  } else
#endif
  {
    // Remember this characteristic in our map of characteristics.  At this point, we can lookup by UUID
    // but not by handle.  The handle is allocated to us on the ESP_GATTS_ADD_CHAR_EVT.
    m_characteristicMap.setByUUID(pCharacteristic, pCharacteristic->getUUID());
  }

#if defined(CONFIG_NIMBLE_ENABLED)
  getServer()->serviceChanged();
#endif

  log_v("<< addCharacteristic()");
}  // addCharacteristic

/**
 * @brief Create a new BLE Characteristic associated with this service.
 * @param [in] uuid - The UUID of the characteristic.
 * @param [in] properties - The properties of the characteristic.
 * @return The new BLE characteristic.
 */
BLECharacteristic *BLEService::createCharacteristic(const char *uuid, uint32_t properties) {
  return createCharacteristic(BLEUUID(uuid), properties);
}

/**
 * @brief Create a new BLE Characteristic associated with this service.
 * @param [in] uuid - The UUID of the characteristic.
 * @param [in] properties - The properties of the characteristic.
 * @return The new BLE characteristic.
 */
BLECharacteristic *BLEService::createCharacteristic(BLEUUID uuid, uint32_t properties) {
  BLECharacteristic *pCharacteristic = new BLECharacteristic(uuid, properties);
  addCharacteristic(pCharacteristic);
  return pCharacteristic;
}  // createCharacteristic

BLECharacteristic *BLEService::getCharacteristic(const char *uuid) {
  return getCharacteristic(BLEUUID(uuid));
}

BLECharacteristic *BLEService::getCharacteristic(BLEUUID uuid) {
  return m_characteristicMap.getByUUID(uuid);
}

/**
 * @brief Return a string representation of this service.
 * A service is defined by:
 * * Its UUID
 * * Its handle
 * @return A string representation of this service.
 */
String BLEService::toString() {
  String res = "UUID: " + getUUID().toString();
  char hex[5];
  snprintf(hex, sizeof(hex), "%04x", getHandle());
  res += ", handle: 0x";
  res += hex;
  return res;
}  // toString

/**
 * @brief Get the BLE server associated with this service.
 * @return The BLEServer associated with this service.
 */
BLEServer *BLEService::getServer() {
  return m_pServer;
}  // getServer

/*****************************************************************************
 *                            Bluedroid functions                            *
 *****************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
/**
 * @brief Get the last created characteristic.
 * It is lamentable that this function has to exist.  It returns the last created characteristic.
 * We need this because the descriptor API is built around the notion that a new descriptor, when created,
 * is associated with the last characteristics created and we need that information.
 * @return The last created characteristic.
 */
BLECharacteristic *BLEService::getLastCreatedCharacteristic() {
  return m_lastCreatedCharacteristic;
}  // getLastCreatedCharacteristic

/**
 * @brief Handle a GATTS server event.
 */
void BLEService::handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    // ESP_GATTS_ADD_CHAR_EVT - Indicate that a characteristic was added to the service.
    // add_char:
    // - esp_gatt_status_t status
    // - uint16_t attr_handle
    // - uint16_t service_handle
    // - esp_bt_uuid_t char_uuid

    // If we have reached the correct service, then locate the characteristic and remember the handle
    // for that characteristic.
    case ESP_GATTS_ADD_CHAR_EVT:
    {
      if (m_handle == param->add_char.service_handle) {
        BLECharacteristic *pCharacteristic = getLastCreatedCharacteristic();
        if (pCharacteristic == nullptr) {
          log_e("Expected to find characteristic with UUID: %s, but didn't!", BLEUUID(param->add_char.char_uuid).toString().c_str());
          dump();
          break;
        }
        pCharacteristic->setHandle(param->add_char.attr_handle);
        m_characteristicMap.setByHandle(param->add_char.attr_handle, pCharacteristic);
        break;
      }  // Reached the correct service.
      break;
    }  // ESP_GATTS_ADD_CHAR_EVT

    // ESP_GATTS_START_EVT
    //
    // start:
    // esp_gatt_status_t status
    // uint16_t service_handle
    case ESP_GATTS_START_EVT:
    {
      if (param->start.service_handle == getHandle()) {
        m_semaphoreStartEvt.give();
      }
      break;
    }  // ESP_GATTS_START_EVT

    // ESP_GATTS_STOP_EVT
    //
    // stop:
    // esp_gatt_status_t status
    // uint16_t service_handle
    //
    case ESP_GATTS_STOP_EVT:
    {
      if (param->stop.service_handle == getHandle()) {
        m_semaphoreStopEvt.give();
      }
      break;
    }  // ESP_GATTS_STOP_EVT

    // ESP_GATTS_CREATE_EVT
    // Called when a new service is registered as having been created.
    //
    // create:
    // * esp_gatt_status_t status
    // * uint16_t service_handle
    // * esp_gatt_srvc_id_t service_id
    // * - esp_gatt_id id
    // *   - esp_bt_uuid uuid
    // *   - uint8_t inst_id
    // * - bool is_primary
    //
    case ESP_GATTS_CREATE_EVT:
    {
      if (getUUID().equals(BLEUUID(param->create.service_id.id.uuid)) && m_instId == param->create.service_id.id.inst_id) {
        setHandle(param->create.service_handle);
        m_semaphoreCreateEvt.give();
      }
      break;
    }  // ESP_GATTS_CREATE_EVT

    // ESP_GATTS_DELETE_EVT
    // Called when a service is deleted.
    //
    // delete:
    // * esp_gatt_status_t status
    // * uint16_t service_handle
    //
    case ESP_GATTS_DELETE_EVT:
    {
      if (param->del.service_handle == getHandle()) {
        m_semaphoreDeleteEvt.give();
      }
      break;
    }  // ESP_GATTS_DELETE_EVT

    default: break;
  }  // Switch

  // Invoke the GATTS handler in each of the associated characteristics.
  m_characteristicMap.handleGATTServerEvent(event, gatts_if, param);
}  // handleGATTServerEvent

/**
 * @brief Start the service.
 * Here we wish to start the service which means that we will respond to partner requests about it.
 * Starting a service also means that we can create the corresponding characteristics.
 * @return Start the service.
 */

bool BLEService::start() {
  // We ask the BLE runtime to start the service and then create each of the characteristics.
  // We start the service through its local handle which was returned in the ESP_GATTS_CREATE_EVT event
  // obtained as a result of calling esp_ble_gatts_create_service().
  //
  log_v(">> start(): Starting service (esp_ble_gatts_start_service): %s", toString().c_str());
  if (m_handle == NULL_HANDLE) {
    log_e("<< !!! We attempted to start a service but don't know its handle!");
    return false;
  }

  BLECharacteristic *pCharacteristic = m_characteristicMap.getFirst();

  while (pCharacteristic != nullptr) {
    m_lastCreatedCharacteristic = pCharacteristic;
    pCharacteristic->executeCreate(this);

    pCharacteristic = m_characteristicMap.getNext();
  }
  // Start each of the characteristics ... these are found in the m_characteristicMap.

  m_semaphoreStartEvt.take("start");
  esp_err_t errRc = ::esp_ble_gatts_start_service(m_handle);

  if (errRc != ESP_OK) {
    log_e("<< esp_ble_gatts_start_service: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return false;
  }
  m_semaphoreStartEvt.wait("start");

  log_v("<< start()");
  return true;
}  // start

#endif  // CONFIG_BLUEDROID_ENABLED

/*****************************************************************************
 *                             NimBLE functions                              *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
/**
 * @brief Remove a characteristic from the service. Check if the characteristic was already removed and if so, check if this
 * is being called to delete the object and do so if requested. Otherwise, ignore the call and return.
 * @param [in] pCharacteristic - The characteristic to remove.
 * @param [in] deleteChr - If true, delete the characteristic.
 */
void BLEService::removeCharacteristic(BLECharacteristic *pCharacteristic, bool deleteChr) {
  if (pCharacteristic->m_removed > 0) {
    if (deleteChr) {
      BLECharacteristic *pExisting = m_characteristicMap.getByUUID(pCharacteristic->getUUID());
      if (pExisting != nullptr) {
        m_characteristicMap.removeCharacteristic(pExisting);
        delete pExisting;
      }
    }

    return;
  }

  pCharacteristic->m_removed = deleteChr ? NIMBLE_ATT_REMOVE_DELETE : NIMBLE_ATT_REMOVE_HIDE;
  getServer()->serviceChanged();
}

/**
 * @brief Builds the database of characteristics/descriptors for the service
 * and registers it with the NimBLE stack.
 * @return bool success/failure .
 */
bool BLEService::start() {
  log_d(">> start(): Starting service: %s", toString().c_str());

  // Rebuild the service definition if the server attributes have changed.
  if (getServer()->m_svcChanged && m_pSvcDef != nullptr) {
    if (m_pSvcDef[0].characteristics) {
      if (m_pSvcDef[0].characteristics[0].descriptors) {
        delete (m_pSvcDef[0].characteristics[0].descriptors);
      }
      delete (m_pSvcDef[0].characteristics);
    }
    delete (m_pSvcDef);
    m_pSvcDef = nullptr;
  }

  if (m_pSvcDef == nullptr) {
    // Nimble requires an array of services to be sent to the api
    // Since we are adding 1 at a time we create an array of 2 and set the type
    // of the second service to 0 to indicate the end of the array.
    ble_gatt_svc_def *svc = new ble_gatt_svc_def[2]{};
    ble_gatt_chr_def *pChr_a = nullptr;
    ble_gatt_dsc_def *pDsc_a = nullptr;

    svc[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    svc[0].uuid = (const ble_uuid_t *)&(m_uuid.getNative()->u);
    svc[0].includes = nullptr;

    int removedCount = 0;
    BLECharacteristic *pCharacteristic;

    pCharacteristic = m_characteristicMap.getFirst();
    while (pCharacteristic != nullptr) {
      if (pCharacteristic->m_removed > 0) {
        if (pCharacteristic->m_removed == NIMBLE_ATT_REMOVE_DELETE) {
          m_characteristicMap.removeCharacteristic(pCharacteristic);
          delete pCharacteristic;
        } else {
          ++removedCount;
        }
      } else {
        pCharacteristic->executeCreate(this);
      }

      pCharacteristic = m_characteristicMap.getNext();
    }

    size_t numChrs = m_characteristicMap.getRegisteredCharacteristicCount() - removedCount;
    log_d("Adding %d characteristics for service %s", numChrs, toString().c_str());

    if (!numChrs) {
      svc[0].characteristics = nullptr;
    } else {
      // Nimble requires the last characteristic to have it's uuid = 0 to indicate the end
      // of the characteristics for the service. We create 1 extra and set it to null
      // for this purpose.
      pChr_a = new ble_gatt_chr_def[numChrs + 1]{};
      uint8_t i = 0;
      pCharacteristic = m_characteristicMap.getFirst();
      while (pCharacteristic != nullptr) {
        if (pCharacteristic->m_removed <= 0) {
          removedCount = 0;
          BLEDescriptor *pDescriptor;

          pDescriptor = pCharacteristic->m_descriptorMap.getFirst();
          while (pDescriptor != nullptr) {
            if (pDescriptor->m_removed > 0) {
              if (pDescriptor->m_removed == NIMBLE_ATT_REMOVE_DELETE) {
                pCharacteristic->m_descriptorMap.removeDescriptor(pDescriptor);
                delete pDescriptor;
              } else {
                ++removedCount;
              }
            }
            pDescriptor = pCharacteristic->m_descriptorMap.getNext();
          }

          size_t numDscs = pCharacteristic->m_descriptorMap.getRegisteredDescriptorCount() - removedCount;
          log_d("Adding %d descriptors for characteristic %s", numDscs, pCharacteristic->getUUID().toString().c_str());

          if (!numDscs) {
            pChr_a[i].descriptors = nullptr;
          } else {
            // Must have last descriptor uuid = 0 so we have to create 1 extra
            pDsc_a = new ble_gatt_dsc_def[numDscs + 1]{};
            uint8_t d = 0;
            pDescriptor = pCharacteristic->m_descriptorMap.getFirst();
            while (pDescriptor != nullptr) {
              if (pDescriptor->m_removed <= 0) {
                pDsc_a[d].uuid = (const ble_uuid_t *)&(pDescriptor->m_bleUUID.getNative()->u);
                pDsc_a[d].att_flags = pDescriptor->m_permissions;
                pDsc_a[d].min_key_size = 0;
                pDsc_a[d].access_cb = BLEDescriptor::handleGATTServerEvent;
                pDsc_a[d].arg = pDescriptor;
                ++d;
              }
              pDescriptor = pCharacteristic->m_descriptorMap.getNext();
            }

            pDsc_a[numDscs].uuid = nullptr;
            pChr_a[i].descriptors = pDsc_a;
          }

          pChr_a[i].uuid = (const ble_uuid_t *)&(pCharacteristic->m_bleUUID.getNative()->u);
          pChr_a[i].access_cb = BLECharacteristic::handleGATTServerEvent;
          pChr_a[i].arg = pCharacteristic;
          pChr_a[i].flags = pCharacteristic->m_properties;
          pChr_a[i].min_key_size = 0;
          pChr_a[i].val_handle = &pCharacteristic->m_handle;
          ++i;
        }

        pCharacteristic = m_characteristicMap.getNext();
      }

      pChr_a[numChrs].uuid = nullptr;
      svc[0].characteristics = pChr_a;
    }

    // end of services must indicate to api with type = 0
    svc[1].type = 0;
    m_pSvcDef = svc;
  }

  int rc = ble_gatts_count_cfg((const ble_gatt_svc_def *)m_pSvcDef);
  if (rc != 0) {
    log_e("ble_gatts_count_cfg failed, rc= %d, %s", rc, BLEUtils::returnCodeToString(rc));
    return false;
  }

  rc = ble_gatts_add_svcs((const ble_gatt_svc_def *)m_pSvcDef);
  if (rc != 0) {
    log_e("ble_gatts_add_svcs, rc= %d, %s", rc, BLEUtils::returnCodeToString(rc));
    return false;
  }

  log_d("<< start()");
  return true;
}  // start

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
