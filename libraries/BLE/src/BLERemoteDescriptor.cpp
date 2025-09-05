/*
 * BLERemoteDescriptor.cpp
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

#include "WString.h"
#include <sstream>
#include "BLERemoteDescriptor.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <freertos/task.h>
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include <host/ble_hs.h>
#endif

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

BLERemoteDescriptor::BLERemoteDescriptor(uint16_t handle, BLEUUID uuid, BLERemoteCharacteristic *pRemoteCharacteristic) {
  m_handle = handle;
  m_uuid = uuid;
  m_pRemoteCharacteristic = pRemoteCharacteristic;
  m_auth = 0;
}

/**
 * @brief Retrieve the handle associated with this remote descriptor.
 * @return The handle associated with this remote descriptor.
 */
uint16_t BLERemoteDescriptor::getHandle() {
  return m_handle;
}  // getHandle

/**
 * @brief Get the characteristic that owns this descriptor.
 * @return The characteristic that owns this descriptor.
 */
BLERemoteCharacteristic *BLERemoteDescriptor::getRemoteCharacteristic() {
  return m_pRemoteCharacteristic;
}  // getRemoteCharacteristic

/**
 * @brief Retrieve the UUID associated this remote descriptor.
 * @return The UUID associated this remote descriptor.
 */
BLEUUID BLERemoteDescriptor::getUUID() {
  return m_uuid;
}  // getUUID

uint8_t BLERemoteDescriptor::readUInt8() {
  String value = readValue();
  if (value.length() >= 1) {
    return (uint8_t)value[0];
  }
  return 0;
}  // readUInt8

uint16_t BLERemoteDescriptor::readUInt16() {
  String value = readValue();
  if (value.length() >= 2) {
    return *(uint16_t *)value.c_str();
  }
  return 0;
}  // readUInt16

uint32_t BLERemoteDescriptor::readUInt32() {
  String value = readValue();
  if (value.length() >= 4) {
    return *(uint32_t *)value.c_str();
  }
  return 0;
}  // readUInt32

/**
 * @brief Return a string representation of this BLE Remote Descriptor.
 * @return A string representation of this BLE Remote Descriptor.
 */
String BLERemoteDescriptor::toString() {
  char val[6];
  snprintf(val, sizeof(val), "%d", getHandle());
  String res = "handle: ";
  res += val;
  res += ", uuid: " + getUUID().toString();
  return res;
}  // toString

/**
 * @brief Write data represented as a string to the BLE Remote Descriptor.
 * @param [in] newValue The data to send to the remote descriptor.
 * @param [in] response True if we expect a response.
 */
bool BLERemoteDescriptor::writeValue(String newValue, bool response) {
  return writeValue((uint8_t *)newValue.c_str(), newValue.length(), response);
}  // writeValue

/**
 * @brief Write a byte value to the Descriptor.
 * @param [in] The single byte to write.
 * @param [in] True if we expect a response.
 */
bool BLERemoteDescriptor::writeValue(uint8_t newValue, bool response) {
  return writeValue(&newValue, 1, response);
}  // writeValue

/**
 * @brief Set authentication request type for characteristic
 * @param [in] auth Authentication request type.
 */
void BLERemoteDescriptor::setAuth(uint8_t auth) {
  m_auth = auth;
}

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

void BLERemoteDescriptor::gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam) {
  switch (event) {
    // ESP_GATTC_READ_DESCR_EVT
    // This event indicates that the server has responded to the read request.
    //
    // read:
    // - esp_gatt_status_t  status
    // - uint16_t           conn_id
    // - uint16_t           handle
    // - uint8_t*           value
    // - uint16_t           value_len
    case ESP_GATTC_READ_DESCR_EVT:
      // If this event is not for us, then nothing further to do.
      if (evtParam->read.handle != getHandle()) {
        break;
      }
      // At this point, we have determined that the event is for us, so now we save the value
      if (evtParam->read.status == ESP_GATT_OK) {
        // it will read the cached value of the descriptor
        m_value = String((char *)evtParam->read.value, evtParam->read.value_len);
      } else {
        m_value = "";
      }
      // Unlock the semaphore to ensure that the requester of the data can continue.
      m_semaphoreReadDescrEvt.give();
      break;

    case ESP_GATTC_WRITE_DESCR_EVT:
      if (evtParam->write.handle != getHandle()) {
        break;
      }
      m_semaphoreWriteDescrEvt.give();
      break;
    default: break;
  }
}

String BLERemoteDescriptor::readValue() {
  log_v(">> readValue: %s", toString().c_str());

  // Check to see that we are connected.
  if (!getRemoteCharacteristic()->getRemoteService()->getClient()->isConnected()) {
    log_e("Disconnected");
    return String();
  }

  m_semaphoreReadDescrEvt.take("readValue");

  // Ask the BLE subsystem to retrieve the value for the remote hosted characteristic.
  esp_err_t errRc = ::esp_ble_gattc_read_char_descr(
    m_pRemoteCharacteristic->getRemoteService()->getClient()->getGattcIf(),
    m_pRemoteCharacteristic->getRemoteService()->getClient()->getConnId(),  // The connection ID to the BLE server
    getHandle(),                                                            // The handle of this characteristic
    (esp_gatt_auth_req_t)m_auth
  );  // Security

  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_read_char: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return "";
  }

  // Block waiting for the event that indicates that the read has completed.  When it has, the String found
  // in m_value will contain our data.
  m_semaphoreReadDescrEvt.wait("readValue");

  log_v("<< readValue(): length: %d", m_value.length());
  return m_value;
}  // readValue

/**
 * @brief Write data to the BLE Remote Descriptor.
 * @param [in] data The data to send to the remote descriptor.
 * @param [in] length The length of the data to send.
 * @param [in] response True if we expect a response.
 * @return True if successful
 */
bool BLERemoteDescriptor::writeValue(uint8_t *data, size_t length, bool response) {
  log_v(">> writeValue: %s", toString().c_str());
  // Check to see that we are connected.
  if (!getRemoteCharacteristic()->getRemoteService()->getClient()->isConnected()) {
    log_e("Disconnected");
    return false;
  }

  m_semaphoreWriteDescrEvt.take("writeValue");

  esp_err_t errRc = ::esp_ble_gattc_write_char_descr(
    m_pRemoteCharacteristic->getRemoteService()->getClient()->getGattcIf(), m_pRemoteCharacteristic->getRemoteService()->getClient()->getConnId(), getHandle(),
    length,  // Data length
    data,    // Data
    response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP, (esp_gatt_auth_req_t)m_auth
  );
  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_write_char_descr: %d", errRc);
  }

  m_semaphoreWriteDescrEvt.wait("writeValue");
  log_v("<< writeValue");
  return (errRc == ESP_OK);
}  // writeValue

#endif

/***************************************************************************
 *                           NimBLE functions                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief Remote descriptor constructor.
 * @param [in] pRemoteCharacteristic A pointer to the Characteristic that this belongs to.
 * @param [in] dsc A pointer to the struct that contains the descriptor information.
 */
BLERemoteDescriptor::BLERemoteDescriptor(BLERemoteCharacteristic *pRemoteCharacteristic, const struct ble_gatt_dsc *dsc) {
  log_d(">> BLERemoteDescriptor()");
  switch (dsc->uuid.u.type) {
    case BLE_UUID_TYPE_16:  m_uuid = BLEUUID(dsc->uuid.u16.value); break;
    case BLE_UUID_TYPE_32:  m_uuid = BLEUUID(dsc->uuid.u32.value); break;
    case BLE_UUID_TYPE_128: m_uuid = BLEUUID(const_cast<ble_uuid128_t *>(&dsc->uuid.u128)); break;
    default:                break;
  }

  m_handle = dsc->handle;
  m_pRemoteCharacteristic = pRemoteCharacteristic;
  m_auth = 0;

  log_d("<< BLERemoteDescriptor(): %s", m_uuid.toString().c_str());
}

/**
 * @brief Read the value of the remote descriptor.
 * @return The value of the remote descriptor.
 */
String BLERemoteDescriptor::readValue() {
  log_d(">> Descriptor readValue: %s", toString().c_str());

  BLEClient *pClient = getRemoteCharacteristic()->getRemoteService()->getClient();
  String value{};

  if (!pClient->isConnected()) {
    log_e("Disconnected");
    return value;
  }

  int rc = 0;
  int retryCount = 1;
  BLETaskData taskData(const_cast<BLERemoteDescriptor *>(this), 0, &value);

  do {
    rc = ble_gattc_read_long(pClient->getConnId(), m_handle, 0, BLERemoteDescriptor::onReadCB, &taskData);
    if (rc != 0) {
      goto exit;
    }

    BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
    rc = taskData.m_flags;

    switch (rc) {
      case 0:
      case BLE_HS_EDONE: rc = 0; break;
      // Descriptor is not long-readable, return with what we have.
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG):
        log_i("Attribute not long");
        rc = ble_gattc_read(pClient->getConnId(), m_handle, BLERemoteDescriptor::onReadCB, &taskData);
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

  m_semaphoreReadDescrEvt.take("readValue");
  m_value = value;
  m_semaphoreReadDescrEvt.give();

exit:
  if (rc != 0) {
    log_e("<< readValue failed rc=%d, %s", rc, BLEUtils::returnCodeToString(rc));
  } else {
    log_d("<< Descriptor readValue(): length: %d rc=%d", value.length(), rc);
  }

  return value;
}  // readValue

/**
 * @brief Callback for Descriptor read operation.
 * @return success == 0 or error code.
 */
int BLERemoteDescriptor::onReadCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  BLETaskData *pTaskData = static_cast<BLETaskData *>(arg);
  BLERemoteDescriptor *desc = static_cast<BLERemoteDescriptor *>(pTaskData->m_pInstance);
  uint16_t conn_id = desc->getRemoteCharacteristic()->getRemoteService()->getClient()->getConnId();

  if (error->status == BLE_HS_ENOTCONN) {
    log_e("<< Descriptor Read; Not connected");
    BLEUtils::taskRelease(*pTaskData, error->status);
    return error->status;
  }

  if (conn_id != conn_handle) {
    return 0;
  }

  log_d("Read complete; status=%d conn_handle=%d", error->status, conn_handle);

  String *strBuf = static_cast<String *>(pTaskData->m_pBuf);
  int rc = error->status;

  if (rc == 0) {
    if (attr) {
      uint32_t data_len = OS_MBUF_PKTLEN(attr->om);
      if (((*strBuf).length() + data_len) > BLE_ATT_ATTR_MAX_LEN) {
        rc = BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      } else {
        log_d("Got %d bytes", data_len);
        (*strBuf) += String((char *)attr->om->om_data, data_len);
        return 0;
      }
    }
  }

  BLEUtils::taskRelease(*pTaskData, rc);
  return rc;
}

/**
 * @brief Callback for descriptor write operation.
 * @return success == 0 or error code.
 */
int BLERemoteDescriptor::onWriteCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  BLETaskData *pTaskData = static_cast<BLETaskData *>(arg);
  BLERemoteDescriptor *descriptor = static_cast<BLERemoteDescriptor *>(pTaskData->m_pInstance);
  int rc = error->status;

  if (rc == BLE_HS_ENOTCONN) {
    log_e("<< Descriptor Write; Not connected");
    BLEUtils::taskRelease(*pTaskData, rc);
    return rc;
  }

  if (descriptor->getRemoteCharacteristic()->getRemoteService()->getClient()->getConnId() != conn_handle) {
    return 0;
  }

  log_i("Write complete; status=%d conn_handle=%d", rc, conn_handle);

  BLEUtils::taskRelease(*pTaskData, rc);
  return 0;
}

/**
 * @brief Write data to the BLE Remote Descriptor.
 * @param [in] data The data to send to the remote descriptor.
 * @param [in] length The length of the data to send.
 * @param [in] response True if we expect a write response.
 * @return True if successful
 */
bool BLERemoteDescriptor::writeValue(uint8_t *data, size_t length, bool response) {
  log_d(">> Descriptor writeValue: %s", toString().c_str());

  BLEClient *pClient = getRemoteCharacteristic()->getRemoteService()->getClient();

  // Check to see that we are connected.
  if (!pClient->isConnected()) {
    log_e("Disconnected");
    return false;
  }

  int rc = 0;
  int retryCount = 1;
  uint16_t mtu = ble_att_mtu(pClient->getConnId()) - 3;
  BLETaskData taskData(const_cast<BLERemoteDescriptor *>(this));

  // Check if the data length is longer than we can write in 1 connection event.
  // If so we must do a long write which requires a response.
  if (length <= mtu && !response) {
    rc = ble_gattc_write_no_rsp_flat(pClient->getConnId(), m_handle, data, length);
    goto exit;
  }

  do {
    if (length > mtu) {
      log_i("long write %d bytes", length);
      os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
      rc = ble_gattc_write_long(pClient->getConnId(), m_handle, 0, om, BLERemoteDescriptor::onWriteCB, &taskData);
    } else {
      rc = ble_gattc_write_flat(pClient->getConnId(), m_handle, data, length, BLERemoteDescriptor::onWriteCB, &taskData);
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

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
