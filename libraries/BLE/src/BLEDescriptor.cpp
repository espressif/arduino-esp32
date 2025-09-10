/*
 * BLEDescriptor.cpp
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 *
 *  Modified on: Apr 3, 2025
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
#include <string.h>
#include <iomanip>
#include <stdlib.h>
#include "sdkconfig.h"
#include <esp_err.h>
#include "BLE2904.h"
#include "BLEService.h"
#include "BLEDescriptor.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

/***************************************************************************
 *                           Common definitions                            *
 ***************************************************************************/

#define NULL_HANDLE (0xffff)

/***************************************************************************
 *                         Common global variables                         *
 ***************************************************************************/

static BLEDescriptorCallbacks defaultCallbacks;

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

/**
 * @brief BLEDescriptor constructor.
 */
BLEDescriptor::BLEDescriptor(const char *uuid, uint16_t len) : BLEDescriptor(BLEUUID(uuid), len) {}

/**
 * @brief BLEDescriptor constructor.
 */
BLEDescriptor::BLEDescriptor(BLEUUID uuid, uint16_t max_len) {
  m_bleUUID = uuid;
  m_handle = NULL_HANDLE;                           // Handle is initially unknown.
  m_pCharacteristic = nullptr;                      // No initial characteristic.
  m_pCallback = nullptr;                            // No initial callback.
  m_value.attr_len = 0;                             // Initial length is 0.
  m_value.attr_max_len = max_len;                   // Maximum length of the data.
  m_value.attr_value = (uint8_t *)malloc(max_len);  // Allocate storage for the value.
#if CONFIG_NIMBLE_ENABLED
  m_removed = 0;
#endif
}  // BLEDescriptor

/**
 * @brief BLEDescriptor destructor.
 */
BLEDescriptor::~BLEDescriptor() {
  free(m_value.attr_value);  // Release the storage we created in the constructor.
}  // ~BLEDescriptor

/**
 * @brief Execute the creation of the descriptor with the BLE runtime in ESP.
 * @param [in] pCharacteristic The characteristic to which to register this descriptor.
 */
void BLEDescriptor::executeCreate(BLECharacteristic *pCharacteristic) {
  log_v(">> executeCreate(): %s", toString().c_str());

  if (m_handle != NULL_HANDLE) {
    log_e("Descriptor already has a handle.");
    return;
  }

  m_pCharacteristic = pCharacteristic;  // Save the characteristic associated with this service.

#if CONFIG_BLUEDROID_ENABLED
  esp_attr_control_t control;
  control.auto_rsp = ESP_GATT_AUTO_RSP;
  m_semaphoreCreateEvt.take("executeCreate");
  esp_err_t errRc =
    ::esp_ble_gatts_add_char_descr(pCharacteristic->getService()->getHandle(), getUUID().getNative(), (esp_gatt_perm_t)m_permissions, &m_value, &control);
  if (errRc != ESP_OK) {
    log_e("<< esp_ble_gatts_add_char_descr: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return;
  }

  m_semaphoreCreateEvt.wait("executeCreate");
#endif
  log_v("<< executeCreate");
}  // executeCreate

/**
 * @brief Get the BLE handle for this descriptor.
 * @return The handle for this descriptor.
 */
uint16_t BLEDescriptor::getHandle() const {
  return m_handle;
}  // getHandle

/**
 * @brief Get the length of the value of this descriptor.
 * @return The length (in bytes) of the value of this descriptor.
 */
size_t BLEDescriptor::getLength() const {
  return m_value.attr_len;
}  // getLength

/**
 * @brief Get the UUID of the descriptor.
 */
BLEUUID BLEDescriptor::getUUID() const {
  return m_bleUUID;
}  // getUUID

/**
 * @brief Get the value of this descriptor.
 * @return A pointer to the value of this descriptor.
 */
uint8_t *BLEDescriptor::getValue() const {
  return m_value.attr_value;
}  // getValue

/**
 * @brief Get the characteristic this descriptor belongs to.
 * @return A pointer to the characteristic this descriptor belongs to.
 */
BLECharacteristic *BLEDescriptor::getCharacteristic() const {
  return m_pCharacteristic;
}  // getCharacteristic

/**
 * @brief Set the callback handlers for this descriptor.
 * @param [in] pCallbacks An instance of a callback structure used to define any callbacks for the descriptor.
 */
void BLEDescriptor::setCallbacks(BLEDescriptorCallbacks *pCallback) {
  log_v(">> setCallbacks: 0x%x", (uint32_t)pCallback);
  if (pCallback != nullptr) {
    m_pCallback = pCallback;
  } else {
    m_pCallback = &defaultCallbacks;
  }
  log_v("<< setCallbacks");
}  // setCallbacks

/**
 * @brief Set the handle of this descriptor.
 * Set the handle of this descriptor to be the supplied value.
 * @param [in] handle The handle to be associated with this descriptor.
 * @return N/A.
 */
void BLEDescriptor::setHandle(uint16_t handle) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  log_v(">> setHandle(0x%.2x): Setting descriptor handle to be 0x%.2x", handle, handle);
  m_handle = handle;
  log_v("<< setHandle()");
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  log_w("NimBLE does not support manually setting the handle of a descriptor. Ignoring request.");
#endif
}  // setHandle

/**
 * @brief Set the value of the descriptor.
 * @param [in] data The data to set for the descriptor.
 * @param [in] length The length of the data in bytes.
 */
void BLEDescriptor::setValue(const uint8_t *data, size_t length) {
  if (length > m_value.attr_max_len) {
    log_e("Size %d too large, must be no bigger than %d", length, m_value.attr_max_len);
    return;
  }

  m_semaphoreSetValue.take();
  m_value.attr_len = length;
  memcpy(m_value.attr_value, data, length);
#if CONFIG_BLUEDROID_ENABLED
  if (m_handle != NULL_HANDLE) {
    esp_ble_gatts_set_attr_value(m_handle, length, (const uint8_t *)data);
    log_d("Set the value in the GATTS database using handle 0x%x", m_handle);
  }
#endif
  m_semaphoreSetValue.give();
}  // setValue

/**
 * @brief Set the value of the descriptor.
 * @param [in] value The value of the descriptor in string form.
 */
void BLEDescriptor::setValue(const String &value) {
  setValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}  // setValue

void BLEDescriptor::setAccessPermissions(uint16_t perm) {
  m_permissions = perm;
}

/**
 * @brief Return a string representation of the descriptor.
 * @return A string representation of the descriptor.
 */
String BLEDescriptor::toString() const {
  char hex[5];
  snprintf(hex, sizeof(hex), "%04x", m_handle);
  String res = "UUID: " + m_bleUUID.toString() + ", handle: 0x" + hex;
  return res;
}  // toString

BLEDescriptorCallbacks::~BLEDescriptorCallbacks() = default;

/**
 * @brief Callback function to support a read request.
 * @param [in] pDescriptor The descriptor that is the source of the event.
 */
void BLEDescriptorCallbacks::onRead(BLEDescriptor *pDescriptor) {
  log_d("BLEDescriptorCallbacks", ">> onRead: default");
  log_d("BLEDescriptorCallbacks", "<< onRead");
}  // onRead

/**
 * @brief Callback function to support a write request.
 * @param [in] pDescriptor The descriptor that is the source of the event.
 */
void BLEDescriptorCallbacks::onWrite(BLEDescriptor *pDescriptor) {
  log_d("BLEDescriptorCallbacks", ">> onWrite: default");
  log_d("BLEDescriptorCallbacks", "<< onWrite");
}  // onWrite

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief Handle GATT server events for the descripttor.
 * @param [in] event
 * @param [in] gatts_if
 * @param [in] param
 */
void BLEDescriptor::handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    // ESP_GATTS_ADD_CHAR_DESCR_EVT
    //
    // add_char_descr:
    // - esp_gatt_status_t status
    // - uint16_t          attr_handle
    // - uint16_t          service_handle
    // - esp_bt_uuid_t     char_uuid
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
      if (m_pCharacteristic != nullptr && m_bleUUID.equals(BLEUUID(param->add_char_descr.descr_uuid))
          && m_pCharacteristic->getService()->getHandle() == param->add_char_descr.service_handle
          && m_pCharacteristic == m_pCharacteristic->getService()->getLastCreatedCharacteristic()) {
        setHandle(param->add_char_descr.attr_handle);
        m_semaphoreCreateEvt.give();
      }
      break;
    }  // ESP_GATTS_ADD_CHAR_DESCR_EVT

    // ESP_GATTS_WRITE_EVT - A request to write the value of a descriptor has arrived.
    //
    // write:
    // - uint16_t conn_id
    // - uint16_t trans_id
    // - esp_bd_addr_t bda
    // - uint16_t handle
    // - uint16_t offset
    // - bool need_rsp
    // - bool is_prep
    // - uint16_t len
    // - uint8_t *value
    case ESP_GATTS_WRITE_EVT:
    {
      if (param->write.handle == m_handle) {
        setValue(param->write.value, param->write.len);  // Set the value of the descriptor.

        if (m_pCallback != nullptr) {  // We have completed the write, if there is a user supplied callback handler, invoke it now.
          m_pCallback->onWrite(this);  // Invoke the onWrite callback handler.
        }
      }  // End of ... this is our handle.

      break;
    }  // ESP_GATTS_WRITE_EVT

    // ESP_GATTS_READ_EVT - A request to read the value of a descriptor has arrived.
    //
    // read:
    // - uint16_t conn_id
    // - uint32_t trans_id
    // - esp_bd_addr_t bda
    // - uint16_t handle
    // - uint16_t offset
    // - bool is_long
    // - bool need_rsp
    //
    case ESP_GATTS_READ_EVT:
    {
      if (param->read.handle == m_handle) {  // If this event is for this descriptor ... process it

        if (m_pCallback != nullptr) {  // If we have a user supplied callback, invoke it now.
          m_pCallback->onRead(this);   // Invoke the onRead callback method in the callback handler.
        }

      }  // End of this is our handle
      break;
    }  // ESP_GATTS_READ_EVT

    default: break;
  }  // switch event
}  // handleGATTServerEvent

#endif

/***************************************************************************
 *                           NimBLE functions                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief Handle GATT server events for the descriptor.
 * @param [in] conn_handle The connection handle.
 * @param [in] attr_handle The attribute handle.
 * @param [in] ctxt The GATT access context.
 * @param [in] arg The argument.
 */
int BLEDescriptor::handleGATTServerEvent(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  const ble_uuid_t *uuid;
  int rc;
  struct ble_gap_conn_desc desc;
  BLEDescriptor *pDescriptor = (BLEDescriptor *)arg;

  log_d("Descriptor %s %s event", pDescriptor->getUUID().toString().c_str(), ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC ? "Read" : "Write");

  uuid = ctxt->chr->uuid;
  if (ble_uuid_cmp(uuid, &pDescriptor->getUUID().getNative()->u) == 0) {
    switch (ctxt->op) {
      case BLE_GATT_ACCESS_OP_READ_DSC:
      {
        rc = ble_gap_conn_find(conn_handle, &desc);
        assert(rc == 0);

        // If the packet header is only 8 bytes this is a follow up of a long read
        // so we don't want to call the onRead() callback again.
        if (ctxt->om->om_pkthdr_len > 8 || pDescriptor->m_value.attr_len <= (ble_att_mtu(desc.conn_handle) - 3)) {
          pDescriptor->m_pCallback->onRead(pDescriptor);
        }

        ble_npl_hw_enter_critical();
        rc = os_mbuf_append(ctxt->om, pDescriptor->m_value.attr_value, pDescriptor->m_value.attr_len);
        ble_npl_hw_exit_critical(0);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }

      case BLE_GATT_ACCESS_OP_WRITE_DSC:
      {
        uint16_t att_max_len = pDescriptor->m_value.attr_max_len;

        if (ctxt->om->om_len > att_max_len) {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t buf[att_max_len];
        size_t len = ctxt->om->om_len;
        memcpy(buf, ctxt->om->om_data, len);
        os_mbuf *next;
        next = SLIST_NEXT(ctxt->om, om_next);
        while (next != NULL) {
          if ((len + next->om_len) > att_max_len) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
          }
          memcpy(&buf[len], next->om_data, next->om_len);
          len += next->om_len;
          next = SLIST_NEXT(next, om_next);
        }

        pDescriptor->setValue(buf, len);
        pDescriptor->m_pCallback->onWrite(pDescriptor);
        return 0;
      }

      default: break;
    }
  }

  return BLE_ATT_ERR_UNLIKELY;
}

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
