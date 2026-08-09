#ifndef OSSM_HEADERBAR_H
#define OSSM_HEADERBAR_H

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "services/wm.h"

enum class BleStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ADVERTISING = 3,
    ERROR = 4
};

enum class WifiStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};


WifiStatus getWifiStatus();
BleStatus getBleStatus();

[[noreturn]] void headerBarTask(void* pvParameters);
void initHeaderBar();

extern TaskHandle_t headerBarTaskHandle;

#endif  // OSSM_HEADERBAR_H
