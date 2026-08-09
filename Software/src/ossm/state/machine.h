#ifndef OSSM_STATE_MACHINE_H
#define OSSM_STATE_MACHINE_H

#include "boost/sml.hpp"

#include "actions.h"
#include "guards.h"
#include "../Events.h"

namespace sml = boost::sml;

struct OSSMStateMachine {
    auto operator()() const {
        using namespace sml;
        using namespace actions;
        using namespace guards;

        return make_transition_table(
        // clang-format off

            *"idle"_s + done = "homing"_s,

            "homing"_s / startHoming = "homing.forward"_s,
            "homing.forward"_s + error = "error"_s,
            "homing.forward"_s + done / startHoming = "homing.backward"_s,
            "homing.backward"_s + error = "error"_s,
            "homing.backward"_s + done[(isStrokeTooShort)] = "error"_s,
            "homing.backward"_s + done[isFirstHomed] / setHomed = "menu"_s,
            "homing.backward"_s + done[(isOption(Menu::StrokeEngine))] / setHomed = "strokeEngine"_s,
            "homing.backward"_s + done[(isOption(Menu::Streaming))] / setHomed = "streaming"_s,

            "menu"_s = "menu.idle"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::StrokeEngine))] = "strokeEngine"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Streaming))] = "streaming"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Pairing)) && isOnline] = "pairing"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Pairing)) && !isOnline] = "pairing.wifi"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::WiFiSetup))] = "wifi"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Restart))] = "restart"_s,

            "strokeEngine"_s [isNotHomed] = "homing"_s,
            "strokeEngine"_s / (resetSettingsStrokeEngine, startStrokeEngine) = "strokeEngine.active"_s,
            // Exit
            "strokeEngine.active"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,
            "strokeEngine.active"_s + event<ReturnToMenu> / emergencyStop = "menu"_s,

            "streaming"_s [isNotHomed] = "homing"_s,
            "streaming"_s / (resetSettingsStreaming, startStreaming) = "streaming.active"_s,
            // Exit
            "streaming.active"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,
            "streaming.active"_s + event<ReturnToMenu> / emergencyStop = "menu"_s,

            "pairing"_s / checkPairing = "pairing.idle"_s,
            "pairing.idle"_s + done = "pairing.success"_s,
            // Exit
            "pairing.idle"_s + buttonPress = "menu"_s,
            "pairing.idle"_s + longPress = "menu"_s,
            "pairing.idle"_s + error = "menu"_s,

            "pairing.success"_s = "pairing.success.idle"_s,
            // Exit
            "pairing.success.idle"_s + buttonPress = "menu"_s,
            "pairing.success.idle"_s + longPress = "menu"_s,

            "pairing.wifi"_s = "pairing.wifi.idle"_s,
            "pairing.wifi.idle"_s + done = "pairing"_s,
            // Exit
            "pairing.wifi.idle"_s + buttonPress = "menu"_s,
            "pairing.wifi.idle"_s + longPress = "menu"_s,

            "wifi"_s = "wifi.idle"_s,
            "wifi.idle"_s + done / stopWifiPortal = "menu"_s,
            "wifi.idle"_s + buttonPress / stopWifiPortal = "menu"_s,
            "wifi.idle"_s + longPress / resetWiFi = "restart"_s,

            "error"_s = "error.idle"_s,
            "error.idle"_s + buttonPress / restart = X,

            "restart"_s / restart = X);

        // clang-format on
    }
};

#endif  // OSSM_STATE_MACHINE_H
