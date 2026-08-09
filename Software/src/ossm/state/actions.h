#ifndef OSSM_STATE_ACTIONS_H
#define OSSM_STATE_ACTIONS_H

// Forward declarations for action implementations (defined in actions.cpp)
void ossmStartHoming();
void ossmDrawPlayControls();
void ossmStartStreaming();
void ossmDrawPatternControls();
void ossmResetSettingsStrokeEngine();
void ossmResetSettingsSimplePen();
void ossmResetSettingsStreaming();
void ossmIncrementControlStrokeEngine();
void ossmIncrementControlStreaming();
void ossmStartSimplePenetration();
void ossmStartStrokeEngine();
void ossmEmergencyStop();
void ossmSetHomed();
void ossmSetNotHomed();
void ossmCheckPairing();
void ossmResetWiFi();
void ossmRestart();

namespace actions {

    constexpr auto startHoming = []() { ossmStartHoming(); };

    constexpr auto drawPlayControls = []() { ossmDrawPlayControls(); };

    constexpr auto startStreaming = []() { ossmStartStreaming(); };

    constexpr auto drawPatternControls = []() { ossmDrawPatternControls(); };

    constexpr auto resetSettingsStrokeEngine = []() { ossmResetSettingsStrokeEngine(); };

    constexpr auto resetSettingsSimplePen = []() { ossmResetSettingsSimplePen(); };

    constexpr auto resetSettingsStreaming = []() { ossmResetSettingsStreaming(); };

    constexpr auto incrementControlStrokeEngine = []() { ossmIncrementControlStrokeEngine(); };

    constexpr auto incrementControlStreaming = []() { ossmIncrementControlStreaming(); };

    constexpr auto startSimplePenetration = []() { ossmStartSimplePenetration(); };

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
