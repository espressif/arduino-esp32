/*
 * BLERemoteCharacteristic.cpp
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

#include <esp_err.h>

#include <sstream>
#include "WString.h"
//#include "BLEExceptions.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include "BLERemoteCharacteristic.h"
#include "BLERemoteDescriptor.h"
#include "esp32-hal-log.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gattc_api.h>
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <freertos/task.h>
#include <host/ble_uuid.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#endif

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

/**
 *@brief Destructor.
 */
BLERemoteCharacteristic::~BLERemoteCharacteristic() {
  removeDescriptors();  // Release resources for any descriptor information we may have allocated.
  free(m_rawData);
}  // ~BLERemoteCharacteristic

/**
 * @brief Does the characteristic support broadcasting?
 * @return True if the characteristic supports broadcasting.
 */
bool BLERemoteCharacteristic::canBroadcast() {
  return (m_charProp & ESP_GATT_CHAR_PROP_BIT_BROADCAST) != 0;
}  // canBroadcast

/**
 * @brief Does the characteristic support indications?
 * @return True if the characteristic supports indications.
 */
bool BLERemoteCharacteristic::canIndicate() {
  return (m_charProp & ESP_GATT_CHAR_PROP_BIT_INDICATE) != 0;
}  // canIndicate

/**
 * @brief Does the characteristic support notifications?
 * @return True if the characteristic supports notifications.
 */
bool BLERemoteCharacteristic::canNotify() {
  return (m_charProp & ESP_GATT_CHAR_PROP_BIT_NOTIFY) != 0;
}  // canNotify

/**
 * @brief Does the characteristic support reading?
 * @return True if the characteristic supports reading.
 */
bool BLERemoteCharacteristic::canRead() {
  return (m_charProp & ESP_GATT_CHAR_PROP_BIT_READ) != 0;
}  // canRead

/**
 * @brief Does the characteristic support writing?
 * @return True if the characteristic supports writing.
 */
bool BLERemoteCharacteristic::canWrite() {
  return (m_charProp & ESP_GATT_CHAR_PROP_BIT_WRITE) != 0;
}  // canWrite

/**
 * @brief Does the characteristic support writing with no response?
 * @return True if the characteristic supports writing with no response.
 */
bool BLERemoteCharacteristic::canWriteNoResponse() {
  return (m_charProp & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) != 0;
}  // canWriteNoResponse

/**
 * @brief Retrieve the map of descriptors keyed by UUID.
 */
std::map<std::string, BLERemoteDescriptor *> *BLERemoteCharacteristic::getDescriptors() {
  // Retrieve descriptors if not already done (lazy loading)
  if (!m_descriptorsRetrieved) {
    log_d("Descriptors not yet retrieved, retrieving now...");
    retrieveDescriptors();
  }
  return &m_descriptorMap;
}  // getDescriptors

/**
 * @brief Get the handle for this characteristic.
 * @return The handle for this characteristic.
 */
uint16_t BLERemoteCharacteristic::getHandle() {
  //log_v(">> getHandle: Characteristic: %s", getUUID().toString().c_str());
  //log_v("<< getHandle: %d 0x%.2x", m_handle, m_handle);
  return m_handle;
}  // getHandle

/**
 * @brief Get the descriptor instance with the given UUID that belongs to this characteristic.
 * @param [in] uuid The UUID of the descriptor to find.
 * @return The Remote descriptor (if present) or null if not present.
 */
BLERemoteDescriptor *BLERemoteCharacteristic::getDescriptor(BLEUUID uuid) {
  log_v(">> getDescriptor: uuid: %s", uuid.toString().c_str());
  // Retrieve descriptors if not already done (lazy loading)
  if (!m_descriptorsRetrieved) {
    log_d("Descriptors not yet retrieved, retrieving now...");
    retrieveDescriptors();
  }
  std::string v = uuid.toString().c_str();
  for (auto &myPair : m_descriptorMap) {
    if (myPair.first == v) {
      log_v("<< getDescriptor: found");
      return myPair.second;
    }
  }
  log_v("<< getDescriptor: Not found");
  return nullptr;
}  // getDescriptor

/**
 * @brief Get the remote service associated with this characteristic.
 * @return The remote service associated with this characteristic.
 */
BLERemoteService *BLERemoteCharacteristic::getRemoteService() {
  return m_pRemoteService;
}  // getRemoteService

/**
 * @brief Get the UUID for this characteristic.
 * @return The UUID for this characteristic.
 */
BLEUUID BLERemoteCharacteristic::getUUID() {
  return m_uuid;
}  // getUUID

/**
 * @brief Read an unsigned 16 bit value
 * @return The unsigned 16 bit value.
 */
uint16_t BLERemoteCharacteristic::readUInt16() {
  String value = readValue();
  if (value.length() >= 2) {
    return *(uint16_t *)(value.c_str());
  }
  return 0;
}  // readUInt16

/**
 * @brief Read an unsigned 32 bit value.
 * @return the unsigned 32 bit value.
 */
uint32_t BLERemoteCharacteristic::readUInt32() {
  String value = readValue();
  if (value.length() >= 4) {
    return *(uint32_t *)(value.c_str());
  }
  return 0;
}  // readUInt32

/**
 * @brief Read a byte value
 * @return The value as a byte
 */
uint8_t BLERemoteCharacteristic::readUInt8() {
  String value = readValue();
  if (value.length() >= 1) {
    return (uint8_t)value[0];
  }
  return 0;
}  // readUInt8

/**
 * @brief Read a float value.
 * @return the float value.
 */
float BLERemoteCharacteristic::readFloat() {
  String value = readValue();
  if (value.length() >= 4) {
    return *(float *)(value.c_str());
  }
  return 0.0;
}  // readFloat

/**
 * @brief Register for notifications.
 * @param [in] notifyCallback A callback to be invoked for a notification.  If NULL is provided then we are
 * unregistering a notification.
 * @return N/A.
 */
void BLERemoteCharacteristic::registerForNotify(notify_callback notifyCallback, bool notifications, bool descriptorRequiresRegistration) {
  log_v(">> registerForNotify(): %s", toString().c_str());

#if defined(CONFIG_BLUEDROID_ENABLED)
  m_notifyCallback = notifyCallback;  // Save the notification callback.

  m_semaphoreRegForNotifyEvt.take("registerForNotify");

  if (notifyCallback != nullptr) {  // If we have a callback function, then this is a registration.
    esp_err_t errRc = ::esp_ble_gattc_register_for_notify(
      m_pRemoteService->getClient()->getGattcIf(), m_pRemoteService->getClient()->getPeerAddress().getNative(), getHandle()
    );

    if (errRc != ESP_OK) {
      log_e("esp_ble_gattc_register_for_notify: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    }

    uint8_t val[] = {0x01, 0x00};
    if (!notifications) {
      val[0] = 0x02;
    }
    BLERemoteDescriptor *desc = getDescriptor(BLEUUID((uint16_t)0x2902));
    if (desc != nullptr && descriptorRequiresRegistration) {
      desc->writeValue(val, 2, true);
    }
  }  // End Register
  else {  // If we weren't passed a callback function, then this is an unregistration.
    esp_err_t errRc = ::esp_ble_gattc_unregister_for_notify(
      m_pRemoteService->getClient()->getGattcIf(), m_pRemoteService->getClient()->getPeerAddress().getNative(), getHandle()
    );

    if (errRc != ESP_OK) {
      log_e("esp_ble_gattc_unregister_for_notify: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    }

    uint8_t val[] = {0x00, 0x00};
    BLERemoteDescriptor *desc = getDescriptor((uint16_t)0x2902);
    if (desc != nullptr && descriptorRequiresRegistration) {
      desc->writeValue(val, 2, true);
    }
  }  // End Unregister

  m_semaphoreRegForNotifyEvt.wait("registerForNotify");

#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  bool success;
  if (notifyCallback != nullptr) {
    success = subscribe(notifications, notifyCallback, descriptorRequiresRegistration);
  } else {
    success = unsubscribe(descriptorRequiresRegistration);
  }

  if (!success) {
    log_e("Failed to subscribe/unsubscribe for notify");
  }
#endif
  log_v("<< registerForNotify()");
}  // registerForNotify

/**
 * @brief Delete the descriptors in the descriptor map.
 * We maintain a map called m_descriptorMap that contains pointers to BLERemoteDescriptors
 * object references.  Since we allocated these in this class, we are also responsible for deleting
 * them.  This method does just that.
 * @return N/A.
 */
void BLERemoteCharacteristic::removeDescriptors() {
  // Iterate through all the descriptors releasing their storage and erasing them from the map.
  for (auto &myPair : m_descriptorMap) {
    delete myPair.second;
  }
  m_descriptorMap.clear();
  m_descriptorsRetrieved = false;  // Allow descriptors to be retrieved again
}  // removeCharacteristics

/**
 * @brief Convert a BLERemoteCharacteristic to a string representation;
 * @return a String representation.
 */
String BLERemoteCharacteristic::toString() {
  String res = "Characteristic: uuid: " + m_uuid.toString();
  char val[6];
  res += ", handle: ";
  snprintf(val, sizeof(val), "%d", getHandle());
  res += val;
  res += " 0x";
  snprintf(val, sizeof(val), "%04x", getHandle());
  res += val;
  res += ", props: " + BLEUtils::characteristicPropertiesToString(m_charProp);
  return res;
}  // toString

/**
 * @brief Write the new value for the characteristic.
 * @param [in] newValue The new value to write.
 * @param [in] response Do we expect a response?
 * @return N/A.
 */
bool BLERemoteCharacteristic::writeValue(String newValue, bool response) {
  return writeValue((uint8_t *)newValue.c_str(), newValue.length(), response);
}  // writeValue

/**
 * @brief Write the new value for the characteristic.
 *
 * This is a convenience function.  Many BLE characteristics are a single byte of data.
 * @param [in] newValue The new byte value to write.
 * @param [in] response Whether we require a response from the write.
 * @return N/A.
 */
bool BLERemoteCharacteristic::writeValue(uint8_t newValue, bool response) {
  return writeValue(&newValue, 1, response);
}  // writeValue

/**
 * @brief Read raw data from remote characteristic as hex bytes
 * @return return pointer data read
 */
uint8_t *BLERemoteCharacteristic::readRawData() {
  return m_rawData;
}

/**
 * @brief Set authentication request type for characteristic
 * @param [in] auth Authentication request type.
 */
void BLERemoteCharacteristic::setAuth(uint8_t auth) {
  m_auth = auth;
}

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief Constructor.
 * @param [in] handle The BLE server side handle of this characteristic.
 * @param [in] uuid The UUID of this characteristic.
 * @param [in] charProp The properties of this characteristic.
 * @param [in] pRemoteService A reference to the remote service to which this remote characteristic pertains.
 */
BLERemoteCharacteristic::BLERemoteCharacteristic(uint16_t handle, BLEUUID uuid, esp_gatt_char_prop_t charProp, BLERemoteService *pRemoteService) {
  log_v(">> BLERemoteCharacteristic: handle: %d 0x%d, uuid: %s", handle, handle, uuid.toString().c_str());
  m_handle = handle;
  m_uuid = uuid;
  m_charProp = charProp;
  m_pRemoteService = pRemoteService;
  m_notifyCallback = nullptr;
  m_rawData = nullptr;
  m_auth = ESP_GATT_AUTH_REQ_NONE;
  m_descriptorsRetrieved = false;

  retrieveDescriptors();  // Get the descriptors for this characteristic
  log_v("<< BLERemoteCharacteristic");
}  // BLERemoteCharacteristic

/**
 * @brief Handle GATT Client events.
 * When an event arrives for a GATT client we give this characteristic the opportunity to
 * take a look at it to see if there is interest in it.
 * @param [in] event The type of event.
 * @param [in] gattc_if The interface on which the event was received.
 * @param [in] evtParam Payload data for the event.
 * @returns N/A
 */
void BLERemoteCharacteristic::gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam) {
  switch (event) {
    // ESP_GATTC_NOTIFY_EVT
    //
    // notify
    // - uint16_t           conn_id    - The connection identifier of the server.
    // - esp_bd_addr_t      remote_bda - The device address of the BLE server.
    // - uint16_t           handle     - The handle of the characteristic for which the event is being received.
    // - uint16_t           value_len  - The length of the received data.
    // - uint8_t*           value      - The received data.
    // - bool               is_notify  - True if this is a notify, false if it is an indicate.
    //
    // We have received a notification event which means that the server wishes us to know about a notification
    // piece of data.  What we must now do is find the characteristic with the associated handle and then
    // invoke its notification callback (if it has one).
    case ESP_GATTC_NOTIFY_EVT:
    {
      if (evtParam->notify.handle != getHandle()) {
        break;
      }
      if (m_notifyCallback != nullptr) {
        log_d("Invoking callback for notification on characteristic %s", toString().c_str());
        m_notifyCallback(this, evtParam->notify.value, evtParam->notify.value_len, evtParam->notify.is_notify);
      }  // End we have a callback function ...
      break;
    }  // ESP_GATTC_NOTIFY_EVT

    // ESP_GATTC_READ_CHAR_EVT
    // This event indicates that the server has responded to the read request.
    //
    // read:
    // - esp_gatt_status_t  status
    // - uint16_t           conn_id
    // - uint16_t           handle
    // - uint8_t*           value
    // - uint16_t           value_len
    case ESP_GATTC_READ_CHAR_EVT:
    {
      // If this event is not for us, then nothing further to do.
      if (evtParam->read.handle != getHandle()) {
        break;
      }

      // At this point, we have determined that the event is for us, so now we save the value
      // and unlock the semaphore to ensure that the requester of the data can continue.
      if (evtParam->read.status == ESP_GATT_OK) {
        m_value = String((char *)evtParam->read.value, evtParam->read.value_len);
        if (m_rawData != nullptr) {
          free(m_rawData);
        }
        m_rawData = (uint8_t *)calloc(evtParam->read.value_len, sizeof(uint8_t));
        memcpy(m_rawData, evtParam->read.value, evtParam->read.value_len);
      } else {
        m_value = "";
      }

      m_semaphoreReadCharEvt.give();
      break;
    }  // ESP_GATTC_READ_CHAR_EVT

    // ESP_GATTC_REG_FOR_NOTIFY_EVT
    //
    // reg_for_notify:
    // - esp_gatt_status_t status
    // - uint16_t          handle
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    {
      // If the request is not for this BLERemoteCharacteristic then move on to the next.
      if (evtParam->reg_for_notify.handle != getHandle()) {
        break;
      }

      // We have processed the notify registration and can unlock the semaphore.
      m_semaphoreRegForNotifyEvt.give();
      break;
    }  // ESP_GATTC_REG_FOR_NOTIFY_EVT

    // ESP_GATTC_UNREG_FOR_NOTIFY_EVT
    //
    // unreg_for_notify:
    // - esp_gatt_status_t status
    // - uint16_t          handle
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
    {
      if (evtParam->unreg_for_notify.handle != getHandle()) {
        break;
      }
      // We have processed the notify un-registration and can unlock the semaphore.
      m_semaphoreRegForNotifyEvt.give();
      break;
    }  // ESP_GATTC_UNREG_FOR_NOTIFY_EVT:

    // ESP_GATTC_WRITE_CHAR_EVT
    //
    // write:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    // - uint16_t          handle
    case ESP_GATTC_WRITE_CHAR_EVT:
    {
      // Determine if this event is for us and, if not, pass onwards.
      if (evtParam->write.handle != getHandle()) {
        break;
      }

      // There is nothing further we need to do here.  This is merely an indication
      // that the write has completed and we can unlock the caller.
      m_semaphoreWriteCharEvt.give();
      break;
    }  // ESP_GATTC_WRITE_CHAR_EVT

    case ESP_GATTC_READ_DESCR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT:
      for (auto &myPair : m_descriptorMap) {
        myPair.second->gattClientEventHandler(event, gattc_if, evtParam);
      }
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      // Cleanup semaphores to avoid deadlocks.
      m_semaphoreReadCharEvt.give(1);
      m_semaphoreWriteCharEvt.give(1);
      break;

    default: break;
  }  // End switch
};  // gattClientEventHandler

/**
 * @brief Populate the descriptors (if any) for this characteristic.
 */
void BLERemoteCharacteristic::retrieveDescriptors() {
  log_v(">> retrieveDescriptors() for characteristic: %s", getUUID().toString().c_str());

  removeDescriptors();  // Remove any existing descriptors.

  // Loop over each of the descriptors within the service associated with this characteristic.
  // For each descriptor we find, create a BLERemoteDescriptor instance.
  uint16_t offset = 0;
  esp_gattc_descr_elem_t result;
  while (true) {
    uint16_t count = 10;
    esp_gatt_status_t status = ::esp_ble_gattc_get_all_descr(
      getRemoteService()->getClient()->getGattcIf(), getRemoteService()->getClient()->getConnId(), getHandle(), &result, &count, offset
    );

    if (status == ESP_GATT_INVALID_OFFSET) {  // We have reached the end of the entries.
      break;
    }

    if (status != ESP_GATT_OK) {
      log_e("esp_ble_gattc_get_all_descr: %s", BLEUtils::gattStatusToString(status).c_str());
      break;
    }

    if (count == 0) {
      break;
    }

    log_d("Found a descriptor: Handle: %d, UUID: %s", result.handle, BLEUUID(result.uuid).toString().c_str());

    // We now have a new characteristic ... let us add that to our set of known characteristics
    BLERemoteDescriptor *pNewRemoteDescriptor = new BLERemoteDescriptor(result.handle, BLEUUID(result.uuid), this);

    m_descriptorMap.insert(std::pair<std::string, BLERemoteDescriptor *>(pNewRemoteDescriptor->getUUID().toString().c_str(), pNewRemoteDescriptor));

    offset++;
  }  // while true
  //m_haveCharacteristics = true; // Remember that we have received the characteristics.
  m_descriptorsRetrieved = true;
  log_v("<< retrieveDescriptors(): Found %d descriptors.", offset);
}  // getDescriptors

/**
 * @brief Read the value of the remote characteristic.
 * @return The value of the remote characteristic.
 */
String BLERemoteCharacteristic::readValue() {
  log_v(">> readValue(): uuid: %s, handle: %d 0x%.2x", getUUID().toString().c_str(), getHandle(), getHandle());

  // Check to see that we are connected.
  if (!getRemoteService()->getClient()->isConnected()) {
    log_e("Disconnected");
    return String();
  }

  // Wait for authentication to complete if bonding is enabled
  // This prevents the read request from being made while pairing is in progress
  BLESecurity::waitForAuthenticationComplete();

  m_semaphoreReadCharEvt.take("readValue");

  // Ask the BLE subsystem to retrieve the value for the remote hosted characteristic.
  // This is an asynchronous request which means that we must block waiting for the response
  // to become available.
  esp_err_t errRc = ::esp_ble_gattc_read_char(
    m_pRemoteService->getClient()->getGattcIf(),
    m_pRemoteService->getClient()->getConnId(),  // The connection ID to the BLE server
    getHandle(),                                 // The handle of this characteristic
    (esp_gatt_auth_req_t)m_auth
  );  // Security

  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_read_char: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return "";
  }

  // Block waiting for the event that indicates that the read has completed.  When it has, the String found
  // in m_value will contain our data.
  m_semaphoreReadCharEvt.wait("readValue");

  log_v("<< readValue(): length: %d", m_value.length());
  return m_value;
}  // readValue

/**
 * @brief Write the new value for the characteristic from a data buffer.
 * @param [in] data A pointer to a data buffer.
 * @param [in] length The length of the data in the data buffer.
 * @param [in] response Whether we require a response from the write.
 * @return True if successful
 */
bool BLERemoteCharacteristic::writeValue(uint8_t *data, size_t length, bool response) {
  // writeValue(String((char*)data, length), response);
  log_v(">> writeValue(), length: %d", length);

  // Check to see that we are connected.
  if (!getRemoteService()->getClient()->isConnected()) {
    log_e("Disconnected");
    return false;
  }

  // Wait for authentication to complete if bonding is enabled
  // This prevents the write request from being made while pairing is in progress
  BLESecurity::waitForAuthenticationComplete();

  m_semaphoreWriteCharEvt.take("writeValue");
  // Invoke the ESP-IDF API to perform the write.
  esp_err_t errRc = ::esp_ble_gattc_write_char(
    m_pRemoteService->getClient()->getGattcIf(), m_pRemoteService->getClient()->getConnId(), getHandle(), length, data,
    response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP, (esp_gatt_auth_req_t)m_auth
  );

  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_write_char: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return false;
  }

  m_semaphoreWriteCharEvt.wait("writeValue");

  log_v("<< writeValue");
  return true;
}  // writeValue

#endif

/***************************************************************************
 *                           NimBLE functions                              *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief Constructor.
 * @param [in] reference to the service this characteristic belongs to.
 * @param [in] ble_gatt_chr struct defined as:
 *  struct ble_gatt_chr {
 *      uint16_t def_handle;
 *      uint16_t val_handle;
 *      uint8_t properties;
 *      ble_uuid_any_t uuid;
 *  };
 */
BLERemoteCharacteristic::BLERemoteCharacteristic(BLERemoteService *pRemoteService, const struct ble_gatt_chr *chr) {
  log_v(">> BLERemoteCharacteristic()");
  switch (chr->uuid.u.type) {
    case BLE_UUID_TYPE_16:  m_uuid = BLEUUID(chr->uuid.u16.value); break;
    case BLE_UUID_TYPE_32:  m_uuid = BLEUUID(chr->uuid.u32.value); break;
    case BLE_UUID_TYPE_128: m_uuid = BLEUUID(const_cast<ble_uuid128_t *>(&chr->uuid.u128)); break;
    default:                break;
  }

  m_handle = chr->val_handle;
  m_defHandle = chr->def_handle;
  m_charProp = chr->properties;
  m_pRemoteService = pRemoteService;
  m_notifyCallback = nullptr;
  m_rawData = nullptr;
  m_auth = 0;
  m_descriptorsRetrieved = false;

  // Don't retrieve descriptors in constructor for NimBLE to avoid deadlock
  // Descriptors will be retrieved on-demand when needed (e.g., for notifications)

  log_v("<< BLERemoteCharacteristic(): %s", m_uuid.toString().c_str());
}  // BLERemoteCharacteristic

/**
 * @brief Callback used by the API when a descriptor is discovered or search complete.
 */
int BLERemoteCharacteristic::descriptorDiscCB(
  uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg
) {
  int rc = error->status;
  log_d("Descriptor Discovered >> status: %d handle: %d", rc, (rc == 0) ? dsc->handle : -1);

  desc_filter_t *filter = (desc_filter_t *)arg;
  const BLEUUID *uuid_filter = filter->uuid;
  BLETaskData *pTaskData = (BLETaskData *)filter->task_data;
  BLERemoteCharacteristic *characteristic = (BLERemoteCharacteristic *)pTaskData->m_pInstance;

  if (characteristic->getRemoteService()->getClient()->getConnId() != conn_handle) {
    return 0;
  }

  if (rc == 0 && characteristic->getHandle() == chr_val_handle && (!uuid_filter || ble_uuid_cmp(&uuid_filter->getNative()->u, &dsc->uuid.u) == 0)) {
    BLERemoteDescriptor *pNewRemoteDescriptor = new BLERemoteDescriptor(characteristic, dsc);
    characteristic->m_descriptorMap.insert(
      std::pair<std::string, BLERemoteDescriptor *>(pNewRemoteDescriptor->getUUID().toString().c_str(), pNewRemoteDescriptor)
    );
    rc = !!uuid_filter * BLE_HS_EDONE;
  }

  if (rc != 0) {
    BLEUtils::taskRelease(*pTaskData, rc);
    log_d("<< Descriptor Discovery");
  }

  return rc;
}

/**
 * @brief Callback for characteristic read operation.
 * @return success == 0 or error code.
 */
int BLERemoteCharacteristic::onReadCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  BLETaskData *pTaskData = static_cast<BLETaskData *>(arg);
  BLERemoteCharacteristic *characteristic = static_cast<BLERemoteCharacteristic *>(pTaskData->m_pInstance);

  if (error->status == BLE_HS_ENOTCONN) {
    log_e("<< Characteristic Read; Not connected");
    BLEUtils::taskRelease(*pTaskData, error->status);
    return error->status;
  }

  if (characteristic->getRemoteService()->getClient()->getConnId() != conn_handle) {
    return 0;
  }

  int rc = error->status;
  log_i("Read complete; status=%d conn_handle=%d", rc, conn_handle);

  String *strBuf = (String *)pTaskData->m_pBuf;

  if (rc == 0) {
    if (attr) {
      uint32_t data_len = OS_MBUF_PKTLEN(attr->om);
      if (((*strBuf).length() + data_len) > BLE_ATT_ATTR_MAX_LEN) {
        rc = BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      } else {
        log_i("Got %d bytes", data_len);
        (*strBuf) += String((char *)attr->om->om_data, data_len);
        return 0;
      }
    }
  }

  BLEUtils::taskRelease(*pTaskData, rc);
  return rc;
}

/**
 * @brief Callback for characteristic write operation.
 * @return success == 0 or error code.
 */
int BLERemoteCharacteristic::onWriteCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  BLETaskData *pTaskData = static_cast<BLETaskData *>(arg);
  BLERemoteCharacteristic *characteristic = static_cast<BLERemoteCharacteristic *>(pTaskData->m_pInstance);

  if (error->status == BLE_HS_ENOTCONN) {
    log_e("<< Characteristic Write; Not connected");
    BLEUtils::taskRelease(*pTaskData, error->status);
    return error->status;
  }

  if (characteristic->getRemoteService()->getClient()->getConnId() != conn_handle) {
    return 0;
  }

  log_i("Write complete; status=%d conn_handle=%d", error->status, conn_handle);
  BLEUtils::taskRelease(*pTaskData, error->status);
  return 0;
}

/**
 * @brief Populate the descriptors (if any) for this characteristic.
 * @param [in] the end handle of the characteristic, or the service, whichever comes first.
 */
bool BLERemoteCharacteristic::retrieveDescriptors(const BLEUUID *uuid_filter) {
  log_d(">> retrieveDescriptors() for characteristic: %s", getUUID().toString().c_str());

  // If this is the last handle then there are no descriptors
  if (m_handle == getRemoteService()->getEndHandle()) {
    m_descriptorsRetrieved = true;
    log_d("<< retrieveDescriptors(): No descriptors found");
    return true;
  }

  BLETaskData taskData(const_cast<BLERemoteCharacteristic *>(this));
  desc_filter_t filter = {uuid_filter, &taskData};
  int rc = 0;

  rc = ble_gattc_disc_all_dscs(
    getRemoteService()->getClient()->getConnId(), m_handle, getRemoteService()->getEndHandle(), BLERemoteCharacteristic::descriptorDiscCB, &filter
  );

  if (rc != 0) {
    log_e("ble_gattc_disc_all_dscs: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
    return false;
  }

  [[maybe_unused]]
  size_t prevDscCount = m_descriptorMap.size();
  BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
  rc = ((BLETaskData *)filter.task_data)->m_flags;

  if (rc != BLE_HS_EDONE) {
    log_e("<< retrieveDescriptors(): failed: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
    return false;
  }

  m_descriptorsRetrieved = true;
  log_d("<< retrieveDescriptors(): Found %d descriptors.", m_descriptorMap.size() - prevDscCount);
  return true;
}  // retrieveDescriptors

/**
 * @brief Read the value of the remote characteristic.
 * @return The value of the remote characteristic.
 */
String BLERemoteCharacteristic::readValue() {
  log_d(">> readValue(): uuid: %s, handle: %d 0x%.2x", getUUID().toString().c_str(), getHandle(), getHandle());

  BLEClient *pClient = getRemoteService()->getClient();
  String value{};

  if (!pClient->isConnected()) {
    log_e("Disconnected");
    return value;
  }

  int rc = 0;
  int retryCount = 1;
  BLETaskData taskData(const_cast<BLERemoteCharacteristic *>(this), 0, &value);

  do {
    rc = ble_gattc_read_long(pClient->getConnId(), m_handle, 0, BLERemoteCharacteristic::onReadCB, &taskData);
    if (rc != 0) {
      goto exit;
    }

    BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
    rc = taskData.m_flags;

    switch (rc) {
      case 0:
      case BLE_HS_EDONE: rc = 0; break;
      // Characteristic is not long-readable, return with what we have.
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG):
        log_i("Attribute not long");
        rc = ble_gattc_read(pClient->getConnId(), m_handle, BLERemoteCharacteristic::onReadCB, &taskData);
        if (rc != 0) {
          goto exit;
        }
        retryCount++;
        break;
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN):
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHOR):
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC):
        if (BLESecurity::m_securityEnabled && retryCount && pClient->secureConnection()) {
          break;
        }
      /* Else falls through. */
      default: goto exit;
    }
  } while (rc != 0 && retryCount--);

  m_semaphoreReadCharEvt.take("readValue");
  m_value = value;
  m_rawData = (uint8_t *)calloc(value.length(), sizeof(uint8_t));
  for (size_t i = 0; i < value.length(); i++) {
    m_rawData[i] = value[i];
  }
  m_semaphoreReadCharEvt.give();

exit:
  if (rc != 0) {
    log_e("<< readValue failed rc=%d, %s", rc, BLEUtils::returnCodeToString(rc));
  } else {
    log_d("<< readValue length: %d rc=%d", value.length(), rc);
  }

  return value;
}  // readValue

/**
 * @brief Write the new value for the characteristic from a data buffer.
 * @param [in] data A pointer to a data buffer.
 * @param [in] length The length of the data in the data buffer.
 * @param [in] response Whether we require a response from the write.
 * @return false if not connected or cant perform write for some reason.
 */
bool BLERemoteCharacteristic::writeValue(uint8_t *data, size_t length, bool response) {
  log_d(">> writeValue(), length: %d", length);

  BLEClient *pClient = getRemoteService()->getClient();

  if (!pClient->isConnected()) {
    log_e("Disconnected");
    return false;
  }

  int rc = 0;
  int retryCount = 1;
  uint16_t mtu = ble_att_mtu(pClient->getConnId()) - 3;
  BLETaskData taskData(const_cast<BLERemoteCharacteristic *>(this));

  // Check if the data length is longer than we can write in one connection event.
  // If so we must do a long write which requires a response.
  if (length <= mtu && !response) {
    rc = ble_gattc_write_no_rsp_flat(pClient->getConnId(), m_handle, data, length);
    goto exit;
  }

  do {
    if (length > mtu) {
      log_i("long write %d bytes", length);
      os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
      rc = ble_gattc_write_long(pClient->getConnId(), m_handle, 0, om, BLERemoteCharacteristic::onWriteCB, &taskData);
    } else {
      rc = ble_gattc_write_flat(pClient->getConnId(), m_handle, data, length, BLERemoteCharacteristic::onWriteCB, &taskData);
    }
    if (rc != 0) {
      goto exit;
    }

    BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
    rc = taskData.m_flags;

    switch (rc) {
      case 0:
      case BLE_HS_EDONE: rc = 0; break;
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG):
        log_e("Long write not supported by peer; Truncating length to %d", mtu);
        retryCount++;
        length = mtu;
        break;

      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN):
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHOR):
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC):
        if (BLESecurity::m_securityEnabled && retryCount && pClient->secureConnection()) {
          break;
        }
      /* Else falls through. */
      default: goto exit;
    }
  } while (rc != 0 && retryCount--);

exit:
  if (rc != 0) {
    log_e("<< writeValue failed rc=%d, %s", rc, BLEUtils::returnCodeToString(rc));
  } else {
    log_d("<< writeValue success. length: %d rc=%d", length, rc);
  }

  return (rc == 0);
}  // writeValue

/**
 * @brief Subscribe or unsubscribe for notifications or indications.
 * @param [in] val 0x00 to unsubscribe, 0x01 for notifications, 0x02 for indications.
 * @param [in] notifyCallback A callback to be invoked for a notification.
 * @param [in] response If write response required set this to true.
 * If NULL is provided then no callback is performed.
 * @return false if writing to the descriptor failed.
 */
bool BLERemoteCharacteristic::setNotify(uint16_t val, notify_callback notifyCallback, bool response) {
  log_v(">> setNotify(): %s, %02x", toString().c_str(), val);

  m_notifyCallback = notifyCallback;

  // Retrieve descriptors if not already done (lazy loading)
  if (!m_descriptorsRetrieved) {
    log_d("Descriptors not yet retrieved, retrieving now...");
    if (!retrieveDescriptors()) {
      log_e("<< setNotify(): Failed to retrieve descriptors");
      return false;
    }
  }

  BLERemoteDescriptor *desc = getDescriptor(BLEUUID((uint16_t)0x2902));
  if (desc == nullptr) {
    log_w("<< setNotify(): Callback set, CCCD not found");
    return true;
  }

  log_d("<< setNotify()");

  response = true;  // Always write with response as per Bluetooth core specification.
  return desc->writeValue((uint8_t *)&val, 2, response);
}  // setNotify

/**
 * @brief Subscribe for notifications or indications.
 * @param [in] notifications If true, subscribe for notifications, false subscribe for indications.
 * @param [in] notifyCallback A callback to be invoked for a notification.
 * @param [in] response If true, require a write response from the descriptor write operation.
 * If NULL is provided then no callback is performed.
 * @return false if writing to the descriptor failed.
 */
bool BLERemoteCharacteristic::subscribe(bool notifications, notify_callback notifyCallback, bool response) {
  if (notifications) {
    return setNotify(0x01, notifyCallback, response);
  } else {
    return setNotify(0x02, notifyCallback, response);
  }
}  // subscribe

/**
 * @brief Unsubscribe for notifications or indications.
 * @param [in] response bool if true, require a write response from the descriptor write operation.
 * @return false if writing to the descriptor failed.
 */
bool BLERemoteCharacteristic::unsubscribe(bool response) {
  return setNotify(0x00, nullptr, response);
}  // unsubscribe

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
