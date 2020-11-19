/*
    RMakerParam.cpp - ESP RainMaker param support.
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

#include "RMakerParam.h"

static esp_err_t err;

esp_err_t Param::addUIType(const char *ui_type)
{
    err = esp_rmaker_param_add_ui_type(param_handle, ui_type);
    if(err != ESP_OK) {
        log_e("Add UI type error");
    }
    return err;
}

esp_err_t Param::addBounds(param_val_t min, param_val_t max, param_val_t step)
{
    err = esp_rmaker_param_add_bounds(param_handle, min, max, step);
    if(err != ESP_OK) {
        log_e("Add Bounds error");
    }
    return err;
}

esp_err_t Param::updateAndReport(param_val_t val)
{
    err = esp_rmaker_param_update_and_report(getParamHandle(), val);
    if(err != ESP_OK){
        log_e("Update and Report param failed");
    }
    return err;
}
