#include "streaming.h"

#include <cstdint>
#include <queue>
#include <chrono>
#include "Arduino.h"
#include "freertos/projdefs.h"
#include "streaming_logic.h"
#include "constants/Config.h"
#include "ossm/state/calibration.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/communication/nimble.h"
#include "services/communication/queue.h"
#include "services/stepper.h"
#include "services/tasks.h"

const int minTimeForMovement = 5; // Minimum time to allow start of movment

namespace sml = boost::sml;
using namespace sml;

namespace streaming {

static void startQueueHandlingTask(void *pvParameters) {
    // This funtion handles the queue of streaming commands

    // Is the state machine in the correct state?
    auto isInCorrectState = []() {
        return stateMachine->is("streaming"_s) ||
               stateMachine->is("streaming.active"_s);
    };

    PositionTime cmd;
    PositionTime lastCmd = {0, 0, std::chrono::steady_clock::now()};
    MotionCommand motionCmd;
    MotionCommand lastMotionCmd = {0, 0, std::chrono::steady_clock::now(),0};

    // Reset the queue to clear any existing commands
    xQueueReset(rawQueue);

    while (isInCorrectState()) {
        // Wait until new command in queue
        if (xQueueReceive(rawQueue, &cmd, pdMS_TO_TICKS(100)) == pdPASS) {
            // Get current clock
            auto now = std::chrono::steady_clock::now(); // Current clock
            int32_t age_ms = 0; // Age of the command

            if (USE_LATENCY_COMPENSATION) {
                // Calculate time diffrence between when command was received to now
                age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - cmd.setTime).count();

                // Sanitycheck
                if (age_ms < 0 || age_ms > 1000) {
                    age_ms = 0;
                }

            } else {
                age_ms = 0;
            }

            // Make sure last commands finishMove is in the future
            if (lastMotionCmd.finishMove < now) {
                lastMotionCmd.finishMove = now - std::chrono::milliseconds(age_ms);
                lastMotionCmd.age_ms = 0;
            }

            // Push the motion command
            motionCmd.position = cmd.position;
            motionCmd.duration = cmd.inTime;
            motionCmd.finishMove = lastMotionCmd.finishMove + std::chrono::milliseconds( motionCmd.duration );
            motionCmd.age_ms = age_ms;

            // Save command to next loop
            lastCmd = cmd;
            lastMotionCmd = motionCmd;

            xQueueSend(motionQueue, &motionCmd, portMAX_DELAY);
        }
    }

    vTaskDelete(nullptr);
}

static void startStreamingTask(void *pvParameters) {
    // Own the shared stepper config at mode entry (see simple_penetration.cpp:
    stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
    stepper->enableOutputs();

    // Is the state machine in the correct state?
    auto isInCorrectState = []() {
        return stateMachine->is("streaming"_s) ||
               stateMachine->is("streaming.active"_s);
    };

    auto now = std::chrono::steady_clock::now();
    MotionCommand cmd;
    MotionCommand lastCmd = {0, 0, std::chrono::steady_clock::now(),0};

    // Reset the queue to clear any existing commands
    xQueueReset(motionQueue);

    // Motion state
    int32_t targetPosition = 0; // Target position in steps
    double distance = 0.0f; // Target position in steps
    double movementTime = 0.0f; // How long sould the movement take (s)

    uint32_t maxConfigSpeed = Config::Driver::maxSpeedMmPerSecond * (1_mm);
    uint32_t maxConfigAccel = Config::Driver::maxAcceleration * (1_mm);

    // Set initial max speed and acceleration
    stepper->setSpeedInHz(maxConfigSpeed);
    stepper->setAcceleration(maxConfigAccel);

    while (isInCorrectState()) {
        // Wait until new command in queue
        if (xQueueReceive(motionQueue, &cmd, pdMS_TO_TICKS(100)) == pdPASS) {

            // Make sure timing on the first move is correct
            if (!stepper->isRunning() || lastCmd.finishMove < now) {
                lastCmd.finishMove = cmd.finishMove - std::chrono::milliseconds(cmd.duration);
            }

            // Read settings
            uint32_t maxPosition = streaming_logic::scaleIntPercent( calibration.measuredStrokeSteps,
                settings.depth ); // Max position in steps
            uint32_t strokeLength = min(streaming_logic::scaleIntPercent( calibration.measuredStrokeSteps,
                settings.stroke), maxPosition); // Stroke length in steps
            uint32_t minPosition = maxPosition - strokeLength; // Min position in steps
            uint32_t speedLimit = streaming_logic::scaleIntPercent( maxConfigSpeed, settings.speed ); // Max speed (steps/s)
            double accelSetpoint = streaming_logic::scaleIntPercent( maxConfigAccel, settings.sensation ); // Setpoint acceleration (steps/s^2) Allowed to go higher

            // Skip movement if speed is 0
            if (speedLimit <= 0) {
                ESP_LOGI("Streaming", "Speed set to 0, skipping moves");
                continue;
            }

            // Calculate the target position
            targetPosition = minPosition + streaming_logic::scaleIntPercent( strokeLength, cmd.position);

            // Calculate distance to travel (in steps)
            distance = streaming_logic::scaleIntPercent(strokeLength,abs(int32_t(cmd.position) - int32_t(lastCmd.position)));

            // Are we too far behind?
            now = std::chrono::steady_clock::now();
            auto executeTime = max(now, lastCmd.finishMove);
            auto calcTime = std::chrono::duration_cast<std::chrono::milliseconds>(cmd.finishMove - executeTime).count();
            if (calcTime < minTimeForMovement){
                ESP_LOGI("Streaming", "Behind in queue or time to complete move is too small");
                continue;
            } else if (distance < 5){
                ESP_LOGI("Streaming", "Position too close to last position. Move will not execute");
                continue;
            }

            // How long sould the movement take (seconds)
            movementTime = min(cmd.duration, uint16_t(calcTime)) / 1000.0f;

            uint32_t requiredSpeed = 0; // Desired speed (steps/s)
            uint32_t targetSpeed = 0;  // Target speed (steps/s, limited by user settings)
            uint32_t requiredAcc = 0; // Desired acceleration (steps/s^2)
            uint32_t targetAcc = 0; // Target acceleration (steps/s^2, limited by user settings)

            // Is the movement possible with the set acceleration?
            double discriminant = (accelSetpoint * movementTime) * (accelSetpoint * movementTime) - (4.0 * accelSetpoint * distance);
            if (discriminant < 0.0) {

                // Calculate desired speed and acceleration for the move, not using set acceleration
                requiredSpeed = (1.5f * distance) / movementTime; // Desired speed (steps/s)
                // Limit speed to the maximum allowed by the configuration and user settings
                targetSpeed = min(requiredSpeed, speedLimit); // Target speed (steps/s, limited by user settings)

                // Calculate desired speed and acceleration for the move
                requiredAcc = (3.0f * targetSpeed * targetSpeed) / distance; // Desired acceleration (steps/s^2)
                // Limit acceleration to the maximum allowed by the configuration and user settings
                targetAcc = min(requiredAcc, maxConfigAccel); // Target acceleration (steps/s^2, limited by user settings)
            } else {
                // Calculate desired speed and acceleration for the move, with user set acceleration
                targetSpeed = ((2.0 * accelSetpoint * distance) / ((accelSetpoint * movementTime) + std::sqrt(discriminant)));
                targetAcc = min(uint32_t(accelSetpoint), maxConfigAccel); // Target acceleration (steps/s^2, limited by user settings)

                requiredSpeed = targetSpeed;
                requiredAcc = targetAcc;
            }

            // Have speed and acceleration been limited by the user settings?
            if (targetSpeed != requiredSpeed && targetAcc != requiredAcc){
                ESP_LOGI("Streaming","Target speed and acceleration limited by user settings. "
                    "Speed: %d/%d, Acceleration: %d/%d", requiredSpeed, targetSpeed, requiredAcc, targetAcc);
                continue;
            }

            // Wait until the precise moment this movement should be executed
            bool nextMoveSameDir = getRampState(stepper).dir_up == (cmd.position < lastCmd.position);
            while (isInCorrectState() && stepper->isRunning() && lastCmd.finishMove > now &&
            (nextMoveSameDir && !(getRampState(stepper).decelerating && abs(stepper->getCurrentSpeedInUs(false)) < targetSpeed))){
                now = std::chrono::steady_clock::now();
                vTaskDelay(1);
            }

            // Move to target position
            stepper->setAcceleration(targetAcc);
            stepper->setSpeedInHz(targetSpeed);
            stepper->moveTo(targetPosition, false);

            ESP_LOGI("Streaming", "P(%d -> %d ): SameDir: %i - %d | %d, T: %.3f, S: %d, A: %d, QBLE: %d | QM: %d",
                    lastCmd.position, cmd.position, nextMoveSameDir, targetPosition, distance, movementTime,
                    targetSpeed, targetAcc, uxQueueMessagesWaiting(rawQueue), uxQueueMessagesWaiting(motionQueue));
            // Save current position as last position
            lastCmd = cmd;

        }
    }

    vTaskDelete(nullptr);
}

void startStreaming() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask", stackSize,
                            nullptr, configMAX_PRIORITIES - 1, nullptr,
                            Tasks::operationTaskCore);

    xTaskCreatePinnedToCore(startQueueHandlingTask, "startQueueHandlingTask", stackSize,
                            nullptr, configMAX_PRIORITIES - 2, nullptr,
                            Tasks::operationTaskCore);
}

}  // namespace simple_penetration
