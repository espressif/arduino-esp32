/*
    RMakerType.cpp - ESP RainMaker type of value support.
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

param_val_t value(int ival)
{
    return esp_rmaker_int(ival);
}

param_val_t value(bool bval)
{
    return esp_rmaker_bool(bval);
}

param_val_t value(char *sval)
{
    return esp_rmaker_str(sval);
}

param_val_t value(float fval)
{
    return esp_rmaker_float(fval);
}
