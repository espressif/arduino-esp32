#include "BLEPeripheral.h"

BLEPeripheral::BLEPeripheral(String localName)
{
  // Always clear all fields in structs or you might get unexpected behaviour
  memset(&_ble_adv_param, 0x00, sizeof(_ble_adv_param));
  memset(&_ble_adv_data, 0x00, sizeof(_ble_adv_data));

  // Default Advertising parameter
  setConnectable(true);
  setLocalName(localName);
  uint8_t peerAddr[BD_ADDR_LEN] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85};
  setPeerAddress(BLE_GAP_ADDR_TYPE_PUBLIC, peerAddr);
  setAdvertisingInterval(80);
  setAdvertisingChannelMap(BLE_GAP_ADVCHAN_ALL);  // 37, 38, 39 channels 
}

BLEPeripheral::BLEPeripheral(void):BLEPeripheral("MY_BLUETOOTH")
{
}

BLEPeripheral::~BLEPeripheral(void)
{
  if (_ble_adv_param.p_peer_addr != NULL) {
    delete _ble_adv_param.p_peer_addr;
  }

  if (_ble_adv_data.p_manuf_specific_data != NULL) {
    if (_ble_adv_data.p_manuf_specific_data->data.p_data != NULL) {
      delete[] _ble_adv_data.p_manuf_specific_data->data.p_data;
    }
    delete _ble_adv_data.p_manuf_specific_data;
  }  
}

void BLEPeripheral::setLocalName(String localName)
{
  setLocalName(BLE_ADVDATA_FULL_NAME, localName);  // default is full name
}


void BLEPeripheral::setLocalName(ble_advdata_name_type_t nameType, String localName)
{
  _ble_adv_data.name_type = nameType;
  _ble_adv_data.local_name = localName;
}


void BLEPeripheral::setCompanyID(const uint16_t companyID)
{
  if (_ble_adv_data.p_manuf_specific_data == NULL) {
    _ble_adv_data.p_manuf_specific_data = new ble_advdata_manuf_data_t;
    // Always clear all fields in structs or you might get unexpected behaviour
    memset(_ble_adv_data.p_manuf_specific_data, 0x00, sizeof(ble_advdata_manuf_data_t));
  }
  _ble_adv_data.p_manuf_specific_data->company_identifier = companyID;
}



void BLEPeripheral::setManufactureData(const uint8_t* data, const uint16_t length)
{
  if (_ble_adv_data.p_manuf_specific_data == NULL) {
    _ble_adv_data.p_manuf_specific_data = new ble_advdata_manuf_data_t;
    // Always clear all fields in structs or you might get unexpected behaviour
    memset(_ble_adv_data.p_manuf_specific_data, 0x00, sizeof(ble_advdata_manuf_data_t));
  }
  _ble_adv_data.p_manuf_specific_data->data.size = length;
  if (_ble_adv_data.p_manuf_specific_data->data.p_data == NULL) {
    _ble_adv_data.p_manuf_specific_data->data.p_data = new uint8_t[length];
    // Always clear all fields in structs or you might get unexpected behaviour
    memset(_ble_adv_data.p_manuf_specific_data->data.p_data, 0x00, length);
  }
  memcpy(_ble_adv_data.p_manuf_specific_data->data.p_data, data, length);
}
  

void BLEPeripheral::setAdvertisingInterval(const float minInterval, const float maxInterval)
{
  _ble_adv_param.interval_min = MSEC_TO_UNITS(maxInterval, UNIT_0_625_MS);
  _ble_adv_param.interval_max = MSEC_TO_UNITS(maxInterval, UNIT_0_625_MS);
  printf("Advertising interval: min = %d ms, max = %d ms\n", _ble_adv_param.interval_min, _ble_adv_param.interval_max);
}

void BLEPeripheral::setAdvertisingInterval(const float advInterval)
{
  setAdvertisingInterval(advInterval, advInterval);
}


void BLEPeripheral::setAdvertisingType(const uint8_t type)
{
  _ble_adv_param.type = type;
}

// If not public address, pass in NULL for address
void BLEPeripheral::setPeerAddress(uint8_t addr_type, uint8_t address[BD_ADDR_LEN])
{
  if (_ble_adv_param.p_peer_addr == NULL) {
    _ble_adv_param.p_peer_addr = new ble_gap_addr_t;
    // Always clear all fields in structs or you might get unexpected behaviour
    memset(_ble_adv_param.p_peer_addr, 0x00, sizeof(ble_gap_addr_t));
  }
  _ble_adv_param.p_peer_addr->addr_type = addr_type;
  if (addr_type == BLE_GAP_ADDR_TYPE_PUBLIC) {
    memcpy(_ble_adv_param.p_peer_addr->addr, address, BD_ADDR_LEN);
  }
}

void BLEPeripheral::setAdvertisingFilterPolicy(const uint8_t policy)
{
  _ble_adv_param.fp = policy;
}

void BLEPeripheral::setAdvertisingChannelMap(const uint8_t chn_map)
{
  _ble_adv_param.chn_map = chn_map;
}

void BLEPeripheral::setConnectable(bool connectable)
{
  uint8_t type = BLE_GAP_ADV_TYPE_ADV_IND;
  if (!connectable) {
    type = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
  }
  setAdvertisingType(type);
}

void BLEPeripheral::hci_cmd_send_reset(void)
{
  uint8_t hci_cmd_buf[128];
  uint16_t sz = make_cmd_reset(hci_cmd_buf);
  API_vhci_host_send_packet(hci_cmd_buf, sz);
}

uint16_t BLEPeripheral::make_cmd_reset(uint8_t *buf)
{
  UINT8_TO_STREAM (buf, H4_TYPE_COMMAND);
  UINT16_TO_STREAM (buf, HCI_RESET);
  UINT8_TO_STREAM (buf, 0);
  return HCI_H4_CMD_PREAMBLE_SIZE;
}

void BLEPeripheral::hci_cmd_send_ble_set_adv_data(void)
{
  uint8_t hci_cmd_buf[128];
  uint8_t adv_data[31] = {0x02, BT_DATA_FLAGS, BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE};
  uint8_t adv_data_len = 3;

  // Advertising data device local name
  if (_ble_adv_data.name_type != BLE_ADVDATA_NO_NAME) {
    if (_ble_adv_data.name_type == BLE_ADVDATA_SHORT_NAME) {
      adv_data[4] = BT_DATA_NAME_SHORTENED;
    } else {
      adv_data[4] = BT_DATA_NAME_COMPLETE;
    }
    uint8_t name_len = (uint8_t) _ble_adv_data.local_name.length();
    adv_data[3] = name_len + 1;
    adv_data_len += 2;
    
    for (int i=0; i<name_len; i++) {
      adv_data[adv_data_len+i] = (uint8_t) _ble_adv_data.local_name.charAt(i);
    }
    adv_data_len += name_len;
  }

  // Advertising data manufacture specified data
  if (_ble_adv_data.p_manuf_specific_data != NULL) {
    adv_data[adv_data_len] = _ble_adv_data.p_manuf_specific_data->data.size
      + sizeof(_ble_adv_data.p_manuf_specific_data->company_identifier) + 1;
    adv_data_len++;
    adv_data[adv_data_len] = BT_DATA_MANUFACTURER_DATA;
    adv_data_len++;

    memcpy(&adv_data[adv_data_len], &(_ble_adv_data.p_manuf_specific_data->company_identifier), sizeof(uint16_t));
    adv_data_len += sizeof(uint16_t);
    
    for (int i=0; i<_ble_adv_data.p_manuf_specific_data->data.size; i++) {
      adv_data[adv_data_len+i] = (uint8_t) _ble_adv_data.p_manuf_specific_data->data.p_data[i];
    }
    adv_data_len += _ble_adv_data.p_manuf_specific_data->data.size;
  }

  /* =================================================================================================
   * Add more advertising data here (make sure the total packet does not exceed 31 bytes)
   * =================================================================================================
   */
  
  uint16_t sz = make_cmd_ble_set_adv_data(hci_cmd_buf, adv_data_len, (uint8_t *)adv_data);
  API_vhci_host_send_packet(hci_cmd_buf, sz);
}


uint16_t BLEPeripheral::make_cmd_ble_set_adv_data(uint8_t *buf, uint8_t data_len, uint8_t *p_data)
{
  UINT8_TO_STREAM (buf, H4_TYPE_COMMAND);
  UINT16_TO_STREAM (buf, HCI_BLE_WRITE_ADV_DATA);
  UINT8_TO_STREAM  (buf, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1);

  memset(buf, 0x00, HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA);

  if (p_data != NULL && data_len > 0) {
    if (data_len > HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA) {
      data_len = HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA;
    }

    UINT8_TO_STREAM (buf, data_len);

    ARRAY_TO_STREAM (buf, p_data, data_len);
  }
  return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA + 1;
}


void BLEPeripheral::hci_cmd_send_ble_set_adv_param(void)
{
  uint8_t hci_cmd_buf[128];
  uint16_t sz = make_cmd_ble_set_adv_param(hci_cmd_buf);
  API_vhci_host_send_packet(hci_cmd_buf, sz);
}



uint16_t BLEPeripheral::make_cmd_ble_set_adv_param (uint8_t *buf)
{
  UINT8_TO_STREAM (buf, H4_TYPE_COMMAND);
  UINT16_TO_STREAM (buf, HCI_BLE_WRITE_ADV_PARAMS);
  UINT8_TO_STREAM  (buf, HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS);

  UINT16_TO_STREAM (buf, _ble_adv_param.interval_min);
  UINT16_TO_STREAM (buf, _ble_adv_param.interval_max);
  UINT8_TO_STREAM (buf, _ble_adv_param.type);
  UINT8_TO_STREAM (buf, _ble_adv_param.own_addr_type);
  UINT8_TO_STREAM (buf, _ble_adv_param.p_peer_addr->addr_type);
  ARRAY_TO_STREAM (buf, _ble_adv_param.p_peer_addr->addr, BD_ADDR_LEN);
  UINT8_TO_STREAM (buf, _ble_adv_param.chn_map);
  UINT8_TO_STREAM (buf, _ble_adv_param.fp);
  return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS;
}


void BLEPeripheral::hci_cmd_send_ble_adv_start(void)
{
  uint8_t hci_cmd_buf[128];
  uint16_t sz = make_cmd_ble_set_adv_enable (hci_cmd_buf, HCI_BLE_ADV_ENABLE);
  API_vhci_host_send_packet(hci_cmd_buf, sz);
}

uint16_t BLEPeripheral::make_cmd_ble_set_adv_enable(uint8_t *buf, uint8_t adv_enable)
{
  UINT8_TO_STREAM (buf, H4_TYPE_COMMAND);
  UINT16_TO_STREAM (buf, HCI_BLE_WRITE_ADV_ENABLE);
  UINT8_TO_STREAM  (buf, HCIC_PARAM_SIZE_WRITE_ADV_ENABLE);
  UINT8_TO_STREAM (buf, adv_enable);
  return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_ADV_ENABLE;
}

bool BLEPeripheral::begin(void)
{
  bt_controller_init();
  ::delay(1000);

  int cmd_cnt = 0;
  bool send_avail = false;
  //API_vhci_host_register_callback(&vhci_host_cb);
 
  while(cmd_cnt < 4) {
    ::delay(1000);
    send_avail = API_vhci_host_check_send_available();
    if (send_avail) {
      switch (cmd_cnt) {
      case 0: hci_cmd_send_reset(); ++cmd_cnt; break;
      case 1: hci_cmd_send_ble_set_adv_param(); ++cmd_cnt; break;
      case 2: hci_cmd_send_ble_set_adv_data(); ++cmd_cnt; break;
      case 3: hci_cmd_send_ble_adv_start(); ++cmd_cnt; break;
      }
    }
  }

  return true;
}




