/*
 * BLEUtils.cpp
 *
 *  Created on: Mar 25, 2017
 *      Author: kolban
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include "BLEAddress.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLEUUID.h"
#include "GeneralUtils.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_bt.h>           // ESP32 BLE
#include <esp_bt_main.h>      // ESP32 BLE
#include <esp_gap_ble_api.h>  // ESP32 BLE
#include <esp_gattc_api.h>    // ESP32 BLE
#include <esp_err.h>          // ESP32 ESP-IDF
#include <map>                // Part of C++ STL
#include <sstream>
#include <iomanip>

#include "esp32-hal-log.h"

typedef struct {
  uint32_t assignedNumber;
  const char *name;
} member_t;

static const member_t members_ids[] = {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  {0xFE08, "Microsoft"},
  {0xFE09, "Pillsy, Inc."},
  {0xFE0A, "ruwido austria gmbh"},
  {0xFE0B, "ruwido austria gmbh"},
  {0xFE0C, "Procter & Gamble"},
  {0xFE0D, "Procter & Gamble"},
  {0xFE0E, "Setec Pty Ltd"},
  {0xFE0F, "Philips Lighting B.V."},
  {0xFE10, "Lapis Semiconductor Co., Ltd."},
  {0xFE11, "GMC-I Messtechnik GmbH"},
  {0xFE12, "M-Way Solutions GmbH"},
  {0xFE13, "Apple Inc."},
  {0xFE14, "Flextronics International USA Inc."},
  {0xFE15, "Amazon Fulfillment Services, Inc."},
  {0xFE16, "Footmarks, Inc."},
  {0xFE17, "Telit Wireless Solutions GmbH"},
  {0xFE18, "Runtime, Inc."},
  {0xFE19, "Google Inc."},
  {0xFE1A, "Tyto Life LLC"},
  {0xFE1B, "Tyto Life LLC"},
  {0xFE1C, "NetMedia, Inc."},
  {0xFE1D, "Illuminati Instrument Corporation"},
  {0xFE1E, "Smart Innovations Co., Ltd"},
  {0xFE1F, "Garmin International, Inc."},
  {0xFE20, "Emerson"},
  {0xFE21, "Bose Corporation"},
  {0xFE22, "Zoll Medical Corporation"},
  {0xFE23, "Zoll Medical Corporation"},
  {0xFE24, "August Home Inc"},
  {0xFE25, "Apple, Inc. "},
  {0xFE26, "Google Inc."},
  {0xFE27, "Google Inc."},
  {0xFE28, "Ayla Networks"},
  {0xFE29, "Gibson Innovations"},
  {0xFE2A, "DaisyWorks, Inc."},
  {0xFE2B, "ITT Industries"},
  {0xFE2C, "Google Inc."},
  {0xFE2D, "SMART INNOVATION Co.,Ltd"},
  {0xFE2E, "ERi,Inc."},
  {0xFE2F, "CRESCO Wireless, Inc"},
  {0xFE30, "Volkswagen AG"},
  {0xFE31, "Volkswagen AG"},
  {0xFE32, "Pro-Mark, Inc."},
  {0xFE33, "CHIPOLO d.o.o."},
  {0xFE34, "SmallLoop LLC"},
  {0xFE35, "HUAWEI Technologies Co., Ltd"},
  {0xFE36, "HUAWEI Technologies Co., Ltd"},
  {0xFE37, "Spaceek LTD"},
  {0xFE38, "Spaceek LTD"},
  {0xFE39, "TTS Tooltechnic Systems AG & Co. KG"},
  {0xFE3A, "TTS Tooltechnic Systems AG & Co. KG"},
  {0xFE3B, "Dolby Laboratories"},
  {0xFE3C, "Alibaba"},
  {0xFE3D, "BD Medical"},
  {0xFE3E, "BD Medical"},
  {0xFE3F, "Friday Labs Limited"},
  {0xFE40, "Inugo Systems Limited"},
  {0xFE41, "Inugo Systems Limited"},
  {0xFE42, "Nets A/S "},
  {0xFE43, "Andreas Stihl AG & Co. KG"},
  {0xFE44, "SK Telecom "},
  {0xFE45, "Snapchat Inc"},
  {0xFE46, "B&O Play A/S "},
  {0xFE47, "General Motors"},
  {0xFE48, "General Motors"},
  {0xFE49, "SenionLab AB"},
  {0xFE4A, "OMRON HEALTHCARE Co., Ltd."},
  {0xFE4B, "Philips Lighting B.V."},
  {0xFE4C, "Volkswagen AG"},
  {0xFE4D, "Casambi Technologies Oy"},
  {0xFE4E, "NTT docomo"},
  {0xFE4F, "Molekule, Inc."},
  {0xFE50, "Google Inc."},
  {0xFE51, "SRAM"},
  {0xFE52, "SetPoint Medical"},
  {0xFE53, "3M"},
  {0xFE54, "Motiv, Inc."},
  {0xFE55, "Google Inc."},
  {0xFE56, "Google Inc."},
  {0xFE57, "Dotted Labs"},
  {0xFE58, "Nordic Semiconductor ASA"},
  {0xFE59, "Nordic Semiconductor ASA"},
  {0xFE5A, "Chronologics Corporation"},
  {0xFE5B, "GT-tronics HK Ltd"},
  {0xFE5C, "million hunters GmbH"},
  {0xFE5D, "Grundfos A/S"},
  {0xFE5E, "Plastc Corporation"},
  {0xFE5F, "Eyefi, Inc."},
  {0xFE60, "Lierda Science & Technology Group Co., Ltd."},
  {0xFE61, "Logitech International SA"},
  {0xFE62, "Indagem Tech LLC"},
  {0xFE63, "Connected Yard, Inc."},
  {0xFE64, "Siemens AG"},
  {0xFE65, "CHIPOLO d.o.o."},
  {0xFE66, "Intel Corporation"},
  {0xFE67, "Lab Sensor Solutions"},
  {0xFE68, "Qualcomm Life Inc"},
  {0xFE69, "Qualcomm Life Inc"},
  {0xFE6A, "Kontakt Micro-Location Sp. z o.o."},
  {0xFE6B, "TASER International, Inc."},
  {0xFE6C, "TASER International, Inc."},
  {0xFE6D, "The University of Tokyo"},
  {0xFE6E, "The University of Tokyo"},
  {0xFE6F, "LINE Corporation"},
  {0xFE70, "Beijing Jingdong Century Trading Co., Ltd."},
  {0xFE71, "Plume Design Inc"},
  {0xFE72, "St. Jude Medical, Inc."},
  {0xFE73, "St. Jude Medical, Inc."},
  {0xFE74, "unwire"},
  {0xFE75, "TangoMe"},
  {0xFE76, "TangoMe"},
  {0xFE77, "Hewlett-Packard Company"},
  {0xFE78, "Hewlett-Packard Company"},
  {0xFE79, "Zebra Technologies"},
  {0xFE7A, "Bragi GmbH"},
  {0xFE7B, "Orion Labs, Inc."},
  {0xFE7C, "Telit Wireless Solutions (Formerly Stollmann E+V GmbH)"},
  {0xFE7D, "Aterica Health Inc."},
  {0xFE7E, "Awear Solutions Ltd"},
  {0xFE7F, "Doppler Lab"},
  {0xFE80, "Doppler Lab"},
  {0xFE81, "Medtronic Inc."},
  {0xFE82, "Medtronic Inc."},
  {0xFE83, "Blue Bite"},
  {0xFE84, "RF Digital Corp"},
  {0xFE85, "RF Digital Corp"},
  {0xFE86, "HUAWEI Technologies Co., Ltd. ( )"},
  {0xFE87, "Qingdao Yeelink Information Technology Co., Ltd. ( )"},
  {0xFE88, "SALTO SYSTEMS S.L."},
  {0xFE89, "B&O Play A/S"},
  {0xFE8A, "Apple, Inc."},
  {0xFE8B, "Apple, Inc."},
  {0xFE8C, "TRON Forum"},
  {0xFE8D, "Interaxon Inc."},
  {0xFE8E, "ARM Ltd"},
  {0xFE8F, "CSR"},
  {0xFE90, "JUMA"},
  {0xFE91, "Shanghai Imilab Technology Co.,Ltd"},
  {0xFE92, "Jarden Safety & Security"},
  {0xFE93, "OttoQ Inc."},
  {0xFE94, "OttoQ Inc."},
  {0xFE95, "Xiaomi Inc."},
  {0xFE96, "Tesla Motor Inc."},
  {0xFE97, "Tesla Motor Inc."},
  {0xFE98, "Currant, Inc."},
  {0xFE99, "Currant, Inc."},
  {0xFE9A, "Estimote"},
  {0xFE9B, "Samsara Networks, Inc"},
  {0xFE9C, "GSI Laboratories, Inc."},
  {0xFE9D, "Mobiquity Networks Inc"},
  {0xFE9E, "Dialog Semiconductor B.V."},
  {0xFE9F, "Google Inc."},
  {0xFEA0, "Google Inc."},
  {0xFEA1, "Intrepid Control Systems, Inc."},
  {0xFEA2, "Intrepid Control Systems, Inc."},
  {0xFEA3, "ITT Industries"},
  {0xFEA4, "Paxton Access Ltd"},
  {0xFEA5, "GoPro, Inc."},
  {0xFEA6, "GoPro, Inc."},
  {0xFEA7, "UTC Fire and Security"},
  {0xFEA8, "Savant Systems LLC"},
  {0xFEA9, "Savant Systems LLC"},
  {0xFEAA, "Google Inc."},
  {0xFEAB, "Nokia Corporation"},
  {0xFEAC, "Nokia Corporation"},
  {0xFEAD, "Nokia Corporation"},
  {0xFEAE, "Nokia Corporation"},
  {0xFEAF, "Nest Labs Inc."},
  {0xFEB0, "Nest Labs Inc."},
  {0xFEB1, "Electronics Tomorrow Limited"},
  {0xFEB2, "Microsoft Corporation"},
  {0xFEB3, "Taobao"},
  {0xFEB4, "WiSilica Inc."},
  {0xFEB5, "WiSilica Inc."},
  {0xFEB6, "Vencer Co, Ltd"},
  {0xFEB7, "Facebook, Inc."},
  {0xFEB8, "Facebook, Inc."},
  {0xFEB9, "LG Electronics"},
  {0xFEBA, "Tencent Holdings Limited"},
  {0xFEBB, "adafruit industries"},
  {0xFEBC, "Dexcom, Inc. "},
  {0xFEBD, "Clover Network, Inc."},
  {0xFEBE, "Bose Corporation"},
  {0xFEBF, "Nod, Inc."},
  {0xFEC0, "KDDI Corporation"},
  {0xFEC1, "KDDI Corporation"},
  {0xFEC2, "Blue Spark Technologies, Inc."},
  {0xFEC3, "360fly, Inc."},
  {0xFEC4, "PLUS Location Systems"},
  {0xFEC5, "Realtek Semiconductor Corp."},
  {0xFEC6, "Kocomojo, LLC"},
  {0xFEC7, "Apple, Inc."},
  {0xFEC8, "Apple, Inc."},
  {0xFEC9, "Apple, Inc."},
  {0xFECA, "Apple, Inc."},
  {0xFECB, "Apple, Inc."},
  {0xFECC, "Apple, Inc."},
  {0xFECD, "Apple, Inc."},
  {0xFECE, "Apple, Inc."},
  {0xFECF, "Apple, Inc."},
  {0xFED0, "Apple, Inc."},
  {0xFED1, "Apple, Inc."},
  {0xFED2, "Apple, Inc."},
  {0xFED3, "Apple, Inc."},
  {0xFED4, "Apple, Inc."},
  {0xFED5, "Plantronics Inc."},
  {0xFED6, "Broadcom Corporation"},
  {0xFED7, "Broadcom Corporation"},
  {0xFED8, "Google Inc."},
  {0xFED9, "Pebble Technology Corporation"},
  {0xFEDA, "ISSC Technologies Corporation"},
  {0xFEDB, "Perka, Inc."},
  {0xFEDC, "Jawbone"},
  {0xFEDD, "Jawbone"},
  {0xFEDE, "Coin, Inc."},
  {0xFEDF, "Design SHIFT"},
  {0xFEE0, "Anhui Huami Information Technology Co."},
  {0xFEE1, "Anhui Huami Information Technology Co."},
  {0xFEE2, "Anki, Inc."},
  {0xFEE3, "Anki, Inc."},
  {0xFEE4, "Nordic Semiconductor ASA"},
  {0xFEE5, "Nordic Semiconductor ASA"},
  {0xFEE6, "Silvair, Inc."},
  {0xFEE7, "Tencent Holdings Limited"},
  {0xFEE8, "Quintic Corp."},
  {0xFEE9, "Quintic Corp."},
  {0xFEEA, "Swirl Networks, Inc."},
  {0xFEEB, "Swirl Networks, Inc."},
  {0xFEEC, "Tile, Inc."},
  {0xFEED, "Tile, Inc."},
  {0xFEEE, "Polar Electro Oy"},
  {0xFEEF, "Polar Electro Oy"},
  {0xFEF0, "Intel"},
  {0xFEF1, "CSR"},
  {0xFEF2, "CSR"},
  {0xFEF3, "Google Inc."},
  {0xFEF4, "Google Inc."},
  {0xFEF5, "Dialog Semiconductor GmbH"},
  {0xFEF6, "Wicentric, Inc."},
  {0xFEF7, "Aplix Corporation"},
  {0xFEF8, "Aplix Corporation"},
  {0xFEF9, "PayPal, Inc."},
  {0xFEFA, "PayPal, Inc."},
  {0xFEFB, "Telit Wireless Solutions (Formerly Stollmann E+V GmbH)"},
  {0xFEFC, "Gimbal, Inc."},
  {0xFEFD, "Gimbal, Inc."},
  {0xFEFE, "GN ReSound A/S"},
  {0xFEFF, "GN Netcom"},
  {0xFFFF, "Reserved"}, /*for testing purposes only*/
#endif
  {0, ""}
};

typedef struct {
  uint32_t assignedNumber;
  const char *name;
} gattdescriptor_t;

static const gattdescriptor_t g_descriptor_ids[] = {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  {0x2905, "Characteristic Aggregate Format"},
  {0x2900, "Characteristic Extended Properties"},
  {0x2904, "Characteristic Presentation Format"},
  {0x2901, "Characteristic User Description"},
  {0x2902, "Client Characteristic Configuration"},
  {0x290B, "Environmental Sensing Configuration"},
  {0x290C, "Environmental Sensing Measurement"},
  {0x290D, "Environmental Sensing Trigger Setting"},
  {0x2907, "External Report Reference"},
  {0x2909, "Number of Digitals"},
  {0x2908, "Report Reference"},
  {0x2903, "Server Characteristic Configuration"},
  {0x290E, "Time Trigger Setting"},
  {0x2906, "Valid Range"},
  {0x290A, "Value Trigger Setting"},
#endif
  {0, ""}
};

typedef struct {
  uint32_t assignedNumber;
  const char *name;
} characteristicMap_t;

static const characteristicMap_t g_characteristicsMappings[] = {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  {0x2A7E, "Aerobic Heart Rate Lower Limit"},
  {0x2A84, "Aerobic Heart Rate Upper Limit"},
  {0x2A7F, "Aerobic Threshold"},
  {0x2A80, "Age"},
  {0x2A5A, "Aggregate"},
  {0x2A43, "Alert Category ID"},
  {0x2A42, "Alert Category ID Bit Mask"},
  {0x2A06, "Alert Level"},
  {0x2A44, "Alert Notification Control Point"},
  {0x2A3F, "Alert Status"},
  {0x2AB3, "Altitude"},
  {0x2A81, "Anaerobic Heart Rate Lower Limit"},
  {0x2A82, "Anaerobic Heart Rate Upper Limit"},
  {0x2A83, "Anaerobic Threshold"},
  {0x2A58, "Analog"},
  {0x2A59, "Analog Output"},
  {0x2A73, "Apparent Wind Direction"},
  {0x2A72, "Apparent Wind Speed"},
  {0x2A01, "Appearance"},
  {0x2AA3, "Barometric Pressure Trend"},
  {0x2A19, "Battery Level"},
  {0x2A1B, "Battery Level State"},
  {0x2A1A, "Battery Power State"},
  {0x2A49, "Blood Pressure Feature"},
  {0x2A35, "Blood Pressure Measurement"},
  {0x2A9B, "Body Composition Feature"},
  {0x2A9C, "Body Composition Measurement"},
  {0x2A38, "Body Sensor Location"},
  {0x2AA4, "Bond Management Control Point"},
  {0x2AA5, "Bond Management Features"},
  {0x2A22, "Boot Keyboard Input Report"},
  {0x2A32, "Boot Keyboard Output Report"},
  {0x2A33, "Boot Mouse Input Report"},
  {0x2AA6, "Central Address Resolution"},
  {0x2AA8, "CGM Feature"},
  {0x2AA7, "CGM Measurement"},
  {0x2AAB, "CGM Session Run Time"},
  {0x2AAA, "CGM Session Start Time"},
  {0x2AAC, "CGM Specific Ops Control Point"},
  {0x2AA9, "CGM Status"},
  {0x2ACE, "Cross Trainer Data"},
  {0x2A5C, "CSC Feature"},
  {0x2A5B, "CSC Measurement"},
  {0x2A2B, "Current Time"},
  {0x2A66, "Cycling Power Control Point"},
  {0x2A66, "Cycling Power Control Point"},
  {0x2A65, "Cycling Power Feature"},
  {0x2A65, "Cycling Power Feature"},
  {0x2A63, "Cycling Power Measurement"},
  {0x2A64, "Cycling Power Vector"},
  {0x2A99, "Database Change Increment"},
  {0x2A85, "Date of Birth"},
  {0x2A86, "Date of Threshold Assessment"},
  {0x2A08, "Date Time"},
  {0x2A0A, "Day Date Time"},
  {0x2A09, "Day of Week"},
  {0x2A7D, "Descriptor Value Changed"},
  {0x2A00, "Device Name"},
  {0x2A7B, "Dew Point"},
  {0x2A56, "Digital"},
  {0x2A57, "Digital Output"},
  {0x2A0D, "DST Offset"},
  {0x2A6C, "Elevation"},
  {0x2A87, "Email Address"},
  {0x2A0B, "Exact Time 100"},
  {0x2A0C, "Exact Time 256"},
  {0x2A88, "Fat Burn Heart Rate Lower Limit"},
  {0x2A89, "Fat Burn Heart Rate Upper Limit"},
  {0x2A26, "Firmware Revision String"},
  {0x2A8A, "First Name"},
  {0x2AD9, "Fitness Machine Control Point"},
  {0x2ACC, "Fitness Machine Feature"},
  {0x2ADA, "Fitness Machine Status"},
  {0x2A8B, "Five Zone Heart Rate Limits"},
  {0x2AB2, "Floor Number"},
  {0x2A8C, "Gender"},
  {0x2A51, "Glucose Feature"},
  {0x2A18, "Glucose Measurement"},
  {0x2A34, "Glucose Measurement Context"},
  {0x2A74, "Gust Factor"},
  {0x2A27, "Hardware Revision String"},
  {0x2A39, "Heart Rate Control Point"},
  {0x2A8D, "Heart Rate Max"},
  {0x2A37, "Heart Rate Measurement"},
  {0x2A7A, "Heat Index"},
  {0x2A8E, "Height"},
  {0x2A4C, "HID Control Point"},
  {0x2A4A, "HID Information"},
  {0x2A8F, "Hip Circumference"},
  {0x2ABA, "HTTP Control Point"},
  {0x2AB9, "HTTP Entity Body"},
  {0x2AB7, "HTTP Headers"},
  {0x2AB8, "HTTP Status Code"},
  {0x2ABB, "HTTPS Security"},
  {0x2A6F, "Humidity"},
  {0x2A2A, "IEEE 11073-20601 Regulatory Certification Data List"},
  {0x2AD2, "Indoor Bike Data"},
  {0x2AAD, "Indoor Positioning Configuration"},
  {0x2A36, "Intermediate Cuff Pressure"},
  {0x2A1E, "Intermediate Temperature"},
  {0x2A77, "Irradiance"},
  {0x2AA2, "Language"},
  {0x2A90, "Last Name"},
  {0x2AAE, "Latitude"},
  {0x2A6B, "LN Control Point"},
  {0x2A6A, "LN Feature"},
  {0x2AB1, "Local East Coordinate"},
  {0x2AB0, "Local North Coordinate"},
  {0x2A0F, "Local Time Information"},
  {0x2A67, "Location and Speed Characteristic"},
  {0x2AB5, "Location Name"},
  {0x2AAF, "Longitude"},
  {0x2A2C, "Magnetic Declination"},
  {0x2AA0, "Magnetic Flux Density - 2D"},
  {0x2AA1, "Magnetic Flux Density - 3D"},
  {0x2A29, "Manufacturer Name String"},
  {0x2A91, "Maximum Recommended Heart Rate"},
  {0x2A21, "Measurement Interval"},
  {0x2A24, "Model Number String"},
  {0x2A68, "Navigation"},
  {0x2A3E, "Network Availability"},
  {0x2A46, "New Alert"},
  {0x2AC5, "Object Action Control Point"},
  {0x2AC8, "Object Changed"},
  {0x2AC1, "Object First-Created"},
  {0x2AC3, "Object ID"},
  {0x2AC2, "Object Last-Modified"},
  {0x2AC6, "Object List Control Point"},
  {0x2AC7, "Object List Filter"},
  {0x2ABE, "Object Name"},
  {0x2AC4, "Object Properties"},
  {0x2AC0, "Object Size"},
  {0x2ABF, "Object Type"},
  {0x2ABD, "OTS Feature"},
  {0x2A04, "Peripheral Preferred Connection Parameters"},
  {0x2A02, "Peripheral Privacy Flag"},
  {0x2A5F, "PLX Continuous Measurement Characteristic"},
  {0x2A60, "PLX Features"},
  {0x2A5E, "PLX Spot-Check Measurement"},
  {0x2A50, "PnP ID"},
  {0x2A75, "Pollen Concentration"},
  {0x2A2F, "Position 2D"},
  {0x2A30, "Position 3D"},
  {0x2A69, "Position Quality"},
  {0x2A6D, "Pressure"},
  {0x2A4E, "Protocol Mode"},
  {0x2A62, "Pulse Oximetry Control Point"},
  {0x2A60, "Pulse Oximetry Pulsatile Event Characteristic"},
  {0x2A78, "Rainfall"},
  {0x2A03, "Reconnection Address"},
  {0x2A52, "Record Access Control Point"},
  {0x2A14, "Reference Time Information"},
  {0x2A3A, "Removable"},
  {0x2A4D, "Report"},
  {0x2A4B, "Report Map"},
  {0x2AC9, "Resolvable Private Address Only"},
  {0x2A92, "Resting Heart Rate"},
  {0x2A40, "Ringer Control point"},
  {0x2A41, "Ringer Setting"},
  {0x2AD1, "Rower Data"},
  {0x2A54, "RSC Feature"},
  {0x2A53, "RSC Measurement"},
  {0x2A55, "SC Control Point"},
  {0x2A4F, "Scan Interval Window"},
  {0x2A31, "Scan Refresh"},
  {0x2A3C, "Scientific Temperature Celsius"},
  {0x2A10, "Secondary Time Zone"},
  {0x2A5D, "Sensor Location"},
  {0x2A25, "Serial Number String"},
  {0x2A05, "Service Changed"},
  {0x2A3B, "Service Required"},
  {0x2A28, "Software Revision String"},
  {0x2A93, "Sport Type for Aerobic and Anaerobic Thresholds"},
  {0x2AD0, "Stair Climber Data"},
  {0x2ACF, "Step Climber Data"},
  {0x2A3D, "String"},
  {0x2AD7, "Supported Heart Rate Range"},
  {0x2AD5, "Supported Inclination Range"},
  {0x2A47, "Supported New Alert Category"},
  {0x2AD8, "Supported Power Range"},
  {0x2AD6, "Supported Resistance Level Range"},
  {0x2AD4, "Supported Speed Range"},
  {0x2A48, "Supported Unread Alert Category"},
  {0x2A23, "System ID"},
  {0x2ABC, "TDS Control Point"},
  {0x2A6E, "Temperature"},
  {0x2A1F, "Temperature Celsius"},
  {0x2A20, "Temperature Fahrenheit"},
  {0x2A1C, "Temperature Measurement"},
  {0x2A1D, "Temperature Type"},
  {0x2A94, "Three Zone Heart Rate Limits"},
  {0x2A12, "Time Accuracy"},
  {0x2A15, "Time Broadcast"},
  {0x2A13, "Time Source"},
  {0x2A16, "Time Update Control Point"},
  {0x2A17, "Time Update State"},
  {0x2A11, "Time with DST"},
  {0x2A0E, "Time Zone"},
  {0x2AD3, "Training Status"},
  {0x2ACD, "Treadmill Data"},
  {0x2A71, "True Wind Direction"},
  {0x2A70, "True Wind Speed"},
  {0x2A95, "Two Zone Heart Rate Limit"},
  {0x2A07, "Tx Power Level"},
  {0x2AB4, "Uncertainty"},
  {0x2A45, "Unread Alert Status"},
  {0x2AB6, "URI"},
  {0x2A9F, "User Control Point"},
  {0x2A9A, "User Index"},
  {0x2A76, "UV Index"},
  {0x2A96, "VO2 Max"},
  {0x2A97, "Waist Circumference"},
  {0x2A98, "Weight"},
  {0x2A9D, "Weight Measurement"},
  {0x2A9E, "Weight Scale Feature"},
  {0x2A79, "Wind Chill"},
#endif
  {0, ""}
};

/**
 * @brief Mapping from service ids to names
 */
typedef struct {
  const char *name;
  const char *type;
  uint32_t assignedNumber;
} gattService_t;

/**
 * Definition of the service ids to names that we know about.
 */
static const gattService_t g_gattServices[] = {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  {"Alert Notification Service", "org.bluetooth.service.alert_notification", 0x1811},
  {"Automation IO", "org.bluetooth.service.automation_io", 0x1815},
  {"Battery Service", "org.bluetooth.service.battery_service", 0x180F},
  {"Blood Pressure", "org.bluetooth.service.blood_pressure", 0x1810},
  {"Body Composition", "org.bluetooth.service.body_composition", 0x181B},
  {"Bond Management", "org.bluetooth.service.bond_management", 0x181E},
  {"Continuous Glucose Monitoring", "org.bluetooth.service.continuous_glucose_monitoring", 0x181F},
  {"Current Time Service", "org.bluetooth.service.current_time", 0x1805},
  {"Cycling Power", "org.bluetooth.service.cycling_power", 0x1818},
  {"Cycling Speed and Cadence", "org.bluetooth.service.cycling_speed_and_cadence", 0x1816},
  {"Device Information", "org.bluetooth.service.device_information", 0x180A},
  {"Environmental Sensing", "org.bluetooth.service.environmental_sensing", 0x181A},
  {"Generic Access", "org.bluetooth.service.generic_access", 0x1800},
  {"Generic Attribute", "org.bluetooth.service.generic_attribute", 0x1801},
  {"Glucose", "org.bluetooth.service.glucose", 0x1808},
  {"Health Thermometer", "org.bluetooth.service.health_thermometer", 0x1809},
  {"Heart Rate", "org.bluetooth.service.heart_rate", 0x180D},
  {"HTTP Proxy", "org.bluetooth.service.http_proxy", 0x1823},
  {"Human Interface Device", "org.bluetooth.service.human_interface_device", 0x1812},
  {"Immediate Alert", "org.bluetooth.service.immediate_alert", 0x1802},
  {"Indoor Positioning", "org.bluetooth.service.indoor_positioning", 0x1821},
  {"Internet Protocol Support", "org.bluetooth.service.internet_protocol_support", 0x1820},
  {"Link Loss", "org.bluetooth.service.link_loss", 0x1803},
  {"Location and Navigation", "org.bluetooth.service.location_and_navigation", 0x1819},
  {"Next DST Change Service", "org.bluetooth.service.next_dst_change", 0x1807},
  {"Object Transfer", "org.bluetooth.service.object_transfer", 0x1825},
  {"Phone Alert Status Service", "org.bluetooth.service.phone_alert_status", 0x180E},
  {"Pulse Oximeter", "org.bluetooth.service.pulse_oximeter", 0x1822},
  {"Reference Time Update Service", "org.bluetooth.service.reference_time_update", 0x1806},
  {"Running Speed and Cadence", "org.bluetooth.service.running_speed_and_cadence", 0x1814},
  {"Scan Parameters", "org.bluetooth.service.scan_parameters", 0x1813},
  {"Transport Discovery", "org.bluetooth.service.transport_discovery", 0x1824},
  {"Tx Power", "org.bluetooth.service.tx_power", 0x1804},
  {"User Data", "org.bluetooth.service.user_data", 0x181C},
  {"Weight Scale", "org.bluetooth.service.weight_scale", 0x181D},
#endif
  {"", "", 0}
};

/**
 * @brief Convert characteristic properties into a string representation.
 * @param [in] prop Characteristic properties.
 * @return A string representation of characteristic properties.
 */
String BLEUtils::characteristicPropertiesToString(esp_gatt_char_prop_t prop) {
  String res = "broadcast: ";
  res += ((prop & ESP_GATT_CHAR_PROP_BIT_BROADCAST) ? "1" : "0");
  res += ", read: ";
  res += ((prop & ESP_GATT_CHAR_PROP_BIT_READ) ? "1" : "0");
  res += ", write_nr: ";
  res += ((prop & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) ? "1" : "0");
  res += ", write: ";
  res += ((prop & ESP_GATT_CHAR_PROP_BIT_WRITE) ? "1" : "0");
  res += ", notify: ";
  res += ((prop & ESP_GATT_CHAR_PROP_BIT_NOTIFY) ? "1" : "0");
  res += ", indicate: ";
  res += ((prop & ESP_GATT_CHAR_PROP_BIT_INDICATE) ? "1" : "0");
  res += ", auth: ";
  res += ((prop & ESP_GATT_CHAR_PROP_BIT_AUTH) ? "1" : "0");
  return res;
}  // characteristicPropertiesToString

/**
 * @brief Convert an esp_gatt_id_t to a string.
 */
static String gattIdToString(esp_gatt_id_t gattId) {
  String res = "uuid: " + BLEUUID(gattId.uuid).toString() + ", inst_id: ";
  char val[8];
  snprintf(val, sizeof(val), "%d", (int)gattId.inst_id);
  res += val;
  return res;
}  // gattIdToString

/**
 * @brief Convert an esp_ble_addr_type_t to a string representation.
 */
const char *BLEUtils::addressTypeToString(esp_ble_addr_type_t type) {
  switch (type) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case BLE_ADDR_TYPE_PUBLIC:     return "BLE_ADDR_TYPE_PUBLIC";
    case BLE_ADDR_TYPE_RANDOM:     return "BLE_ADDR_TYPE_RANDOM";
    case BLE_ADDR_TYPE_RPA_PUBLIC: return "BLE_ADDR_TYPE_RPA_PUBLIC";
    case BLE_ADDR_TYPE_RPA_RANDOM: return "BLE_ADDR_TYPE_RPA_RANDOM";
#endif
    default: return " esp_ble_addr_type_t";
  }
}  // addressTypeToString

/**
 * @brief Convert the BLE Advertising Data flags to a string.
 * @param adFlags The flags to convert
 * @return String A string representation of the advertising flags.
 */
String BLEUtils::adFlagsToString(uint8_t adFlags) {
  String res;
  if (adFlags & (1 << 0)) {
    res += "[LE Limited Discoverable Mode] ";
  }
  if (adFlags & (1 << 1)) {
    res += "[LE General Discoverable Mode] ";
  }
  if (adFlags & (1 << 2)) {
    res += "[BR/EDR Not Supported] ";
  }
  if (adFlags & (1 << 3)) {
    res += "[Simultaneous LE and BR/EDR to Same Device Capable (Controller)] ";
  }
  if (adFlags & (1 << 4)) {
    res += "[Simultaneous LE and BR/EDR to Same Device Capable (Host)] ";
  }
  return res;
}  // adFlagsToString

/**
 * @brief Given an advertising type, return a string representation of the type.
 *
 * For details see ...
 * https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile
 *
 * @return A string representation of the type.
 */
const char *BLEUtils::advTypeToString(uint8_t advType) {
  switch (advType) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_BLE_AD_TYPE_FLAG:  // 0x01
      return "ESP_BLE_AD_TYPE_FLAG";
    case ESP_BLE_AD_TYPE_16SRV_PART:  // 0x02
      return "ESP_BLE_AD_TYPE_16SRV_PART";
    case ESP_BLE_AD_TYPE_16SRV_CMPL:  // 0x03
      return "ESP_BLE_AD_TYPE_16SRV_CMPL";
    case ESP_BLE_AD_TYPE_32SRV_PART:  // 0x04
      return "ESP_BLE_AD_TYPE_32SRV_PART";
    case ESP_BLE_AD_TYPE_32SRV_CMPL:  // 0x05
      return "ESP_BLE_AD_TYPE_32SRV_CMPL";
    case ESP_BLE_AD_TYPE_128SRV_PART:  // 0x06
      return "ESP_BLE_AD_TYPE_128SRV_PART";
    case ESP_BLE_AD_TYPE_128SRV_CMPL:  // 0x07
      return "ESP_BLE_AD_TYPE_128SRV_CMPL";
    case ESP_BLE_AD_TYPE_NAME_SHORT:  // 0x08
      return "ESP_BLE_AD_TYPE_NAME_SHORT";
    case ESP_BLE_AD_TYPE_NAME_CMPL:  // 0x09
      return "ESP_BLE_AD_TYPE_NAME_CMPL";
    case ESP_BLE_AD_TYPE_TX_PWR:  // 0x0a
      return "ESP_BLE_AD_TYPE_TX_PWR";
    case ESP_BLE_AD_TYPE_DEV_CLASS:  // 0x0b
      return "ESP_BLE_AD_TYPE_DEV_CLASS";
    case ESP_BLE_AD_TYPE_SM_TK:  // 0x10
      return "ESP_BLE_AD_TYPE_SM_TK";
    case ESP_BLE_AD_TYPE_SM_OOB_FLAG:  // 0x11
      return "ESP_BLE_AD_TYPE_SM_OOB_FLAG";
    case ESP_BLE_AD_TYPE_INT_RANGE:  // 0x12
      return "ESP_BLE_AD_TYPE_INT_RANGE";
    case ESP_BLE_AD_TYPE_SOL_SRV_UUID:  // 0x14
      return "ESP_BLE_AD_TYPE_SOL_SRV_UUID";
    case ESP_BLE_AD_TYPE_128SOL_SRV_UUID:  // 0x15
      return "ESP_BLE_AD_TYPE_128SOL_SRV_UUID";
    case ESP_BLE_AD_TYPE_SERVICE_DATA:  // 0x16
      return "ESP_BLE_AD_TYPE_SERVICE_DATA";
    case ESP_BLE_AD_TYPE_PUBLIC_TARGET:  // 0x17
      return "ESP_BLE_AD_TYPE_PUBLIC_TARGET";
    case ESP_BLE_AD_TYPE_RANDOM_TARGET:  // 0x18
      return "ESP_BLE_AD_TYPE_RANDOM_TARGET";
    case ESP_BLE_AD_TYPE_APPEARANCE:  // 0x19
      return "ESP_BLE_AD_TYPE_APPEARANCE";
    case ESP_BLE_AD_TYPE_ADV_INT:  // 0x1a
      return "ESP_BLE_AD_TYPE_ADV_INT";
    case ESP_BLE_AD_TYPE_32SOL_SRV_UUID: return "ESP_BLE_AD_TYPE_32SOL_SRV_UUID";
    case ESP_BLE_AD_TYPE_32SERVICE_DATA:  // 0x20
      return "ESP_BLE_AD_TYPE_32SERVICE_DATA";
    case ESP_BLE_AD_TYPE_128SERVICE_DATA:  // 0x21
      return "ESP_BLE_AD_TYPE_128SERVICE_DATA";
    case ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE:  // 0xff
      return "ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE";
#endif
    default: log_v(" adv data type: 0x%x", advType); return "";
  }  // End switch
}  // advTypeToString

esp_gatt_id_t BLEUtils::buildGattId(esp_bt_uuid_t uuid, uint8_t inst_id) {
  esp_gatt_id_t retGattId;
  retGattId.uuid = uuid;
  retGattId.inst_id = inst_id;
  return retGattId;
}

esp_gatt_srvc_id_t BLEUtils::buildGattSrvcId(esp_gatt_id_t gattId, bool is_primary) {
  esp_gatt_srvc_id_t retSrvcId;
  retSrvcId.id = gattId;
  retSrvcId.is_primary = is_primary;
  return retSrvcId;
}

/**
 * @brief Create a hex representation of data.
 *
 * @param [in] target Where to write the hex string.  If this is null, we malloc storage.
 * @param [in] source The start of the binary data.
 * @param [in] length The length of the data to convert.
 * @return A pointer to the formatted buffer.
 */
char *BLEUtils::buildHexData(uint8_t *target, uint8_t *source, uint8_t length) {
  // Guard against too much data.
  if (length > 100) {
    length = 100;
  }

  if (target == nullptr) {
    target = (uint8_t *)malloc(length * 2 + 1);
    if (target == nullptr) {
      log_e("buildHexData: malloc failed");
      return nullptr;
    }
  }
  char *startOfData = (char *)target;

  for (int i = 0; i < length; i++) {
    sprintf((char *)target, "%.2x", (char)*source);
    source++;
    target += 2;
  }

  // Handle the special case where there was no data.
  if (length == 0) {
    *startOfData = 0;
  }

  return startOfData;
}  // buildHexData

/**
 * @brief Build a printable string of memory range.
 * Create a string representation of a piece of memory. Only printable characters will be included
 * while those that are not printable will be replaced with '.'.
 * @param [in] source Start of memory.
 * @param [in] length Length of memory.
 * @return A string representation of a piece of memory.
 */
String BLEUtils::buildPrintData(uint8_t *source, size_t length) {
  String res;
  for (int i = 0; i < length; i++) {
    char c = *source;
    res += (isprint(c) ? c : '.');
    source++;
  }
  return res;
}  // buildPrintData

/**
 * @brief Convert a close/disconnect reason to a string.
 * @param [in] reason The close reason.
 * @return A string representation of the reason.
 */
String BLEUtils::gattCloseReasonToString(esp_gatt_conn_reason_t reason) {
  switch (reason) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_GATT_CONN_UNKNOWN:
    {
      return "ESP_GATT_CONN_UNKNOWN";
    }
    case ESP_GATT_CONN_L2C_FAILURE:
    {
      return "ESP_GATT_CONN_L2C_FAILURE";
    }
    case ESP_GATT_CONN_TIMEOUT:
    {
      return "ESP_GATT_CONN_TIMEOUT";
    }
    case ESP_GATT_CONN_TERMINATE_PEER_USER:
    {
      return "ESP_GATT_CONN_TERMINATE_PEER_USER";
    }
    case ESP_GATT_CONN_TERMINATE_LOCAL_HOST:
    {
      return "ESP_GATT_CONN_TERMINATE_LOCAL_HOST";
    }
    case ESP_GATT_CONN_FAIL_ESTABLISH:
    {
      return "ESP_GATT_CONN_FAIL_ESTABLISH";
    }
    case ESP_GATT_CONN_LMP_TIMEOUT:
    {
      return "ESP_GATT_CONN_LMP_TIMEOUT";
    }
    case ESP_GATT_CONN_CONN_CANCEL:
    {
      return "ESP_GATT_CONN_CONN_CANCEL";
    }
    case ESP_GATT_CONN_NONE:
    {
      return "ESP_GATT_CONN_NONE";
    }
#endif
    default:
    {
      return "Unknown";
    }
  }
}  // gattCloseReasonToString

String BLEUtils::gattClientEventTypeToString(esp_gattc_cb_event_t eventType) {
  switch (eventType) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_GATTC_ACL_EVT:            return "ESP_GATTC_ACL_EVT";
    case ESP_GATTC_ADV_DATA_EVT:       return "ESP_GATTC_ADV_DATA_EVT";
    case ESP_GATTC_ADV_VSC_EVT:        return "ESP_GATTC_ADV_VSC_EVT";
    case ESP_GATTC_BTH_SCAN_CFG_EVT:   return "ESP_GATTC_BTH_SCAN_CFG_EVT";
    case ESP_GATTC_BTH_SCAN_DIS_EVT:   return "ESP_GATTC_BTH_SCAN_DIS_EVT";
    case ESP_GATTC_BTH_SCAN_ENB_EVT:   return "ESP_GATTC_BTH_SCAN_ENB_EVT";
    case ESP_GATTC_BTH_SCAN_PARAM_EVT: return "ESP_GATTC_BTH_SCAN_PARAM_EVT";
    case ESP_GATTC_BTH_SCAN_RD_EVT:    return "ESP_GATTC_BTH_SCAN_RD_EVT";
    case ESP_GATTC_BTH_SCAN_THR_EVT:   return "ESP_GATTC_BTH_SCAN_THR_EVT";
    case ESP_GATTC_CANCEL_OPEN_EVT:    return "ESP_GATTC_CANCEL_OPEN_EVT";
    case ESP_GATTC_CFG_MTU_EVT:        return "ESP_GATTC_CFG_MTU_EVT";
    case ESP_GATTC_CLOSE_EVT:          return "ESP_GATTC_CLOSE_EVT";
    case ESP_GATTC_CONGEST_EVT:        return "ESP_GATTC_CONGEST_EVT";
    case ESP_GATTC_CONNECT_EVT:        return "ESP_GATTC_CONNECT_EVT";
    case ESP_GATTC_DISCONNECT_EVT:     return "ESP_GATTC_DISCONNECT_EVT";
    case ESP_GATTC_ENC_CMPL_CB_EVT:    return "ESP_GATTC_ENC_CMPL_CB_EVT";
    case ESP_GATTC_EXEC_EVT:
      return "ESP_GATTC_EXEC_EVT";
      //case ESP_GATTC_GET_CHAR_EVT:
      //			return "ESP_GATTC_GET_CHAR_EVT";
      //case ESP_GATTC_GET_DESCR_EVT:
      //			return "ESP_GATTC_GET_DESCR_EVT";
      //case ESP_GATTC_GET_INCL_SRVC_EVT:
      //			return "ESP_GATTC_GET_INCL_SRVC_EVT";
    case ESP_GATTC_MULT_ADV_DATA_EVT:    return "ESP_GATTC_MULT_ADV_DATA_EVT";
    case ESP_GATTC_MULT_ADV_DIS_EVT:     return "ESP_GATTC_MULT_ADV_DIS_EVT";
    case ESP_GATTC_MULT_ADV_ENB_EVT:     return "ESP_GATTC_MULT_ADV_ENB_EVT";
    case ESP_GATTC_MULT_ADV_UPD_EVT:     return "ESP_GATTC_MULT_ADV_UPD_EVT";
    case ESP_GATTC_NOTIFY_EVT:           return "ESP_GATTC_NOTIFY_EVT";
    case ESP_GATTC_OPEN_EVT:             return "ESP_GATTC_OPEN_EVT";
    case ESP_GATTC_PREP_WRITE_EVT:       return "ESP_GATTC_PREP_WRITE_EVT";
    case ESP_GATTC_READ_CHAR_EVT:        return "ESP_GATTC_READ_CHAR_EVT";
    case ESP_GATTC_REG_EVT:              return "ESP_GATTC_REG_EVT";
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:   return "ESP_GATTC_REG_FOR_NOTIFY_EVT";
    case ESP_GATTC_SCAN_FLT_CFG_EVT:     return "ESP_GATTC_SCAN_FLT_CFG_EVT";
    case ESP_GATTC_SCAN_FLT_PARAM_EVT:   return "ESP_GATTC_SCAN_FLT_PARAM_EVT";
    case ESP_GATTC_SCAN_FLT_STATUS_EVT:  return "ESP_GATTC_SCAN_FLT_STATUS_EVT";
    case ESP_GATTC_SEARCH_CMPL_EVT:      return "ESP_GATTC_SEARCH_CMPL_EVT";
    case ESP_GATTC_SEARCH_RES_EVT:       return "ESP_GATTC_SEARCH_RES_EVT";
    case ESP_GATTC_SRVC_CHG_EVT:         return "ESP_GATTC_SRVC_CHG_EVT";
    case ESP_GATTC_READ_DESCR_EVT:       return "ESP_GATTC_READ_DESCR_EVT";
    case ESP_GATTC_UNREG_EVT:            return "ESP_GATTC_UNREG_EVT";
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: return "ESP_GATTC_UNREG_FOR_NOTIFY_EVT";
    case ESP_GATTC_WRITE_CHAR_EVT:       return "ESP_GATTC_WRITE_CHAR_EVT";
    case ESP_GATTC_WRITE_DESCR_EVT:      return "ESP_GATTC_WRITE_DESCR_EVT";
#endif
    default: log_v("Unknown GATT Client event type: %d", eventType); return "Unknown";
  }
}  // gattClientEventTypeToString

/**
 * @brief Return a string representation of a GATT server event code.
 * @param [in] eventType A GATT server event code.
 * @return A string representation of the GATT server event code.
 */
String BLEUtils::gattServerEventTypeToString(esp_gatts_cb_event_t eventType) {
  switch (eventType) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_GATTS_REG_EVT:                 return "ESP_GATTS_REG_EVT";
    case ESP_GATTS_READ_EVT:                return "ESP_GATTS_READ_EVT";
    case ESP_GATTS_WRITE_EVT:               return "ESP_GATTS_WRITE_EVT";
    case ESP_GATTS_EXEC_WRITE_EVT:          return "ESP_GATTS_EXEC_WRITE_EVT";
    case ESP_GATTS_MTU_EVT:                 return "ESP_GATTS_MTU_EVT";
    case ESP_GATTS_CONF_EVT:                return "ESP_GATTS_CONF_EVT";
    case ESP_GATTS_UNREG_EVT:               return "ESP_GATTS_UNREG_EVT";
    case ESP_GATTS_CREATE_EVT:              return "ESP_GATTS_CREATE_EVT";
    case ESP_GATTS_ADD_INCL_SRVC_EVT:       return "ESP_GATTS_ADD_INCL_SRVC_EVT";
    case ESP_GATTS_ADD_CHAR_EVT:            return "ESP_GATTS_ADD_CHAR_EVT";
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:      return "ESP_GATTS_ADD_CHAR_DESCR_EVT";
    case ESP_GATTS_DELETE_EVT:              return "ESP_GATTS_DELETE_EVT";
    case ESP_GATTS_START_EVT:               return "ESP_GATTS_START_EVT";
    case ESP_GATTS_STOP_EVT:                return "ESP_GATTS_STOP_EVT";
    case ESP_GATTS_CONNECT_EVT:             return "ESP_GATTS_CONNECT_EVT";
    case ESP_GATTS_DISCONNECT_EVT:          return "ESP_GATTS_DISCONNECT_EVT";
    case ESP_GATTS_OPEN_EVT:                return "ESP_GATTS_OPEN_EVT";
    case ESP_GATTS_CANCEL_OPEN_EVT:         return "ESP_GATTS_CANCEL_OPEN_EVT";
    case ESP_GATTS_CLOSE_EVT:               return "ESP_GATTS_CLOSE_EVT";
    case ESP_GATTS_LISTEN_EVT:              return "ESP_GATTS_LISTEN_EVT";
    case ESP_GATTS_CONGEST_EVT:             return "ESP_GATTS_CONGEST_EVT";
    case ESP_GATTS_RESPONSE_EVT:            return "ESP_GATTS_RESPONSE_EVT";
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:      return "ESP_GATTS_CREAT_ATTR_TAB_EVT";
    case ESP_GATTS_SET_ATTR_VAL_EVT:        return "ESP_GATTS_SET_ATTR_VAL_EVT";
    case ESP_GATTS_SEND_SERVICE_CHANGE_EVT: return "ESP_GATTS_SEND_SERVICE_CHANGE_EVT";
#endif
    default: return "Unknown";
  }
}  // gattServerEventTypeToString

/**
 * @brief Convert a BLE device type to a string.
 * @param [in] type The device type.
 */
const char *BLEUtils::devTypeToString(esp_bt_dev_type_t type) {
  switch (type) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_BT_DEVICE_TYPE_BREDR: return "ESP_BT_DEVICE_TYPE_BREDR";
    case ESP_BT_DEVICE_TYPE_BLE:   return "ESP_BT_DEVICE_TYPE_BLE";
    case ESP_BT_DEVICE_TYPE_DUMO:  return "ESP_BT_DEVICE_TYPE_DUMO";
#endif
    default: return "Unknown";
  }
}  // devTypeToString

/**
 * @brief Dump the GAP event to the log.
 */
void BLEUtils::dumpGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  log_v("Received a GAP event: %s", gapEventToString(event));
  switch (event) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    // ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT
    // adv_data_cmpl
    // - esp_bt_status_t
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->adv_data_cmpl.status);
      break;
    }  // ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT

    // ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT
    //
    // adv_data_raw_cmpl
    // - esp_bt_status_t status
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->adv_data_raw_cmpl.status);
      break;
    }  // ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT

    // ESP_GAP_BLE_ADV_START_COMPLETE_EVT
    //
    // adv_start_cmpl
    // - esp_bt_status_t status
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->adv_start_cmpl.status);
      break;
    }  // ESP_GAP_BLE_ADV_START_COMPLETE_EVT

    // ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT
    //
    // adv_stop_cmpl
    // - esp_bt_status_t status
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->adv_stop_cmpl.status);
      break;
    }  // ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT

    // ESP_GAP_BLE_AUTH_CMPL_EVT
    //
    // auth_cmpl
    // - esp_bd_addr_t bd_addr
    // - bool key_present
    // - esp_link_key key
    // - bool success
    // - uint8_t fail_reason
    // - esp_bd_addr_type_t addr_type
    // - esp_bt_dev_type_t dev_type
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
    {
      log_v(
        "[bd_addr: %s, key_present: %d, key: ***, key_type: %d, success: %d, fail_reason: %d, addr_type: ***, dev_type: %s]",
        BLEAddress(param->ble_security.auth_cmpl.bd_addr).toString().c_str(), param->ble_security.auth_cmpl.key_present, param->ble_security.auth_cmpl.key_type,
        param->ble_security.auth_cmpl.success, param->ble_security.auth_cmpl.fail_reason, BLEUtils::devTypeToString(param->ble_security.auth_cmpl.dev_type)
      );
      break;
    }  // ESP_GAP_BLE_AUTH_CMPL_EVT

    // ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT
    //
    // clear_bond_dev_cmpl
    // - esp_bt_status_t status
    case ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->clear_bond_dev_cmpl.status);
      break;
    }  // ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT

    // ESP_GAP_BLE_LOCAL_IR_EVT
    case ESP_GAP_BLE_LOCAL_IR_EVT:
    {
      break;
    }  // ESP_GAP_BLE_LOCAL_IR_EVT

    // ESP_GAP_BLE_LOCAL_ER_EVT
    case ESP_GAP_BLE_LOCAL_ER_EVT:
    {
      break;
    }  // ESP_GAP_BLE_LOCAL_ER_EVT

    // ESP_GAP_BLE_NC_REQ_EVT
    case ESP_GAP_BLE_NC_REQ_EVT:
    {
      log_v("[bd_addr: %s, passkey: %d]", BLEAddress(param->ble_security.key_notif.bd_addr).toString().c_str(), param->ble_security.key_notif.passkey);
      break;
    }  // ESP_GAP_BLE_NC_REQ_EVT

    // ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT
    //
    // read_rssi_cmpl
    // - esp_bt_status_t status
    // - int8_t rssi
    // - esp_bd_addr_t remote_addr
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
    {
      log_v(
        "[status: %d, rssi: %d, remote_addr: %s]", param->read_rssi_cmpl.status, param->read_rssi_cmpl.rssi,
        BLEAddress(param->read_rssi_cmpl.remote_addr).toString().c_str()
      );
      break;
    }  // ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT

    // ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT
    //
    // scan_param_cmpl.
    // - esp_bt_status_t status
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->scan_param_cmpl.status);
      break;
    }  // ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT

    // ESP_GAP_BLE_SCAN_RESULT_EVT
    //
    // scan_rst:
    // - search_evt
    // - bda
    // - dev_type
    // - ble_addr_type
    // - ble_evt_type
    // - rssi
    // - ble_adv
    // - flag
    // - num_resps
    // - adv_data_len
    // - scan_rsp_len
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
      switch (param->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
        {
          log_v(
            "search_evt: %s, bda: %s, dev_type: %s, ble_addr_type: %s, ble_evt_type: %s, rssi: %d, ble_adv: ??, flag: %d (%s), num_resps: %d, adv_data_len: "
            "%d, scan_rsp_len: %d",
            searchEventTypeToString(param->scan_rst.search_evt), BLEAddress(param->scan_rst.bda).toString().c_str(), devTypeToString(param->scan_rst.dev_type),
            addressTypeToString(param->scan_rst.ble_addr_type), eventTypeToString(param->scan_rst.ble_evt_type), param->scan_rst.rssi, param->scan_rst.flag,
            adFlagsToString(param->scan_rst.flag).c_str(), param->scan_rst.num_resps, param->scan_rst.adv_data_len, param->scan_rst.scan_rsp_len
          );
          break;
        }  // ESP_GAP_SEARCH_INQ_RES_EVT

        default:
        {
          log_v("search_evt: %s", searchEventTypeToString(param->scan_rst.search_evt));
          break;
        }
      }
      break;
    }  // ESP_GAP_BLE_SCAN_RESULT_EVT

    // ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT
    //
    // scan_rsp_data_cmpl
    // - esp_bt_status_t status
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->scan_rsp_data_cmpl.status);
      break;
    }  // ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT

    // ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->scan_rsp_data_raw_cmpl.status);
      break;
    }  // ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT

    // ESP_GAP_BLE_SCAN_START_COMPLETE_EVT
    //
    // scan_start_cmpl
    // - esp_bt_status_t status
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->scan_start_cmpl.status);
      break;
    }  // ESP_GAP_BLE_SCAN_START_COMPLETE_EVT

    // ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT
    //
    // scan_stop_cmpl
    // - esp_bt_status_t status
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
    {
      log_v("[status: %d]", param->scan_stop_cmpl.status);
      break;
    }  // ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT

    // ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT
    //
    // update_conn_params
    // - esp_bt_status_t status
    // - esp_bd_addr_t bda
    // - uint16_t min_int
    // - uint16_t max_int
    // - uint16_t latency
    // - uint16_t conn_int
    // - uint16_t timeout
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    {
      log_v(
        "[status: %d, bd_addr: %s, min_int: %d, max_int: %d, latency: %d, conn_int: %d, timeout: %d]", param->update_conn_params.status,
        BLEAddress(param->update_conn_params.bda).toString().c_str(), param->update_conn_params.min_int, param->update_conn_params.max_int,
        param->update_conn_params.latency, param->update_conn_params.conn_int, param->update_conn_params.timeout
      );
      break;
    }  // ESP_GAP_BLE_SCAN_UPDATE_CONN_PARAMS_EVT

    // ESP_GAP_BLE_SEC_REQ_EVT
    case ESP_GAP_BLE_SEC_REQ_EVT:
    {
      log_v("[bd_addr: %s]", BLEAddress(param->ble_security.ble_req.bd_addr).toString().c_str());
      break;
    }  // ESP_GAP_BLE_SEC_REQ_EVT
#endif
    default:
    {
      log_v("*** dumpGapEvent: Logger not coded ***");
      break;
    }  // default
  }  // switch
}  // dumpGapEvent

/**
 * @brief Decode and dump a GATT client event
 *
 * @param [in] event The type of event received.
 * @param [in] evtParam The data associated with the event.
 */
void BLEUtils::dumpGattClientEvent(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam) {

  //esp_ble_gattc_cb_param_t* evtParam = (esp_ble_gattc_cb_param_t*) param;
  log_v("GATT Event: %s", BLEUtils::gattClientEventTypeToString(event).c_str());
  switch (event) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    // ESP_GATTC_CLOSE_EVT
    //
    // close:
    // - esp_gatt_status_t      status
    // - uint16_t               conn_id
    // - esp_bd_addr_t          remote_bda
    // - esp_gatt_conn_reason_t reason
    case ESP_GATTC_CLOSE_EVT:
    {
      log_v(
        "[status: %s, reason:%s, conn_id: %d]", BLEUtils::gattStatusToString(evtParam->close.status).c_str(),
        BLEUtils::gattCloseReasonToString(evtParam->close.reason).c_str(), evtParam->close.conn_id
      );
      break;
    }

    // ESP_GATTC_CONNECT_EVT
    //
    // connect:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    // - esp_bd_addr_t     remote_bda
    case ESP_GATTC_CONNECT_EVT:
    {
      log_v("[conn_id: %d, remote_bda: %s]", evtParam->connect.conn_id, BLEAddress(evtParam->connect.remote_bda).toString().c_str());
      break;
    }

    // ESP_GATTC_DISCONNECT_EVT
    //
    // disconnect:
    // - esp_gatt_conn_reason_t reason
    // - uint16_t               conn_id
    // - esp_bd_addr_t          remote_bda
    case ESP_GATTC_DISCONNECT_EVT:
    {
      log_v(
        "[reason: %s, conn_id: %d, remote_bda: %s]", BLEUtils::gattCloseReasonToString(evtParam->disconnect.reason).c_str(), evtParam->disconnect.conn_id,
        BLEAddress(evtParam->disconnect.remote_bda).toString().c_str()
      );
      break;
    }  // ESP_GATTC_DISCONNECT_EVT

    // ESP_GATTC_GET_CHAR_EVT
    //
    // get_char:
    // - esp_gatt_status_t    status
    // - uin1t6_t             conn_id
    // - esp_gatt_srvc_id_t   srvc_id
    // - esp_gatt_id_t        char_id
    // - esp_gatt_char_prop_t char_prop
    /*
		case ESP_GATTC_GET_CHAR_EVT: {

			// If the status of the event shows that we have a value other than ESP_GATT_OK then the
			// characteristic fields are not set to a usable value .. so don't try and log them.
			if (evtParam->get_char.status == ESP_GATT_OK) {
				String description = "Unknown";
				if (evtParam->get_char.char_id.uuid.len == ESP_UUID_LEN_16) {
					description = BLEUtils::gattCharacteristicUUIDToString(evtParam->get_char.char_id.uuid.uuid.uuid16);
				}
				log_v("[status: %s, conn_id: %d, srvc_id: %s, char_id: %s [description: %s]\nchar_prop: %s]",
					BLEUtils::gattStatusToString(evtParam->get_char.status).c_str(),
					evtParam->get_char.conn_id,
					BLEUtils::gattServiceIdToString(evtParam->get_char.srvc_id).c_str(),
					gattIdToString(evtParam->get_char.char_id).c_str(),
					description.c_str(),
					BLEUtils::characteristicPropertiesToString(evtParam->get_char.char_prop).c_str()
				);
			} else {
				log_v("[status: %s, conn_id: %d, srvc_id: %s]",
					BLEUtils::gattStatusToString(evtParam->get_char.status).c_str(),
					evtParam->get_char.conn_id,
					BLEUtils::gattServiceIdToString(evtParam->get_char.srvc_id).c_str()
				);
			}
			break;
		} // ESP_GATTC_GET_CHAR_EVT
		*/

    // ESP_GATTC_NOTIFY_EVT
    //
    // notify
    // uint16_t           conn_id
    // esp_bd_addr_t      remote_bda
    // handle             handle
    // uint16_t           value_len
    // uint8_t*           value
    // bool               is_notify
    //
    case ESP_GATTC_NOTIFY_EVT:
    {
      log_v(
        "[conn_id: %d, remote_bda: %s, handle: %d 0x%.2x, value_len: %d, is_notify: %d]", evtParam->notify.conn_id,
        BLEAddress(evtParam->notify.remote_bda).toString().c_str(), evtParam->notify.handle, evtParam->notify.handle, evtParam->notify.value_len,
        evtParam->notify.is_notify
      );
      break;
    }

    // ESP_GATTC_OPEN_EVT
    //
    // open:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    // - esp_bd_addr_t     remote_bda
    // - uint16_t          mtu
    //
    case ESP_GATTC_OPEN_EVT:
    {
      log_v(
        "[status: %s, conn_id: %d, remote_bda: %s, mtu: %d]", BLEUtils::gattStatusToString(evtParam->open.status).c_str(), evtParam->open.conn_id,
        BLEAddress(evtParam->open.remote_bda).toString().c_str(), evtParam->open.mtu
      );
      break;
    }  // ESP_GATTC_OPEN_EVT

    // ESP_GATTC_READ_CHAR_EVT
    //
    // Callback to indicate that requested data that we wanted to read is now available.
    //
    // read:
    // esp_gatt_status_t  status
    // uint16_t           conn_id
    // uint16_t           handle
    // uint8_t*           value
    // uint16_t           value_type
    // uint16_t           value_len
    case ESP_GATTC_READ_CHAR_EVT:
    {
      log_v(
        "[status: %s, conn_id: %d, handle: %d 0x%.2x, value_len: %d]", BLEUtils::gattStatusToString(evtParam->read.status).c_str(), evtParam->read.conn_id,
        evtParam->read.handle, evtParam->read.handle, evtParam->read.value_len
      );
      if (evtParam->read.status == ESP_GATT_OK) {
        GeneralUtils::hexDump(evtParam->read.value, evtParam->read.value_len);
        /*
				char* pHexData = BLEUtils::buildHexData(nullptr, evtParam->read.value, evtParam->read.value_len);
				log_v("value: %s \"%s\"", pHexData, BLEUtils::buildPrintData(evtParam->read.value, evtParam->read.value_len).c_str());
				free(pHexData);
				*/
      }
      break;
    }  // ESP_GATTC_READ_CHAR_EVT

    // ESP_GATTC_REG_EVT
    //
    // reg:
    // - esp_gatt_status_t status
    // - uint16_t          app_id
    case ESP_GATTC_REG_EVT:
    {
      log_v("[status: %s, app_id: 0x%x]", BLEUtils::gattStatusToString(evtParam->reg.status).c_str(), evtParam->reg.app_id);
      break;
    }  // ESP_GATTC_REG_EVT

    // ESP_GATTC_REG_FOR_NOTIFY_EVT
    //
    // reg_for_notify:
    // - esp_gatt_status_t status
    // - uint16_t          handle
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    {
      log_v(
        "[status: %s, handle: %d 0x%.2x]", BLEUtils::gattStatusToString(evtParam->reg_for_notify.status).c_str(), evtParam->reg_for_notify.handle,
        evtParam->reg_for_notify.handle
      );
      break;
    }  // ESP_GATTC_REG_FOR_NOTIFY_EVT

    // ESP_GATTC_SEARCH_CMPL_EVT
    //
    // search_cmpl:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
      log_v("[status: %s, conn_id: %d]", BLEUtils::gattStatusToString(evtParam->search_cmpl.status).c_str(), evtParam->search_cmpl.conn_id);
      break;
    }  // ESP_GATTC_SEARCH_CMPL_EVT

    // ESP_GATTC_SEARCH_RES_EVT
    //
    // search_res:
    // - uint16_t      conn_id
    // - uint16_t      start_handle
    // - uint16_t      end_handle
    // - esp_gatt_id_t srvc_id
    case ESP_GATTC_SEARCH_RES_EVT:
    {
      log_v(
        "[conn_id: %d, start_handle: %d 0x%.2x, end_handle: %d 0x%.2x, srvc_id: %s", evtParam->search_res.conn_id, evtParam->search_res.start_handle,
        evtParam->search_res.start_handle, evtParam->search_res.end_handle, evtParam->search_res.end_handle,
        gattIdToString(evtParam->search_res.srvc_id).c_str()
      );
      break;
    }  // ESP_GATTC_SEARCH_RES_EVT

    // ESP_GATTC_WRITE_CHAR_EVT
    //
    // write:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    // - uint16_t          handle
    // - uint16_t          offset
    case ESP_GATTC_WRITE_CHAR_EVT:
    {
      log_v(
        "[status: %s, conn_id: %d, handle: %d 0x%.2x, offset: %d]", BLEUtils::gattStatusToString(evtParam->write.status).c_str(), evtParam->write.conn_id,
        evtParam->write.handle, evtParam->write.handle, evtParam->write.offset
      );
      break;
    }  // ESP_GATTC_WRITE_CHAR_EVT
#endif
    default: break;
  }
}  // dumpGattClientEvent

/**
 * @brief Dump the details of a GATT server event.
 * A GATT Server event is a callback received from the BLE subsystem when we are acting as a BLE
 * server.  The callback indicates the type of event in the `event` field.  The `evtParam` is a
 * union of structures where we can use the `event` to indicate which of the structures has been
 * populated and hence is valid.
 *
 * @param [in] event The event type that was posted.
 * @param [in] evtParam A union of structures only one of which is populated.
 */
void BLEUtils::dumpGattServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *evtParam) {
  log_v("GATT ServerEvent: %s", BLEUtils::gattServerEventTypeToString(event).c_str());
  switch (event) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
      log_v(
        "[status: %s, attr_handle: %d 0x%.2x, service_handle: %d 0x%.2x, char_uuid: %s]", gattStatusToString(evtParam->add_char_descr.status).c_str(),
        evtParam->add_char_descr.attr_handle, evtParam->add_char_descr.attr_handle, evtParam->add_char_descr.service_handle,
        evtParam->add_char_descr.service_handle, BLEUUID(evtParam->add_char_descr.descr_uuid).toString().c_str()
      );
      break;
    }  // ESP_GATTS_ADD_CHAR_DESCR_EVT

    case ESP_GATTS_ADD_CHAR_EVT:
    {
      if (evtParam->add_char.status == ESP_GATT_OK) {
        log_v(
          "[status: %s, attr_handle: %d 0x%.2x, service_handle: %d 0x%.2x, char_uuid: %s]", gattStatusToString(evtParam->add_char.status).c_str(),
          evtParam->add_char.attr_handle, evtParam->add_char.attr_handle, evtParam->add_char.service_handle, evtParam->add_char.service_handle,
          BLEUUID(evtParam->add_char.char_uuid).toString().c_str()
        );
      } else {
        log_e(
          "[status: %s, attr_handle: %d 0x%.2x, service_handle: %d 0x%.2x, char_uuid: %s]", gattStatusToString(evtParam->add_char.status).c_str(),
          evtParam->add_char.attr_handle, evtParam->add_char.attr_handle, evtParam->add_char.service_handle, evtParam->add_char.service_handle,
          BLEUUID(evtParam->add_char.char_uuid).toString().c_str()
        );
      }
      break;
    }  // ESP_GATTS_ADD_CHAR_EVT

    // ESP_GATTS_CONF_EVT
    //
    // conf:
    // - esp_gatt_status_t status  – The status code.
    // - uint16_t          conn_id – The connection used.
    case ESP_GATTS_CONF_EVT:
    {
      log_v("[status: %s, conn_id: 0x%.2x]", gattStatusToString(evtParam->conf.status).c_str(), evtParam->conf.conn_id);
      break;
    }  // ESP_GATTS_CONF_EVT

    case ESP_GATTS_CONGEST_EVT:
    {
      log_v("[conn_id: %d, congested: %d]", evtParam->congest.conn_id, evtParam->congest.congested);
      break;
    }  // ESP_GATTS_CONGEST_EVT

    case ESP_GATTS_CONNECT_EVT:
    {
      log_v("[conn_id: %d, remote_bda: %s]", evtParam->connect.conn_id, BLEAddress(evtParam->connect.remote_bda).toString().c_str());
      break;
    }  // ESP_GATTS_CONNECT_EVT

    case ESP_GATTS_CREATE_EVT:
    {
      log_v(
        "[status: %s, service_handle: %d 0x%.2x, service_id: [%s]]", gattStatusToString(evtParam->create.status).c_str(), evtParam->create.service_handle,
        evtParam->create.service_handle, gattServiceIdToString(evtParam->create.service_id).c_str()
      );
      break;
    }  // ESP_GATTS_CREATE_EVT

    case ESP_GATTS_DISCONNECT_EVT:
    {
      log_v("[conn_id: %d, remote_bda: %s]", evtParam->connect.conn_id, BLEAddress(evtParam->connect.remote_bda).toString().c_str());
      break;
    }  // ESP_GATTS_DISCONNECT_EVT

    // ESP_GATTS_EXEC_WRITE_EVT
    // exec_write:
    // - uint16_t conn_id
    // - uint32_t trans_id
    // - esp_bd_addr_t bda
    // - uint8_t exec_write_flag
#ifdef ARDUHAL_LOG_LEVEL_VERBOSE
    case ESP_GATTS_EXEC_WRITE_EVT:
    {
      char *pWriteFlagText;
      switch (evtParam->exec_write.exec_write_flag) {
        case ESP_GATT_PREP_WRITE_EXEC:
        {
          pWriteFlagText = (char *)"WRITE";
          break;
        }

        case ESP_GATT_PREP_WRITE_CANCEL:
        {
          pWriteFlagText = (char *)"CANCEL";
          break;
        }

        default: pWriteFlagText = (char *)"<Unknown>"; break;
      }

      log_v(
        "[conn_id: %d, trans_id: %d, bda: %s, exec_write_flag: 0x%.2x=%s]", evtParam->exec_write.conn_id, evtParam->exec_write.trans_id,
        BLEAddress(evtParam->exec_write.bda).toString().c_str(), evtParam->exec_write.exec_write_flag, pWriteFlagText
      );
      break;
    }  // ESP_GATTS_DISCONNECT_EVT
#endif

    case ESP_GATTS_MTU_EVT:
    {
      log_v("[conn_id: %d, mtu: %d]", evtParam->mtu.conn_id, evtParam->mtu.mtu);
      break;
    }  // ESP_GATTS_MTU_EVT

    case ESP_GATTS_READ_EVT:
    {
      log_v(
        "[conn_id: %d, trans_id: %d, bda: %s, handle: 0x%.2x, is_long: %d, need_rsp:%d]", evtParam->read.conn_id, evtParam->read.trans_id,
        BLEAddress(evtParam->read.bda).toString().c_str(), evtParam->read.handle, evtParam->read.is_long, evtParam->read.need_rsp
      );
      break;
    }  // ESP_GATTS_READ_EVT

    case ESP_GATTS_RESPONSE_EVT:
    {
      log_v("[status: %s, handle: 0x%.2x]", gattStatusToString(evtParam->rsp.status).c_str(), evtParam->rsp.handle);
      break;
    }  // ESP_GATTS_RESPONSE_EVT

    case ESP_GATTS_REG_EVT:
    {
      log_v("[status: %s, app_id: %d]", gattStatusToString(evtParam->reg.status).c_str(), evtParam->reg.app_id);
      break;
    }  // ESP_GATTS_REG_EVT

    // ESP_GATTS_START_EVT
    //
    // start:
    // - esp_gatt_status_t status
    // - uint16_t          service_handle
    case ESP_GATTS_START_EVT:
    {
      log_v("[status: %s, service_handle: 0x%.2x]", gattStatusToString(evtParam->start.status).c_str(), evtParam->start.service_handle);
      break;
    }  // ESP_GATTS_START_EVT

    // ESP_GATTS_WRITE_EVT
    //
    // write:
    // - uint16_t      conn_id  – The connection id.
    // - uint16_t      trans_id – The transfer id.
    // - esp_bd_addr_t bda      – The address of the partner.
    // - uint16_t      handle   – The attribute handle.
    // - uint16_t      offset   – The offset of the currently received within the whole value.
    // - bool          need_rsp – Do we need a response?
    // - bool          is_prep  – Is this a write prepare?  If set, then this is to be considered part of the received value and not the whole value.  A subsequent ESP_GATTS_EXEC_WRITE will mark the total.
    // - uint16_t      len      – The length of the incoming value part.
    // - uint8_t*      value    – The data for this value part.
    case ESP_GATTS_WRITE_EVT:
    {
      log_v(
        "[conn_id: %d, trans_id: %d, bda: %s, handle: 0x%.2x, offset: %d, need_rsp: %d, is_prep: %d, len: %d]", evtParam->write.conn_id,
        evtParam->write.trans_id, BLEAddress(evtParam->write.bda).toString().c_str(), evtParam->write.handle, evtParam->write.offset, evtParam->write.need_rsp,
        evtParam->write.is_prep, evtParam->write.len
      );
      char *pHex = buildHexData(nullptr, evtParam->write.value, evtParam->write.len);
      log_v("[Data: %s]", pHex);
      free(pHex);
      break;
    }  // ESP_GATTS_WRITE_EVT
#endif
    default: log_v("dumpGattServerEvent: *** NOT CODED ***"); break;
  }
}  // dumpGattServerEvent

/**
 * @brief Convert a BLE event type to a string.
 * @param [in] eventType The event type.
 * @return The event type as a string.
 */
const char *BLEUtils::eventTypeToString(esp_ble_evt_type_t eventType) {
  switch (eventType) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_BLE_EVT_CONN_ADV:     return "ESP_BLE_EVT_CONN_ADV";
    case ESP_BLE_EVT_CONN_DIR_ADV: return "ESP_BLE_EVT_CONN_DIR_ADV";
    case ESP_BLE_EVT_DISC_ADV:     return "ESP_BLE_EVT_DISC_ADV";
    case ESP_BLE_EVT_NON_CONN_ADV: return "ESP_BLE_EVT_NON_CONN_ADV";
    case ESP_BLE_EVT_SCAN_RSP:     return "ESP_BLE_EVT_SCAN_RSP";
#endif
    default: log_v("Unknown esp_ble_evt_type_t: %d (0x%.2x)", eventType, eventType); return "*** Unknown ***";
  }
}  // eventTypeToString

/**
 * @brief Convert a BT GAP event type to a string representation.
 * @param [in] eventType The type of event.
 * @return A string representation of the event type.
 */
const char *BLEUtils::gapEventToString(uint32_t eventType) {
  switch (eventType) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:          return "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT";
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:      return "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT";
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:             return "ESP_GAP_BLE_ADV_START_COMPLETE_EVT";
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:              /* !< When stop adv complete, the event comes */ return "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT";
    case ESP_GAP_BLE_AUTH_CMPL_EVT:                      /* Authentication complete indication. */ return "ESP_GAP_BLE_AUTH_CMPL_EVT";
    case ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT:        return "ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT";
    case ESP_GAP_BLE_GET_BOND_DEV_COMPLETE_EVT:          return "ESP_GAP_BLE_GET_BOND_DEV_COMPLETE_EVT";
    case ESP_GAP_BLE_KEY_EVT:                            /* BLE  key event for peer device keys */ return "ESP_GAP_BLE_KEY_EVT";
    case ESP_GAP_BLE_LOCAL_IR_EVT:                       /* BLE local IR event */ return "ESP_GAP_BLE_LOCAL_IR_EVT";
    case ESP_GAP_BLE_LOCAL_ER_EVT:                       /* BLE local ER event */ return "ESP_GAP_BLE_LOCAL_ER_EVT";
    case ESP_GAP_BLE_NC_REQ_EVT:                         /* Numeric Comparison request event */ return "ESP_GAP_BLE_NC_REQ_EVT";
    case ESP_GAP_BLE_OOB_REQ_EVT:                        /* OOB request event */ return "ESP_GAP_BLE_OOB_REQ_EVT";
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:                  /* passkey notification event */ return "ESP_GAP_BLE_PASSKEY_NOTIF_EVT";
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                    /* passkey request event */ return "ESP_GAP_BLE_PASSKEY_REQ_EVT";
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:             return "ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT";
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:       return "ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT";
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:        return "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT";
    case ESP_GAP_BLE_SCAN_RESULT_EVT:                    return "ESP_GAP_BLE_SCAN_RESULT_EVT";
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT: return "ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT";
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:     return "ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT";
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:            return "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT";
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:             return "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT";
    case ESP_GAP_BLE_SEC_REQ_EVT:                        /* BLE  security request */ return "ESP_GAP_BLE_SEC_REQ_EVT";
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:     return "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT";
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:        return "ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT";
    case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:           return "ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT";
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:             return "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT";
#endif
    default: log_v("gapEventToString: Unknown event type %d 0x%.2x", eventType, eventType); return "Unknown event type";
  }
}  // gapEventToString

String BLEUtils::gattCharacteristicUUIDToString(uint32_t characteristicUUID) {
  const characteristicMap_t *p = g_characteristicsMappings;
  while (strlen(p->name) > 0) {
    if (p->assignedNumber == characteristicUUID) {
      return String(p->name);
    }
    p++;
  }
  return "Unknown";
}  // gattCharacteristicUUIDToString

/**
 * @brief Given the UUID for a BLE defined descriptor, return its string representation.
 * @param [in] descriptorUUID UUID of the descriptor to be returned as a string.
 * @return The string representation of a descriptor UUID.
 */
String BLEUtils::gattDescriptorUUIDToString(uint32_t descriptorUUID) {
  gattdescriptor_t *p = (gattdescriptor_t *)g_descriptor_ids;
  while (strlen(p->name) > 0) {
    if (p->assignedNumber == descriptorUUID) {
      return String(p->name);
    }
    p++;
  }
  return "";
}  // gattDescriptorUUIDToString

/**
 * @brief Return a string representation of an esp_gattc_service_elem_t.
 * @return A string representation of an esp_gattc_service_elem_t.
 */
String BLEUtils::gattcServiceElementToString(esp_gattc_service_elem_t *pGATTCServiceElement) {
  String res;
  char val[6];
  res += "[uuid: " + BLEUUID(pGATTCServiceElement->uuid).toString() + ", start_handle: ";
  snprintf(val, sizeof(val), "%d", pGATTCServiceElement->start_handle);
  res += val;
  res += " 0x";
  snprintf(val, sizeof(val), "%04x", pGATTCServiceElement->start_handle);
  res += val;
  res += ", end_handle: ";
  snprintf(val, sizeof(val), "%d", pGATTCServiceElement->end_handle);
  res += val;
  res += " 0x";
  snprintf(val, sizeof(val), "%04x", pGATTCServiceElement->end_handle);
  res += val;
  res += "]";
  return res;
}  // gattcServiceElementToString

/**
 * @brief Convert an esp_gatt_srvc_id_t to a string.
 */
String BLEUtils::gattServiceIdToString(esp_gatt_srvc_id_t srvcId) {
  return gattIdToString(srvcId.id);
}  // gattServiceIdToString

String BLEUtils::gattServiceToString(uint32_t serviceId) {
  gattService_t *p = (gattService_t *)g_gattServices;
  while (strlen(p->name) > 0) {
    if (p->assignedNumber == serviceId) {
      return String(p->name);
    }
    p++;
  }
  return "Unknown";
}  // gattServiceToString

/**
 * @brief Convert a GATT status to a string.
 *
 * @param [in] status The status to convert.
 * @return A string representation of the status.
 */
String BLEUtils::gattStatusToString(esp_gatt_status_t status) {
  switch (status) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_GATT_OK:                   return "ESP_GATT_OK";
    case ESP_GATT_INVALID_HANDLE:       return "ESP_GATT_INVALID_HANDLE";
    case ESP_GATT_READ_NOT_PERMIT:      return "ESP_GATT_READ_NOT_PERMIT";
    case ESP_GATT_WRITE_NOT_PERMIT:     return "ESP_GATT_WRITE_NOT_PERMIT";
    case ESP_GATT_INVALID_PDU:          return "ESP_GATT_INVALID_PDU";
    case ESP_GATT_INSUF_AUTHENTICATION: return "ESP_GATT_INSUF_AUTHENTICATION";
    case ESP_GATT_REQ_NOT_SUPPORTED:    return "ESP_GATT_REQ_NOT_SUPPORTED";
    case ESP_GATT_INVALID_OFFSET:       return "ESP_GATT_INVALID_OFFSET";
    case ESP_GATT_INSUF_AUTHORIZATION:  return "ESP_GATT_INSUF_AUTHORIZATION";
    case ESP_GATT_PREPARE_Q_FULL:       return "ESP_GATT_PREPARE_Q_FULL";
    case ESP_GATT_NOT_FOUND:            return "ESP_GATT_NOT_FOUND";
    case ESP_GATT_NOT_LONG:             return "ESP_GATT_NOT_LONG";
    case ESP_GATT_INSUF_KEY_SIZE:       return "ESP_GATT_INSUF_KEY_SIZE";
    case ESP_GATT_INVALID_ATTR_LEN:     return "ESP_GATT_INVALID_ATTR_LEN";
    case ESP_GATT_ERR_UNLIKELY:         return "ESP_GATT_ERR_UNLIKELY";
    case ESP_GATT_INSUF_ENCRYPTION:     return "ESP_GATT_INSUF_ENCRYPTION";
    case ESP_GATT_UNSUPPORT_GRP_TYPE:   return "ESP_GATT_UNSUPPORT_GRP_TYPE";
    case ESP_GATT_INSUF_RESOURCE:       return "ESP_GATT_INSUF_RESOURCE";
    case ESP_GATT_NO_RESOURCES:         return "ESP_GATT_NO_RESOURCES";
    case ESP_GATT_INTERNAL_ERROR:       return "ESP_GATT_INTERNAL_ERROR";
    case ESP_GATT_WRONG_STATE:          return "ESP_GATT_WRONG_STATE";
    case ESP_GATT_DB_FULL:              return "ESP_GATT_DB_FULL";
    case ESP_GATT_BUSY:                 return "ESP_GATT_BUSY";
    case ESP_GATT_ERROR:                return "ESP_GATT_ERROR";
    case ESP_GATT_CMD_STARTED:          return "ESP_GATT_CMD_STARTED";
    case ESP_GATT_ILLEGAL_PARAMETER:    return "ESP_GATT_ILLEGAL_PARAMETER";
    case ESP_GATT_PENDING:              return "ESP_GATT_PENDING";
    case ESP_GATT_AUTH_FAIL:            return "ESP_GATT_AUTH_FAIL";
    case ESP_GATT_MORE:                 return "ESP_GATT_MORE";
    case ESP_GATT_INVALID_CFG:          return "ESP_GATT_INVALID_CFG";
    case ESP_GATT_SERVICE_STARTED:      return "ESP_GATT_SERVICE_STARTED";
    case ESP_GATT_ENCRYPTED_NO_MITM:    return "ESP_GATT_ENCRYPTED_NO_MITM";
    case ESP_GATT_NOT_ENCRYPTED:        return "ESP_GATT_NOT_ENCRYPTED";
    case ESP_GATT_CONGESTED:            return "ESP_GATT_CONGESTED";
    case ESP_GATT_DUP_REG:              return "ESP_GATT_DUP_REG";
    case ESP_GATT_ALREADY_OPEN:         return "ESP_GATT_ALREADY_OPEN";
    case ESP_GATT_CANCEL:               return "ESP_GATT_CANCEL";
    case ESP_GATT_STACK_RSP:            return "ESP_GATT_STACK_RSP";
    case ESP_GATT_APP_RSP:              return "ESP_GATT_APP_RSP";
    case ESP_GATT_UNKNOWN_ERROR:        return "ESP_GATT_UNKNOWN_ERROR";
    case ESP_GATT_CCC_CFG_ERR:          return "ESP_GATT_CCC_CFG_ERR";
    case ESP_GATT_PRC_IN_PROGRESS:      return "ESP_GATT_PRC_IN_PROGRESS";
    case ESP_GATT_OUT_OF_RANGE:         return "ESP_GATT_OUT_OF_RANGE";
#endif
    default: return "Unknown";
  }
}  // gattStatusToString

String BLEUtils::getMember(uint32_t memberId) {
  member_t *p = (member_t *)members_ids;

  while (strlen(p->name) > 0) {
    if (p->assignedNumber == memberId) {
      return String(p->name);
    }
    p++;
  }
  return "Unknown";
}

/**
 * @brief convert a GAP search event to a string.
 * @param [in] searchEvt
 * @return The search event type as a string.
 */
const char *BLEUtils::searchEventTypeToString(esp_gap_search_evt_t searchEvt) {
  switch (searchEvt) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    case ESP_GAP_SEARCH_INQ_RES_EVT:            return "ESP_GAP_SEARCH_INQ_RES_EVT";
    case ESP_GAP_SEARCH_INQ_CMPL_EVT:           return "ESP_GAP_SEARCH_INQ_CMPL_EVT";
    case ESP_GAP_SEARCH_DISC_RES_EVT:           return "ESP_GAP_SEARCH_DISC_RES_EVT";
    case ESP_GAP_SEARCH_DISC_BLE_RES_EVT:       return "ESP_GAP_SEARCH_DISC_BLE_RES_EVT";
    case ESP_GAP_SEARCH_DISC_CMPL_EVT:          return "ESP_GAP_SEARCH_DISC_CMPL_EVT";
    case ESP_GAP_SEARCH_DI_DISC_CMPL_EVT:       return "ESP_GAP_SEARCH_DI_DISC_CMPL_EVT";
    case ESP_GAP_SEARCH_SEARCH_CANCEL_CMPL_EVT: return "ESP_GAP_SEARCH_SEARCH_CANCEL_CMPL_EVT";
#endif
    default: log_v("Unknown event type: 0x%x", searchEvt); return "Unknown event type";
  }
}  // searchEventTypeToString

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
