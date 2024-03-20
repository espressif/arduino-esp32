/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "NetworkEvents.h"
#include "IPAddress.h"
#include "WString.h"

class NetworkManager : public NetworkEvents, public Printable {
public:
	NetworkManager();

	bool begin();
	int hostByName(const char *aHostname, IPAddress &aResult, bool preferV6=false);
	uint8_t * macAddress(uint8_t * mac);
	String macAddress();

    size_t printTo(Print & out) const;

    static const char * getHostname();
    static bool setHostname(const char * hostname);
    static bool hostname(const String& aHostname) { return setHostname(aHostname.c_str()); }
};

extern NetworkManager Network;
