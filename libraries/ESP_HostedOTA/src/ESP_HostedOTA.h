#pragma once

#include <stdbool.h>

/**
 * Updates the ESP-Hosted co-processor firmware over-the-air (OTA)
 * This function downloads and installs firmware updates for the ESP-Hosted slave device
 * @return true if update was successful, false otherwise
 */
bool updateEspHostedSlave();
