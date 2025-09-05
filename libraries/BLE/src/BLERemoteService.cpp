/*
 * BLERemoteService.cpp
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <sstream>
#include "BLERemoteService.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include <esp_err.h>
#include "esp32-hal-log.h"

/***************************************************************************
 *                           Common functions                             *
 ***************************************************************************/

BLERemoteService::~BLERemoteService() {
  removeCharacteristics();
}

/**
 * @brief Get the remote characteristic object for the characteristic UUID.
 * @param [in] uuid Remote characteristic uuid.
 * @return Reference to the remote characteristic object.
 * @throws BLEUuidNotFoundException
 */
BLERemoteCharacteristic *BLERemoteService::getCharacteristic(const char *uuid) {
  return getCharacteristic(BLEUUID(uuid));
}  // getCharacteristic

/**
 * @brief Get the characteristic object for the UUID.
 * @param [in] uuid Characteristic uuid.
 * @return Reference to the characteristic object.
 * @throws BLEUuidNotFoundException
 */
BLERemoteCharacteristic *BLERemoteService::getCharacteristic(BLEUUID uuid) {
  // Design
  // ------
  // We wish to retrieve the characteristic given its UUID.  It is possible that we have not yet asked the
  // device what characteristics it has in which case we have nothing to match against.  If we have not
  // asked the device about its characteristics, then we do that now.  Once we get the results we can then
  // examine the characteristics map to see if it has the characteristic we are looking for.
  if (!m_haveCharacteristics) {
    retrieveCharacteristics();
  }
  std::string v = uuid.toString().c_str();
  for (auto &myPair : m_characteristicMap) {
    if (myPair.first == v) {
      return myPair.second;
    }
  }
  // throw new BLEUuidNotFoundException();  // <-- we dont want exception here, which will cause app crash, we want to search if any characteristic can be found one after another
  return nullptr;
}  // getCharacteristic

/**
 * @brief Retrieve a map of all the characteristics of this service.
 * @return A map of all the characteristics of this service.
 */
std::map<std::string, BLERemoteCharacteristic *> *BLERemoteService::getCharacteristics() {
  log_v(">> getCharacteristics() for service: %s", getUUID().toString().c_str());
  // If is possible that we have not read the characteristics associated with the service so do that
  // now.  The request to retrieve the characteristics by calling "retrieveCharacteristics" is a blocking
  // call and does not return until all the characteristics are available.
  if (!m_haveCharacteristics) {
    retrieveCharacteristics();
  }
  log_v("<< getCharacteristics() for service: %s", getUUID().toString().c_str());
  return &m_characteristicMap;
}  // getCharacteristics

/**
 * @brief Retrieve a map of all the characteristics of this service.
 * @return A map of all the characteristics of this service.
 */
std::map<uint16_t, BLERemoteCharacteristic *> *BLERemoteService::getCharacteristicsByHandle() {
  // If is possible that we have not read the characteristics associated with the service so do that
  // now.  The request to retrieve the characteristics by calling "retrieveCharacteristics" is a blocking
  // call and does not return until all the characteristics are available.
  if (!m_haveCharacteristics) {
    retrieveCharacteristics();
  }
  return &m_characteristicMapByHandle;
}  // getCharacteristicsByHandle

/**
 * @brief This function is designed to get characteristics map when we have multiple characteristics with the same UUID
 */
void BLERemoteService::getCharacteristics(std::map<uint16_t, BLERemoteCharacteristic *> **pCharacteristicMap) {
  log_v(">> getCharacteristics() for service: %s", getUUID().toString().c_str());
  (void)pCharacteristicMap;
  // If is possible that we have not read the characteristics associated with the service so do that
  // now.  The request to retrieve the characteristics by calling "retrieveCharacteristics" is a blocking
  // call and does not return until all the characteristics are available.
  if (!m_haveCharacteristics) {
    retrieveCharacteristics();
  }
  log_v("<< getCharacteristics() for service: %s", getUUID().toString().c_str());
  *pCharacteristicMap = &m_characteristicMapByHandle;
}  // Get the characteristics map.

/**
 * @brief Get the client associated with this service.
 * @return A reference to the client associated with this service.
 */
BLEClient *BLERemoteService::getClient() {
  return m_pClient;
}  // getClient

uint16_t BLERemoteService::getEndHandle() {
  return m_endHandle;
}  // getEndHandle

uint16_t BLERemoteService::getStartHandle() {
  return m_startHandle;
}  // getStartHandle

uint16_t BLERemoteService::getHandle() {
  log_v(">> getHandle: service: %s", getUUID().toString().c_str());
  log_v("<< getHandle: %d 0x%.2x", getStartHandle(), getStartHandle());
  return getStartHandle();
}  // getHandle

BLEUUID BLERemoteService::getUUID() {
  return m_uuid;
}

/**
 * @brief Read the value of a characteristic associated with this service.
 */
String BLERemoteService::getValue(BLEUUID characteristicUuid) {
  log_v(">> readValue: uuid: %s", characteristicUuid.toString().c_str());
  String ret = getCharacteristic(characteristicUuid)->readValue();
  log_v("<< readValue");
  return ret;
}  // readValue

/**
 * @brief Delete the characteristics in the characteristics map.
 * We maintain a map called m_characteristicsMap that contains pointers to BLERemoteCharacteristic
 * object references.  Since we allocated these in this class, we are also responsible for deleting
 * them.  This method does just that.
 * @return N/A.
 */
void BLERemoteService::removeCharacteristics() {
  m_characteristicMap.clear();  // Clear the map
  for (auto &myPair : m_characteristicMapByHandle) {
    delete myPair.second;
    // delete the characteristics only once
  }
  m_characteristicMapByHandle.clear();  // Clear the map
}  // removeCharacteristics

/**
 * @brief Set the value of a characteristic.
 * @param [in] characteristicUuid The characteristic to set.
 * @param [in] value The value to set.
 * @throws BLEUuidNotFound
 */
void BLERemoteService::setValue(BLEUUID characteristicUuid, String value) {
  log_v(">> setValue: uuid: %s", characteristicUuid.toString().c_str());
  getCharacteristic(characteristicUuid)->writeValue(value);
  log_v("<< setValue");
}  // setValue

/**
 * @brief Create a string representation of this remote service.
 * @return A string representation of this remote service.
 */
String BLERemoteService::toString() {
  String res = "Service: uuid: " + m_uuid.toString();
  char val[6];
  res += ", start_handle: ";
  snprintf(val, sizeof(val), "%d", m_startHandle);
  res += val;
  snprintf(val, sizeof(val), "%04x", m_startHandle);
  res += " 0x";
  res += val;
  res += ", end_handle: ";
  snprintf(val, sizeof(val), "%d", m_endHandle);
  res += val;
  snprintf(val, sizeof(val), "%04x", m_endHandle);
  res += " 0x";
  res += val;
  for (auto &myPair : m_characteristicMap) {
    res += "\n" + myPair.second->toString();
    // myPair.second is the value
  }
  return res;
}  // toString

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

BLERemoteService::BLERemoteService(esp_gatt_id_t srvcId, BLEClient *pClient, uint16_t startHandle, uint16_t endHandle) {
  log_v(">> BLERemoteService()");
  m_srvcId = srvcId;
  m_pClient = pClient;
  m_uuid = BLEUUID(m_srvcId);
  m_haveCharacteristics = false;
  m_startHandle = startHandle;
  m_endHandle = endHandle;

  log_v("<< BLERemoteService()");
}

esp_gatt_id_t *BLERemoteService::getSrvcId() {
  return &m_srvcId;
}  // getSrvcId

/**
 * @brief Retrieve all the characteristics for this service.
 * This function will not return until we have all the characteristics.
 * @return N/A
 */
void BLERemoteService::retrieveCharacteristics() {
  log_v(">> getCharacteristics() for service: %s", getUUID().toString().c_str());

  removeCharacteristics();  // Forget any previous characteristics.

  uint16_t offset = 0;
  esp_gattc_char_elem_t result;
  while (true) {
    uint16_t count = 1;  // only room for 1 result allocated, so go one by one
    esp_gatt_status_t status =
      ::esp_ble_gattc_get_all_char(getClient()->getGattcIf(), getClient()->getConnId(), m_startHandle, m_endHandle, &result, &count, offset);

    if (status == ESP_GATT_INVALID_OFFSET) {  // We have reached the end of the entries.
      break;
    }

    if (status != ESP_GATT_OK) {  // If we got an error, end.
      log_e("esp_ble_gattc_get_all_char: %s", BLEUtils::gattStatusToString(status).c_str());
      break;
    }

    if (count == 0) {  // If we failed to get any new records, end.
      break;
    }

    log_d("Found a characteristic: Handle: %d, UUID: %s", result.char_handle, BLEUUID(result.uuid).toString().c_str());

    // We now have a new characteristic ... let us add that to our set of known characteristics
    BLERemoteCharacteristic *pNewRemoteCharacteristic = new BLERemoteCharacteristic(result.char_handle, BLEUUID(result.uuid), result.properties, this);

    m_characteristicMap.insert(
      std::pair<std::string, BLERemoteCharacteristic *>(pNewRemoteCharacteristic->getUUID().toString().c_str(), pNewRemoteCharacteristic)
    );
    m_characteristicMapByHandle.insert(std::pair<uint16_t, BLERemoteCharacteristic *>(result.char_handle, pNewRemoteCharacteristic));
    offset++;  // Increment our count of number of descriptors found.
  }  // Loop forever (until we break inside the loop).

  m_haveCharacteristics = true;  // Remember that we have received the characteristics.
  log_v("<< getCharacteristics()");
}  // getCharacteristics

/**
 * @brief Handle GATT Client events
 */
void BLERemoteService::gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam) {
  switch (event) {
      //
      // ESP_GATTC_GET_CHAR_EVT
      //
      // get_char:
      // - esp_gatt_status_t    status
      // - uin1t6_t             conn_id
      // - esp_gatt_srvc_id_t   srvc_id
      // - esp_gatt_id_t        char_id
      // - esp_gatt_char_prop_t char_prop
      //
      /*
    case ESP_GATTC_GET_CHAR_EVT: {
      // Is this event for this service?  If yes, then the local srvc_id and the event srvc_id will be
      // the same.
      if (compareSrvcId(m_srvcId, evtParam->get_char.srvc_id) == false) {
        break;
      }

      // If the status is NOT OK then we have a problem and continue.
      if (evtParam->get_char.status != ESP_GATT_OK) {
        m_semaphoreGetCharEvt.give();
        break;
      }

      // This is an indication that we now have the characteristic details for a characteristic owned
      // by this service so remember it.
      m_characteristicMap.insert(std::pair<std::string, BLERemoteCharacteristic*>(
          BLEUUID(evtParam->get_char.char_id.uuid).toString().c_str(),
          new BLERemoteCharacteristic(evtParam->get_char.char_id, evtParam->get_char.char_prop, this)  ));


      // Now that we have received a characteristic, lets ask for the next one.
      esp_err_t errRc = ::esp_ble_gattc_get_characteristic(
          m_pClient->getGattcIf(),
          m_pClient->getConnId(),
          &m_srvcId,
          &evtParam->get_char.char_id);
      if (errRc != ESP_OK) {
        log_e("esp_ble_gattc_get_characteristic: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
        break;
      }

      //m_semaphoreGetCharEvt.give();
      break;
    } // ESP_GATTC_GET_CHAR_EVT
*/
    default: break;
  }  // switch

  // Send the event to each of the characteristics owned by this service.
  for (auto &myPair : m_characteristicMapByHandle) {
    myPair.second->gattClientEventHandler(event, gattc_if, evtParam);
  }
}  // gattClientEventHandler

#endif

/***************************************************************************
 *                           NimBLE functions                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

BLERemoteService::BLERemoteService(BLEClient *pClient, const struct ble_gatt_svc *service) {
  log_v(">> BLERemoteService()");
  m_pClient = pClient;
  switch (service->uuid.u.type) {
    case BLE_UUID_TYPE_16:  m_uuid = BLEUUID(service->uuid.u16.value); break;
    case BLE_UUID_TYPE_32:  m_uuid = BLEUUID(service->uuid.u32.value); break;
    case BLE_UUID_TYPE_128: m_uuid = BLEUUID(const_cast<ble_uuid128_t *>(&service->uuid.u128)); break;
    default:                break;
  }
  m_startHandle = service->start_handle;
  m_endHandle = service->end_handle;
  m_haveCharacteristics = false;
  log_v("<< BLERemoteService(): %s", m_uuid.toString().c_str());
}

/**
 * @brief Retrieve all the characteristics for this service.
 * This function will not return until we have all the characteristics.
 */
void BLERemoteService::retrieveCharacteristics() {
  log_v(">> retrieveCharacteristics() for service: %s", getUUID().toString().c_str());

  int rc = 0;
  BLETaskData taskData(const_cast<BLERemoteService *>(this));

  rc = ble_gattc_disc_all_chrs(m_pClient->getConnId(), m_startHandle, m_endHandle, BLERemoteService::characteristicDiscCB, &taskData);

  if (rc != 0) {
    log_e("ble_gattc_disc_all_chrs: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
    return;
  }

  BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
  rc = taskData.m_flags;
  if (rc == 0 || rc == BLE_HS_EDONE) {
    log_d("<< retrieveCharacteristics()");
    return;
  }

  log_e("<< retrieveCharacteristics() rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
}  // retrieveCharacteristics

/**
 * @brief Callback for characteristic discovery.
 * @return success == 0 or error code.
 */
int BLERemoteService::characteristicDiscCB(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
  log_d("Characteristic Discovered >> status: %d handle: %d", error->status, (error->status == 0) ? chr->val_handle : -1);

  BLETaskData *pTaskData = (BLETaskData *)arg;
  BLERemoteService *service = (BLERemoteService *)pTaskData->m_pInstance;

  if (error->status == BLE_HS_ENOTCONN) {
    log_e("<< Characteristic Discovery; Not connected");
    BLEUtils::taskRelease(*pTaskData, error->status);
    return error->status;
  }

  // Make sure the discovery is for this device
  if (service->getClient()->getConnId() != conn_handle) {
    return 0;
  }

  if (error->status == 0) {
    // Found a service - add it to the vector
    BLERemoteCharacteristic *pRemoteCharacteristic = new BLERemoteCharacteristic(service, chr);
    service->m_characteristicMap.insert(
      std::pair<std::string, BLERemoteCharacteristic *>(pRemoteCharacteristic->getUUID().toString().c_str(), pRemoteCharacteristic)
    );
    service->m_characteristicMapByHandle.insert(std::pair<uint16_t, BLERemoteCharacteristic *>(chr->val_handle, pRemoteCharacteristic));
    return 0;
  }

  BLEUtils::taskRelease(*pTaskData, error->status);
  service->m_haveCharacteristics = true;
  log_d("<< Characteristic Discovered");
  return error->status;
}

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
