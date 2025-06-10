/*
 * BLETypes.h
 *
 *  Created on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 */

#ifndef COMPONENTS_CPP_BLE_BLETYPES_H_
#define COMPONENTS_CPP_BLE_BLETYPES_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include <stdint.h>
#include "WString.h"

/*****************************************************************************
 *                              NimBLE includes                              *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <freertos/task.h>
#endif

/*****************************************************************************
 *                          Common type definitions                          *
 *****************************************************************************/

typedef struct {
  void *peer_device;  // peer device BLEClient or BLEServer
  bool connected;     // connection status
  uint16_t mtu;       // negotiated MTU per peer device
} conn_status_t;

/*****************************************************************************
 *                           NimBLE type definitions                         *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
typedef struct {
  void *pATT;
  TaskHandle_t task;
  int rc;
  String *buf;
} ble_task_data_t;
#endif


#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLEUTILS_H_ */
