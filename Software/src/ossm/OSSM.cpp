#include "OSSM.h"

#include "command/commands.hpp"
#include "ossm/state/ble.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/communication/mqtt.h"
#include "services/communication/queue.h"
#include "services/stepper.h"

namespace sml = boost::sml;
using namespace sml;

// Global OSSM pointer (kept for backward compatibility during migration)
OSSM *ossm = nullptr;

// Static member definition - now forwards to global settings
SettingPercents OSSM::setting = {.speed = 0,
                                 .stroke = 50,
                                 .sensation = 50,
                                 .depth = 10,
                                 .buffer = 100,
                                 .pattern = StrokePatterns::SimpleStroke};

OSSM::OSSM() {
    // Initialize global state from OSSM::setting
    settings = OSSM::setting;
}

void OSSM::ble_click(String commandString) {
    CommandValue command = commandFromString(commandString);
    ESP_LOGD("OSSM", "COMMAND: %d", command.command);

    String currentState;
    if (stateMachine != nullptr) {
        stateMachine->visit_current_states(
            [&currentState](auto state) { currentState = state.c_str(); });
    }

    switch (command.command) {
        case Commands::goToStrokeEngine:
            menuState.currentOption = Menu::StrokeEngine;
            if (stateMachine != nullptr) {
                stateMachine->process_event(ButtonPress{});
            }
            break;
        case Commands::goToStreaming:
            menuState.currentOption = Menu::Streaming;
            if (stateMachine != nullptr) {
                stateMachine->process_event(ButtonPress{});
            }
            break;
        case Commands::goToMenu:
            if (stateMachine != nullptr) {
                stateMachine->process_event(ReturnToMenu{});
            }
            break;
        case Commands::setSpeed:
            bleState.lastSpeedCommandWasFromBLE = true;
            settings.speedBLE = command.value;
            settings.speed = command.value;
            break;
        case Commands::setStroke:
            session.playControl = PlayControls::STROKE;
            settings.stroke = command.value;
            break;
        case Commands::setDepth:
            session.playControl = PlayControls::DEPTH;
            settings.depth = command.value;
            break;
        case Commands::setSensation:
            session.playControl = PlayControls::SENSATION;
            settings.sensation = command.value;
            break;
        case Commands::setBuffer:
            session.playControl = PlayControls::BUFFER;
            settings.buffer = command.value;
            break;
        case Commands::setPattern:
            settings.pattern = static_cast<StrokePatterns>(command.value % 9);
            break;
        case Commands::streamPosition:{
            // Position (0-100)
            PositionTime addItem;
            addItem.position = static_cast<uint8_t>(command.value);
            addItem.inTime = static_cast<uint16_t>(command.time);
            addItem.setTime = std::chrono::steady_clock::now();

            xQueueSend(rawQueue, &addItem, portMAX_DELAY);
            break;
        }
        case Commands::setWifi:
        case Commands::ignore:
            break;
    }
}

String OSSM::getStateFingerprint() {
    String currentState;
    if (stateMachine != nullptr) {
        stateMachine->visit_current_states(
            [&currentState](auto state) { currentState = state.c_str(); });
    }

    String output = currentState + ":";
    output += String((int)settings.speed) + ":";
    output += String((int)settings.stroke) + ":";
    output += String((int)settings.sensation) + ":";
    output += String((int)settings.depth) + ":";
    output += String(static_cast<int>(settings.pattern)) + ":";
    output += sessionId;
    return output;
}

// ┌──────────────────────────────────────────────────────────────────────┐
// │ MQTT TELEMETRY PAYLOAD — SHARED CONTRACT WITH RAD DASHBOARD        │
// │                                                                    │
// │ This payload is published via MQTT to `ossm/{macAddress}` and      │
// │ received by the Dashboard at:                                      │
// │   rad-app/src/app/api/lockbox/event/ossm/[mac]/route.ts            │
// │                                                                    │
// │ The Dashboard validates it with a Zod schema (payloadSchema).      │
// │ ANY change here MUST be mirrored in that Zod schema, and vice      │
// │ versa, or telemetry ingestion will silently fail (400 Bad Request). │
// │                                                                    │
// │ Required fields (all must be present):                             │
// │   timestamp  : number   — millis() uptime                         │
// │   state      : string   — Boost.SML state name                    │
// │   speed      : integer  — cast from float                         │
// │   stroke     : integer  — cast from float                         │
// │   sensation  : integer  — cast from float                         │
// │   depth      : integer  — cast from float                         │
// │   pattern    : integer  — StrokePatterns enum ordinal             │
// │   position   : number   — stepper position in mm (float)          │
// │   sessionId  : UUID     — regenerated each time a play mode starts  │
// │                                                                    │
// │ Optional fields:                                                   │
// │   meta       : string   — JSON-encoded metadata (optional)         │
// │                                                                    │
// │ This is also sent over BLE via NimBLE notifications.               │
// │ See: test/test_mqtt_payload/ for contract tests.                   │
// └──────────────────────────────────────────────────────────────────────┘
String OSSM::getCurrentState() {
    String currentState;
    if (stateMachine != nullptr) {
        stateMachine->visit_current_states(
            [&currentState](auto state) { currentState = state.c_str(); });
    }

    float positionMm = float(stepper->getCurrentPosition()) / float(1_mm);
    if (isnan(positionMm)) positionMm = 0.0f;

    return "{\"timestamp\":" + String((unsigned long)millis()) +
           ",\"state\":\"" + currentState +
           "\",\"speed\":" + String((int)settings.speed) +
           ",\"stroke\":" + String((int)settings.stroke) +
           ",\"sensation\":" + String((int)settings.sensation) +
           ",\"depth\":" + String((int)settings.depth) +
           ",\"buffer\":" + String((int)settings.buffer) +
           ",\"pattern\":" + String(static_cast<int>(settings.pattern)) +
           ",\"position\":" + String(positionMm, 2) +
           ",\"sessionId\":\"" + sessionId + "\"}";
}
