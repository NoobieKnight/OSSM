#include "stepper.h"

#include "constants/Config.h"

FastAccelStepperEngine stepperEngine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;
class StrokeEngine Stroker;


void initStepper() {
    stepperEngine.init();
    stepper = stepperEngine.stepperConnectToPin(Pins::Driver::motorStepPin);
    if (stepper) {
        stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
        stepper->setEnablePin(Pins::Driver::motorEnablePin, true);
        stepper->setAutoEnable(false);
    }

    // disable motor briefly in case we are against a hard stop.
    digitalWrite(Pins::Driver::motorEnablePin, HIGH);
    delay(600);
    digitalWrite(Pins::Driver::motorEnablePin, LOW);
    delay(100);
}

// Get the current movment state of a stepper
RampState getRampState(FastAccelStepper *inStepper) {
    RampState stepperState;
    uint8_t state = inStepper->rampState();

    // Extract fields
    uint8_t ramp = state & RAMP_STATE_MASK;
    uint8_t direction = state & RAMP_DIRECTION_MASK;

    // Ramp states
    stepperState.idle = (ramp == RAMP_STATE_IDLE);
    stepperState.accelerating = (ramp & RAMP_STATE_ACCELERATING_FLAG);
    stepperState.decelerating = (ramp & RAMP_STATE_DECELERATING_FLAG);

    // Direction
    stepperState.dir_up = (direction == RAMP_DIRECTION_COUNT_UP);

    return stepperState;

}


