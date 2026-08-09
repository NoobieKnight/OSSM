#ifndef OSSM_STATE_ACTIONS_H
#define OSSM_STATE_ACTIONS_H

// Forward declarations for action implementations (defined in actions.cpp)
void ossmStartHoming();
void ossmStartStreaming();
void ossmResetSettingsStrokeEngine();
void ossmResetSettingsStreaming();
void ossmStartStrokeEngine();
void ossmEmergencyStop();
void ossmSetHomed();
void ossmSetNotHomed();
void ossmCheckPairing();
void ossmResetWiFi();
void ossmRestart();

namespace actions {

    constexpr auto startHoming = []() { ossmStartHoming(); };

    constexpr auto startStreaming = []() { ossmStartStreaming(); };

    constexpr auto resetSettingsStrokeEngine = []() { ossmResetSettingsStrokeEngine(); };

    constexpr auto resetSettingsStreaming = []() { ossmResetSettingsStreaming(); };

    constexpr auto startStrokeEngine = []() { ossmStartStrokeEngine(); };

    constexpr auto emergencyStop = []() { ossmEmergencyStop(); };

    constexpr auto stopWifiPortal = []() {};

    constexpr auto resetWiFi = []() { ossmResetWiFi(); };

    constexpr auto checkPairing = []() { ossmCheckPairing(); };

    constexpr auto setHomed = []() { ossmSetHomed(); };

    constexpr auto setNotHomed = []() { ossmSetNotHomed(); };

    constexpr auto restart = []() { ossmRestart(); };

}  // namespace actions

#endif  // OSSM_STATE_ACTIONS_H
