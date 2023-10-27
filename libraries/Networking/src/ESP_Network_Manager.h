#pragma once

#include "ESP_Network_Events.h"
#include "IPAddress.h"

class ESP_Network_Manager : public ESP_Network_Events {
public:
	ESP_Network_Manager();

	bool begin();
	int hostByName(const char *aHostname, IPAddress &aResult);
};

extern ESP_Network_Manager Network;
