#ifndef _BT_HCI_H_
#define _BT_HCI_H_

#include <cstdint>
#include "Arduino.h"

/*  HCI Command opcode group field(OGF) */
#define HCI_GRP_LINK_CTRL_CMDS		   (0x01 << 10)
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS    (0x03 << 10)            /* 0x0C00 */
#define HCI_GRP_INFO_CMDS		   (0x04 << 10)
#define HCI_GRP_BLE_CMDS                   (0x08 << 10)

/*  HCI Command opcode command field(OCF) */
#define HCI_RESET                          (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_BLE_WRITE_ADV_ENABLE           (0x000A | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_PARAMS           (0x0006 | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_ADV_DATA             (0x0008 | HCI_GRP_BLE_CMDS)

#define HCI_BLE_ADV_DISABLE			0x00
#define HCI_BLE_ADV_ENABLE			0x01

#define HCI_H4_CMD_PREAMBLE_SIZE                (4)
#define HCIC_PARAM_SIZE_WRITE_ADV_ENABLE        (1)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS    (15)
#define HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA      (31)
#define BD_ADDR_LEN                             (6) /* Device address length */

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
#define 	BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE         (0x01)
#define 	BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE         (0x02)
#define 	BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED         (0x04) 
#define 	BLE_GAP_ADV_FLAG_LE_BR_EDR_CONTROLLER         (0x08)
#define 	BLE_GAP_ADV_FLAG_LE_BR_EDR_HOST               (0x10)
#define 	BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE   (BLE_GAP_ADV_FLAG_LE_LIMITED_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)
#define 	BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE   (BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED)

/* Advertising types */
#define 	BLE_GAP_ADV_TYPE_ADV_IND                   0x00
#define 	BLE_GAP_ADV_TYPE_ADV_DIRECT_IND            0x01
#define 	BLE_GAP_ADV_TYPE_ADV_SCAN_IND              0x02
#define 	BLE_GAP_ADV_TYPE_ADV_NONCONN_IND           0x03
#define         BLE_GAP_ADV_TYPE_ADV_DIRECT_IND_LOW_DUTY   0x04

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
#define         BLE_GAP_ADVCHAN_37          0x01
#define         BLE_GAP_ADVCHAN_38          0x02
#define         BLE_GAP_ADVCHAN_39          0x04
#define         BLE_GAP_ADVCHAN_ALL         BLE_GAP_ADVCHAN_37 | BLE_GAP_ADVCHAN_38 | BLE_GAP_ADVCHAN_39

#define MSEC_TO_UNITS(TIME, RESOLUTION)	 (((TIME) * 1000) / (RESOLUTION))
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#define BDADDR_TO_STREAM(p, a)   {int ijk; for (ijk = 0; ijk < BD_ADDR_LEN;  ijk++) *(p)++ = (uint8_t) a[BD_ADDR_LEN - 1 - ijk];}
#define ARRAY_TO_STREAM(p, a, len)  {int ijk; for (ijk = 0; ijk < len;        ijk++) *(p)++ = (uint8_t) a[ijk];}


/* HCI H4 message type definitions */
enum {
  H4_TYPE_COMMAND = 1,
  H4_TYPE_ACL     = 2,
  H4_TYPE_SCO     = 3,
  H4_TYPE_EVENT   = 4
};

/* Advertising data name type */
enum ble_advdata_name_type_t { 
  BLE_ADVDATA_NO_NAME,  // include no device name in advertising data
  BLE_ADVDATA_SHORT_NAME,  // include short device name in advertising data
  BLE_ADVDATA_FULL_NAME  // include full device name in advertising data
};

enum { 
  UNIT_0_625_MS = 625,  /* Number of microseconds in 0.625 milliseconds. */ 
  UNIT_1_25_MS = 1250,  /* Number of microseconds in 1.25 milliseconds. */
  UNIT_10_MS = 10000  /* Number of microseconds in 10 milliseconds. */
};


/* BLE GAP address struct */
struct ble_gap_addr {
  uint8_t addr_type;
  uint8_t addr[BD_ADDR_LEN];
};

/* BLE GAP Identify Resolving Key struct */
struct ble_gap_irk {
  uint8_t irk[16];
};

/* BLE GAP Whitelist struct */
struct ble_gap_whitelist {
  struct ble_gap_addr** pp_addrs;  // pointer to array of device address pointers, pointing to addresses to be used in whitelist. NULL is none are given
  uint8_t addr_count;  // count of device addresses in array
  struct ble_gap_irk** pp_irks;  // pointer to array of identify resolving key (irk) pointers, each pointing to an IRK in the whitelist. NULL if nono are given
  uint8_t irk_count;  // count of IRKs in array
};

/* BLE UUID struct */
struct ble_uuid {
  uint16_t uuid;  // 16-bit UUID value or octets 12-13 of 128-bit UUID
  uint8_t type;  // UUID type
};


/* BLE UUID List struct */
struct ble_advdata_uuid_list {
  uint16_t uuid_cnt;  // number of UUID entries
  struct ble_uuid* p_uuids;
};


/* BLE Advertising parameters struct */
struct ble_gap_adv_params {
  uint8_t type;
  uint8_t own_addr_type;
  struct ble_gap_addr* p_peer_addr;
  uint8_t fp;  // filter policy
  struct ble_gap_whitelist* p_whitelist;
  uint16_t interval_min;  // minimum advertising interval between 0x0020 and 0x4000 in 0.625 ms units (20ms to 10.24s)
  uint16_t interval_max;
  uint8_t chn_map;
  uint16_t timeout;  // advertising timeout between 0x0001 and 0x3FFF in seconds, 0x0000 disables timeout.
};

struct uint8_array {
  uint16_t size;
  uint8_t* p_data;
};

/* BLE Connection Interval struct */
struct ble_advdata_conn_int {
  uint16_t min_conn_interval;  // minimum Connection Interval, in units of 1.25ms, range 6 to 3200 (i.e. 7.5ms to 4s).
  uint16_t max_conn_interval;  // maximum Connection Interval, in units of 1.25ms, range 6 to 3200 (i.e. 7.5ms to 4s). Value of 0xFFFF indicates no specific maximum.
};

/* BLE Manufacture Company Identifier and Manufacture Specific Data struct */
struct ble_advdata_manuf_data {
  uint16_t company_identifier;
  struct uint8_array data;
};

/* BLE service data strcut */
struct ble_advdata_service_data {
  uint16_t service_uuid;
  struct uint8_array data;
};

/* BLE advertising data encoding*/
struct ble_advdata {
  ble_advdata_name_type_t name_type;
  String local_name;
  bool include_appearance;  // determines if appearance shall be included
  struct uint8_array flags;  // advertising data flags field
  int8_t* p_tx_power_level;  // TX power level field
  struct ble_advdata_uuid_list  uuids_more_available;  // list of UUIDs in the 'More Available' list
  struct ble_advdata_uuid_list  uuids_complete;  // list of UUIDs in the 'Complete' list
  struct ble_advdata_uuid_list  uuids_solicited;  // list of solcited UUIDs
  struct ble_advdata_conn_int*  p_slave_conn_int;  // slave connection interval range
  struct ble_advdata_manuf_data*  p_manuf_specific_data;  // manufacture specific data
  struct ble_advdata_service_data*  p_service_data_array;  // array of service data structures
  uint8_t  service_data_count;  // number of service data structures
};

#endif
