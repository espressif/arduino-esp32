/*
    RMakerParam.h - ESP RainMaker param support.
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

#include "RMakerType.h"

class Param
{
    private:
        const param_handle_t *param_handle;
       
    public:
        Param()
        {
            param_handle = NULL;
        }
        Param(const char *param_name, const char *param_type, param_val_t val, uint8_t properties)
        {
            param_handle = esp_rmaker_param_create(param_name, param_type, val, properties);
        }
        void setParamHandle(const param_handle_t *param_handle) 
        {
            this->param_handle = param_handle;
        }
        const char *getParamName()
        {
            return esp_rmaker_param_get_name(param_handle);
        }
        const param_handle_t *getParamHandle()
        {
            return param_handle;
        } 
         
        esp_err_t addUIType(const char *ui_type);
        esp_err_t addBounds(param_val_t min, param_val_t max, param_val_t step);
        esp_err_t updateAndReport(param_val_t val);
};
