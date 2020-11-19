/*
    RMakerType.h - ESP RainMaker type of value support.
    Copyright (c) 2020 Arduino. All right reserved.
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
  
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
  
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_err.h>
#include <esp32-hal.h>

typedef esp_rmaker_node_t* node_t;
typedef esp_rmaker_node_info_t node_info_t;
typedef esp_rmaker_param_val_t param_val_t;
typedef esp_rmaker_write_ctx_t write_ctx_t;
typedef esp_rmaker_read_ctx_t read_ctx_t;
typedef esp_rmaker_device_t device_handle_t;
typedef esp_rmaker_param_t param_handle_t;
typedef esp_rmaker_ota_type_t ota_type_t;

param_val_t value(int);
param_val_t value(bool);
param_val_t value(char *);
param_val_t value(float);
