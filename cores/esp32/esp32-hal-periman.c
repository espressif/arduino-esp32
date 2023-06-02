/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32-hal-log.h"
#include "esp32-hal-periman.h"
#include "esp_bit_defs.h"

typedef struct {
	peripheral_bus_type_t type;
	void * bus;
} peripheral_pin_item_t;

static peripheral_bus_deinit_cb_t deinit_functions[ESP32_BUS_TYPE_MAX];
static peripheral_pin_item_t pins[SOC_GPIO_PIN_COUNT];

#define GPIO_NOT_VALID(p) ((p >= SOC_GPIO_PIN_COUNT) || ((SOC_GPIO_VALID_GPIO_MASK & (1ULL << p)) == 0))

bool perimanSetPinBus(uint8_t pin, peripheral_bus_type_t type, void * bus){
	peripheral_bus_type_t otype = ESP32_BUS_TYPE_INIT;
	void * obus = NULL;
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return false;
	}
	if(type >= ESP32_BUS_TYPE_MAX){
		log_e("Invalid type: %u", (unsigned int)type);
		return false;
	}
	if(type > ESP32_BUS_TYPE_GPIO && bus == NULL){
		log_e("Bus is NULL");
		return false;
	}
	if (type == ESP32_BUS_TYPE_INIT && bus != NULL){
		log_e("Can't set a Bus to INIT Type");
		return false;
	}	
	otype = pins[pin].type;
	obus = pins[pin].bus;
	if(type == otype && bus == obus){
		if (type != ESP32_BUS_TYPE_INIT) {
		        log_i("Bus already set");
		}
		return true;
	}
	if(obus != NULL){
		if(deinit_functions[otype] == NULL){
			log_e("Bus does not have deinit function set");
			return false;
		}
		if(!deinit_functions[otype](obus)){
			log_e("Previous bus failed to deinit");
			return false;
		}
	}
	pins[pin].type = type;
	pins[pin].bus = bus;
	return true;
}

void * perimanGetPinBus(uint8_t pin, peripheral_bus_type_t type){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return NULL;
	}
	if(type >= ESP32_BUS_TYPE_MAX  || type  == ESP32_BUS_TYPE_INIT){
		log_e("Invalid type: %u", (unsigned int)type);
		return NULL;
	}
	if(pins[pin].type == type){
		return pins[pin].bus;
	}
	return NULL;
}

peripheral_bus_type_t perimanGetPinBusType(uint8_t pin){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return ESP32_BUS_TYPE_MAX;
	}
	return pins[pin].type;
}

bool perimanSetBusDeinit(peripheral_bus_type_t type, peripheral_bus_deinit_cb_t cb){
	if(type >= ESP32_BUS_TYPE_MAX || type == ESP32_BUS_TYPE_INIT){
		log_e("Invalid type: %u", (unsigned int)type);
		return false;
	}
	if(cb == NULL){
		log_e("Callback is NULL");
		return false;
	}
	deinit_functions[type] = cb;
	return true;
}

bool perimanPinIsValid(uint8_t pin){
	return !(GPIO_NOT_VALID(pin));
}
