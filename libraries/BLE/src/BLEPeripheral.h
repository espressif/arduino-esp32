#ifndef _BLEPERIPHERAL_H_
#define _BLEPERIPHERAL_H_

#include <cstdint>
#include <Arduino.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bt.h"

/*  HCI Command opcode group field(OGF) */
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS    (0x03 << 10)            /* 0x0C00 */
#define HCI_GRP_BLE_CMDS                   (0x08 << 10)

/*  HCI Command opcode command field(OCF) */
#define HCI_RESET                          (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_BLE_WRITE_ADV_ENABLE           (0x000A | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_PARAMS           (0x0006 | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_DATA             (0x0008 | HCI_GRP_BLE_CMDS)

#define HCI_H4_CMD_PREAMBLE_SIZE                (4)
#define HCIC_PARAM_SIZE_WRITE_ADV_ENABLE        (1)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS    (15)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA      (31)


/* Advertising types */
#define 	BLE_GAP_ADV_TYPE_ADV_IND         0x00
#define 	BLE_GAP_ADV_TYPE_ADV_DIRECT_IND  0x01
#define 	BLE_GAP_ADV_TYPE_ADV_SCAN_IND    0x02
#define 	BLE_GAP_ADV_TYPE_ADV_NONCONN_IND 0x03


/* EIR/AD data type definitions */
#define BT_DATA_FLAGS			0x01 /* AD flags */
#define BT_DATA_UUID16_SOME		0x02 /* 16-bit UUID, more available */
#define BT_DATA_UUID16_ALL		0x03 /* 16-bit UUID, all listed */
#define BT_DATA_UUID32_SOME		0x04 /* 32-bit UUID, more available */
#define BT_DATA_UUID32_ALL		0x05 /* 32-bit UUID, all listed */
#define BT_DATA_UUID128_SOME		0x06 /* 128-bit UUID, more available */
#define BT_DATA_UUID128_ALL		0x07 /* 128-bit UUID, all listed */
#define BT_DATA_NAME_SHORTENED		0x08 /* Shortened name */
#define BT_DATA_NAME_COMPLETE		0x09 /* Complete name */
#define BT_DATA_TX_POWER		0x0a /* Tx Power */
#define BT_DATA_SOLICIT16		0x14 /* Solicit UUIDs, 16-bit */
#define BT_DATA_SOLICIT128		0x15 /* Solicit UUIDs, 128-bit */
#define BT_DATA_SVC_DATA16		0x16 /* Service data, 16-bit UUID */
#define BT_DATA_GAP_APPEARANCE		0x19 /* GAP appearance */
#define BT_DATA_SOLICIT32		0x1f /* Solicit UUIDs, 32-bit */
#define BT_DATA_SVC_DATA32		0x20 /* Service data, 32-bit UUID */
#define BT_DATA_SVC_DATA128		0x21 /* Service data, 128-bit UUID */
#define BT_DATA_MANUFACTURER_DATA	0xff /* Manufacturer Specific Data */


/* Advertising Discovery Flags */
#define 	BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE   (0x01)
#define 	BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE   (0x02)
#define 	BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED   (0x04) 
#define 	BLE_GAP_ADV_FLAG_LE_BR_EDR_CONTROLLER   (0x08)
#define 	BLE_GAP_ADV_FLAG_LE_BR_EDR_HOST   (0x10)
#define 	BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE   (BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)
#define 	BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE   (BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)


/* Advertising Filter Policies */
#define 	BLE_GAP_ADV_FP_ANY              0x00
#define 	BLE_GAP_ADV_FP_FILTER_SCANREQ   0x01
#define 	BLE_GAP_ADV_FP_FILTER_CONNREQ   0x02
#define 	BLE_GAP_ADV_FP_FILTER_BOTH      0x03


/* Advertising Device Address Types */
#define 	BLE_GAP_ADDR_TYPE_PUBLIC                          0x00
#define 	BLE_GAP_ADDR_TYPE_RANDOM_STATIC                   0x01
#define 	BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE       0x02
#define 	BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE   0x03


/* GAP Advertising Channel Maps */
#define         GAP_ADVCHAN_37          0x01
#define         GAP_ADVCHAN_38          0x02
#define         GAP_ADVCHAN_39          0x04
#define         GAP_ADVCHAN_ALL         GAP_ADVCHAN_37 | GAP_ADVCHAN_38 | GAP_ADVCHAN_39


/* GAP Filter Policies */
#define 	BLE_GAP_ADV_FP_ANY              0x00
#define 	BLE_GAP_ADV_FP_FILTER_SCANREQ   0x01
#define 	BLE_GAP_ADV_FP_FILTER_CONNREQ   0x02
#define 	BLE_GAP_ADV_FP_FILTER_BOTH      0x03




#define BD_ADDR_LEN     (6)                     /* Device address length */


#define MSEC_TO_UNITS(TIME, RESOLUTION)	 (((TIME) * 1000) / (RESOLUTION))
#define UINT16_TO_STREAM(p, u16)  {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#define BDADDR_TO_STREAM(p, a)   {int ijk; for (ijk = 0; ijk < BD_ADDR_LEN;  ijk++) *(p)++ = (uint8_t) a[BD_ADDR_LEN - 1 - ijk];}
#define ARRAY_TO_STREAM(p, a, len)  {int ijk; for (ijk = 0; ijk < len;        ijk++) *(p)++ = (uint8_t) a[ijk];}


/* Advertising data name type */
enum ble_advdata_name_type_t { 
  BLE_ADVDATA_NO_NAME,  // include no device name in advertising data
  BLE_ADVDATA_SHORT_NAME,  // include short device name in advertising data
  BLE_ADVDATA_FULL_NAME  // include full device name in advertising data
};

/* HCI H4 message type definitions */
enum {
  H4_TYPE_COMMAND = 1,
  H4_TYPE_ACL     = 2,
  H4_TYPE_SCO     = 3,
  H4_TYPE_EVENT   = 4
};

enum { 
  UNIT_0_625_MS = 625,  /* Number of microseconds in 0.625 milliseconds. */ 
  UNIT_1_25_MS = 1250,  /* Number of microseconds in 1.25 milliseconds. */
  UNIT_10_MS = 10000  /* Number of microseconds in 10 milliseconds. */
};


/* BLE GAP address struct */
typedef struct {
  uint8_t addr_type;
  uint8_t addr[BD_ADDR_LEN];
}ble_gap_addr_t;

/* BLE GAP Identify Resolving Key struct */
typedef struct {
  uint8_t irk[16];
}ble_gap_irk_t;

/* BLE GAP Whitelist struct */
typedef struct {
  ble_gap_addr_t** pp_addrs;  // pointer to array of device address pointers, pointing to addresses to be used in whitelist. NULL is none are given
  uint8_t addr_count;  // count of device addresses in array
  ble_gap_irk_t** pp_irks;  // pointer to array of identify resolving key (irk) pointers, each pointing to an IRK in the whitelist. NULL if nono are given
  uint8_t irk_count;  // count of IRKs in array
}ble_gap_whitelist_t;

/* BLE Advertising parameters struct */
typedef struct {
  uint8_t type;
  uint8_t own_addr_type;
  ble_gap_addr_t* p_peer_addr;
  uint8_t fp;  // filter policy
  ble_gap_whitelist_t* p_whitelist;
  uint16_t interval_min;  // minimum advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20ms to 10.24s)
  uint16_t interval_max;
  uint8_t chn_map;
  uint16_t timeout;  // advertising timeout between 0x0001 and 0x3FFF in seconds, 0x0000 disables timeout.
}ble_gap_adv_params_t;

/* BLE UUID struct */
typedef struct {
  uint16_t uuid;  // 16-bit UUID value or octets 12-13 of 128-bit UUID
  uint8_t type;  // UUID type
}ble_uuid_t;

typedef struct {
  uint16_t size;
  uint8_t* p_data;
}uint8_array_t;

/* BLE UUID List struct */
typedef struct {
  uint16_t uuid_cnt;  // number of UUID entries
  ble_uuid_t* p_uuids;
}ble_advdata_uuid_list_t;

/* BLE Connection Interval struct */
typedef struct {
  uint16_t min_conn_interval;  // minimum Connection Interval, in units of 1.25ms, range 6 to 3200 (i.e. 7.5ms to 4s).
  uint16_t max_conn_interval;  // maximum Connection Interval, in units of 1.25ms, range 6 to 3200 (i.e. 7.5ms to 4s). Value of 0xFFFF indicates no specific maximum.
}ble_advdata_conn_int_t;

/* BLE Manufacture Company Identifier and Manufacture Specific Data struct */
typedef struct {
  uint16_t company_identifier;
  uint8_array_t data;
}ble_advdata_manuf_data_t;

/* BLE service data strcut */
typedef struct {
  uint16_t service_uuid;
  uint8_array_t data;
}ble_advdata_service_data_t;

/* BLE advertising data encoding*/
typedef struct {
  ble_advdata_name_type_t name_type;
  String local_name;
  bool include_appearance;  // determines if appearance shall be included
  uint8_array_t flags;  // advertising data flags field
  int8_t* p_tx_power_level;  // TX power level field
  ble_advdata_uuid_list_t  uuids_more_available;  // list of UUIDs in the 'More Available' list
  ble_advdata_uuid_list_t  uuids_complete;  // list of UUIDs in the 'Complete' list
  ble_advdata_uuid_list_t  uuids_solicited;  // list of solcited UUIDs
  ble_advdata_conn_int_t*  p_slave_conn_int;  // slave connection interval range
  ble_advdata_manuf_data_t*  p_manuf_specific_data;  // manufacture specific data
  ble_advdata_service_data_t*  p_service_data_array;  // array of service data structures
  uint8_t  service_data_count;  // number of service data structures
}ble_advdata_t;




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
