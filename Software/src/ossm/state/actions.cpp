#include "actions.h"

#include "ossm/homing/homing.h"
#include "ossm/streaming/streaming.h"
#include "ossm/state/calibration.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "ossm/stroke_engine/stroke_engine.h"
#include "services/communication/mqtt.h"
#include "services/stepper.h"
#include "services/wm.h"
#include "utils/random.h"


void ossmStartHoming() {
    homing::clearHoming();
    homing::startHoming();
}

void ossmStartStreaming() {
    streaming::startStreaming();
}

void ossmResetSettingsStrokeEngine() {
    sessionId = uuid();

    settings.speed = 0;
    settings.speedBLE = std::nullopt;
    settings.stroke = 50;
    settings.depth = 10;
    settings.sensation = 50;
    session.playControl = PlayControls::DEPTH;
}

void ossmResetSettingsStreaming() {
    sessionId = uuid();

    settings.speed = 0;
    settings.speedBLE = std::nullopt;
    settings.stroke = 50;
    settings.depth = 50;
    settings.sensation = 50;
    settings.buffer = 100;
    session.playControl = PlayControls::DEPTH;
}

void ossmStartStrokeEngine() {
    stroke_engine::startStrokeEngine();
}

void ossmEmergencyStop() {
    stepper->forceStop();
    stepper->disableOutputs();
}

void ossmCheckPairing() {
}

void ossmSetHomed() {
    calibration.isHomed = true;
    calibration.justHomed = true;
}

void ossmSetNotHomed() {
    calibration.isHomed = false;
}

void ossmRestart() {
    ESP.restart();
}
