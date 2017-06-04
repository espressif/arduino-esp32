#ifndef _BLEPERIPHERAL_H_
#define _BLEPERIPHERAL_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bt.h"
#include "BLECommon.h"

class BLEPeripheral {
 public:

  /**
     * Constructor for BLE Peripheral
     *
     * default constructor
     */
  BLEPeripheral(void);

  /**
   * Constructor for BLE Peripheral
   *
   * @param[in] user defined device local name
   */
  BLEPeripheral(String localName);
   
  /**
   * Destructor for BLE Peripheral
   *
   */
  ~BLEPeripheral(void);

  /**
     * Set the local name that the BLE Peripheral Device advertises
     *
     * @param[in] localName  local name to advertise
     *
     * @return none
     */
  void setLocalName(String localName);

   /**
     * Set the local name that the BLE Peripheral Device advertises along with the name type
     *
     * @param[in] nameType  type of local name shown in advertising data packet
     * @param[in] localName  local name to advertise. Choose from:
     *            BLE_ADVDATA_NO_NAME
     *            BLE_ADVDATA_SHORT_NAME
     *            BLE_ADVDATA_FULL_NAME
     *
     * @return none
     *
     * @note if the nameType is BLE_ADVDATA_NO_NAME, set localName to NULL
     */
  void setLocalName(ble_advdata_name_type_t nameType, String localName);

  /**
     * Set the company identifier of the BLE Peripheral Device
     *
     * @param[in] companyID  16-bit company identifier
     *
     * @return none
     *
     * @note check https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
     */
  void setCompanyID(const uint16_t companyID);

  /**
     * Set the manufacture specific data of the BLE Peripheral Device
     *
     * @param[in] data  a stream in uint8_t for manufacture specified data
     * @param[in] length  length of the manufacture specified data
     *
     * @return none
     *
     * @note manufacture specified data is the additional information other than company identifier
     */
  void setManufactureData(const uint8_t* data, const uint16_t length);

  /**
     * Set the advertising interval that the BLE Peripheral Device advertises
     *
     * @param[in] minInterval  minimum advertising interval in miliseconds
     * @param[in] maxInterval  maximum advertising interval in miliseconds
     *
     * @return none
     *
     * @note minInterval shall be less than or equal to maxInterval. The minInterval and maxInterval 
     *       should not be the same value to enable the controller to determine the best advertising interval
     *       given other activities.
     * @note minInterval and maxInterval shall not be set to less than 0x00A0 (100 ms) if the Advertising_Type
     *       is set to 0x02 (ADV_SCAN_IND) or 0x03 (ADV_NONCONN_IND).
     */
  void setAdvertisingInterval(const float  minInterval, const float  maxInterval);

  /**
     * Set the advertising interval that the BLE Peripheral Device advertises
     *
     * @param[in] interval  minimum advertising interval in miliseconds
     *
     * @return none
     *
     * @note this is the user specified advertising interval rather than optimized by the controller
     */
  void setAdvertisingInterval(const float interval);

  /**
     * Set the advertising type that the BLE Peripheral Device advertises
     *
     * @param[in] type  Advertising_Type. Choose from:
     *                  BLE_GAP_ADV_TYPE_ADV_IND       
     *                  BLE_GAP_ADV_TYPE_ADV_DIRECT_IND
     *                  BLE_GAP_ADV_TYPE_ADV_SCAN_IND
     *                  BLE_GAP_ADV_TYPE_ADV_NONCONN_IND
     *
     * @return none
     */
  void setAdvertisingType(const uint8_t type);

   /**
     * Set the peer device address along with the peer¡¯s identity address
     *
     * @param[in] addr_type  
     * @param[in] address
     *
     * @return none
     */
  void setPeerAddress(uint8_t addr_type, uint8_t address[BD_ADDR_LEN]);

  /**
     * Set the filter policy that the BLE Peripheral Device advertises
     *
     * @param[in] policy  gap filte policy encoding
     *
     * @return none
     *
     * @note Advertising_Filter_Policy parameter shall be ignored when directed advertising is enabled
     */
  void setAdvertisingFilterPolicy(const uint8_t policy);

  /**
     * Set the bluetooth advertising channels that the BLE Peripheral Device advertises
     *
     * @param[in] chn_map  channel map encoding.
     *
     * @return none
     *
     */
  void setAdvertisingChannelMap(const uint8_t chn_map);

  /**
     * Start BLE Advertising
     *
     * @param[in] none
     *
     * @return none
     *
     */
  bool begin(void);
  
  
 private:
  /*
   * Private helper methods
   */
  void hci_cmd_send_reset(void);
  void hci_cmd_send_ble_set_adv_param(void);
  void hci_cmd_send_ble_set_adv_data(void);
  void hci_cmd_send_ble_adv_start(void);
  uint16_t make_cmd_ble_set_adv_data(uint8_t *buf, uint8_t data_len, uint8_t *p_data);
  uint16_t make_cmd_reset(uint8_t *buf);
  uint16_t make_cmd_ble_set_adv_param (uint8_t *buf);
  uint16_t make_cmd_ble_set_adv_enable(uint8_t *buf, uint8_t adv_enable);

  void setConnectable(bool connectable);

  
 private:
  ble_gap_adv_params_t _ble_adv_param;
  ble_advdata_t _ble_adv_data;
  
};

#endif
