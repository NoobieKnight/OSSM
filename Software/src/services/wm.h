#ifndef OSSM_WM_H
#define OSSM_WM_H

#include "Arduino.h"

enum class BleState {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ADVERTISING = 3,
    ERROR = 4
};

void initWM();
bool setWiFiCredentials(const String& ssid, const String& password);
bool connectWiFi();
String getWiFiStatus();
BleState getBleState();


#endif  // OSSM_WM_H
