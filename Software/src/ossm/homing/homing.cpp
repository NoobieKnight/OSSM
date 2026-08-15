#include "homing.h"

#include "Strings.h"
#include "homing_logic.h"
#include "constants/Config.h"
#include "constants/Pins.h"
#include "constants/UserConfig.h"
#include "ossm/Events.h"
#include "ossm/state/calibration.h"
#include "ossm/state/error.h"
#include "ossm/state/state.h"
#include "services/led.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "utils/analog.h"

namespace sml = boost::sml;
using namespace sml;

namespace homing {

void clearHoming() {
    ESP_LOGD("Homing", "Homing started");

    // Set homing active flag for LED indication
    setHomingActive(true);

    calibration.isForward = true;

    // Set acceleration and deceleration in steps/s^2
    stepper->setAcceleration(1000_mm);
    // Set speed in steps/s
    stepper->setSpeedInHz(Config::Driver::homingSpeed);

    // Clear the stored values.
    calibration.measuredStrokeSteps = 0;

    // Recalibrate the current sensor offset.
    calibration.currentSensorOffset = (getAnalogAveragePercent(
        SampleOnPin{Pins::Driver::currentSensorPin, 1000}));
}

static void startHomingTask(void *pvParameters) {
    TickType_t xTaskStartTime = xTaskGetTickCount();

    // Determine the direction of homing based on the state machine.
    bool invertMotorDirection = !stateMachine->is("homing.backward"_s);

    // Stroke Engine and Simple Penetration treat this differently.
    stepper->enableOutputs();
    stepper->setDirectionPin(Pins::Driver::motorDirectionPin, invertMotorDirection);

    // Set target position as 50mm+ max stroke length to ensure hitting the endstop.
    int32_t targetPositionInSteps =
        round(-(50_mm + Config::Driver::maxStrokeSteps));

    ESP_LOGD("Homing", "Target position in steps: %d", targetPositionInSteps);
    stepper->moveTo(targetPositionInSteps, false);

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("homing"_s) ||
               stateMachine->is("homing.forward"_s) ||
               stateMachine->is("homing.backward"_s);
    };

    // run loop for 15second or until loop exits
    while (isInCorrectState()) {
        TickType_t xCurrentTickCount = xTaskGetTickCount();
        // Calculate the time in ticks that the task has been running.
        TickType_t xTicksPassed = xCurrentTickCount - xTaskStartTime;

        // Calculate the number of milliseconds that have passed since the task started.
        // 'portTICK_PERIOD_MS' is the number of milliseconds per tick.
        uint32_t msPassed = xTicksPassed * portTICK_PERIOD_MS;
        // Calculate the timeout for homing based on the maximum stroke steps + 5s
        uint32_t msTimeoutHoming = (Config::Driver::maxStrokeSteps / Config::Driver::homingSpeed + 5) * 1000;

        if (homing_logic::isHomingTimedOut(msPassed, msTimeoutHoming)) {
            ESP_LOGE("Homing", "Homing took too long. Check power and restart");
            errorState.message = ui::strings::homingTookTooLong;

            // Clear homing active flag for LED indication
            setHomingActive(false);

            stateMachine->process_event(Error{});
            break;
        }

        // measure the current analog value.
        float current = getAnalogAveragePercent(
                            SampleOnPin{Pins::Driver::currentSensorPin, Config::Driver::sensorlessCurrentSamples}) -
                        calibration.currentSensorOffset;

        ESP_LOGV("Homing", "Current: %f", current);
        bool isCurrentOverLimit = homing_logic::isCurrentOverLimit(
            current, 0, Config::Driver::sensorlessCurrentLimit);

        if (!isCurrentOverLimit) {
            vTaskDelay(5);  // Increased from 1ms to 10ms to reduce CPU load
            continue;
        }

        ESP_LOGD("Homing", "Current over limit: %f", current);
        stepper->stopMove();

        stepper->setSpeedInHz(100_mm);
        // step away from the hard stop, with your hands in the air!
        int32_t currentPosition = stepper->getCurrentPosition();
        stepper->moveTo(currentPosition + Config::Driver::homingOffsetMn,
                        true);

        // measure and save the current position
        calibration.measuredStrokeSteps = min(abs(stepper->getCurrentPosition()),
        int(Config::Driver::maxStrokeSteps));

        ESP_LOGD("Homing", "Measured stroke in steps: %f", calibration.measuredStrokeSteps);
        ESP_LOGD("Homing", "Measured stroke in mm: %f",
            (calibration.measuredStrokeSteps + Config::Driver::homingOffsetMn * 2) / Config::Driver::stepsPerMM);

        // Set current position
        stepper->setCurrentPosition(0);
        stepper->forceStopAndNewPosition(0);

        // Go to position after homing if last homing was done and stroke is not too short.
        if (stateMachine->is("homing.backward"_s) && !isStrokeTooShort()) {
            stepper->moveTo(UserConfig::afterHomingPosition,true);
        } else if(stateMachine->is("homing.forward"_s)) {
            stepper->moveTo(calibration.measuredStrokeSteps,true);
        }

        // Clear homing active flag for LED indication
        setHomingActive(false);

        stateMachine->process_event(Done{});
        break;
    };

    // Restore stepper direction
    stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);

    vTaskDelete(nullptr);
}

void startHoming() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;
    xTaskCreatePinnedToCore(startHomingTask, "startHomingTask", stackSize,
                            nullptr, configMAX_PRIORITIES - 1,
                            &Tasks::runHomingTaskH, Tasks::operationTaskCore);
}

bool isStrokeTooShort() {
    if (calibration.measuredStrokeSteps > Config::Driver::minStrokeLengthMm) {
        return false;
    }
    errorState.message = ui::strings::strokeTooShort;
    return true;
}

}  // namespace homing
