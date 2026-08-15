#ifndef SOFTWARE_STEPPER_H
#define SOFTWARE_STEPPER_H

#include <cstdint>
#include "FastAccelStepper.h"
#include "StrokeEngine.h"
#include "constants/Pins.h"

struct RampState {
    bool idle;          // Stepper is not moving
    bool accelerating;  // Stepper is accelerating
    bool decelerating;  // Stepper is decelerating
    bool dir_up;        // Direction positive
};


extern FastAccelStepperEngine stepperEngine;
extern FastAccelStepper *stepper;
extern class StrokeEngine Stroker;

void initStepper();

RampState getRampState(FastAccelStepper *inStepper);

#endif  // SOFTWARE_STEPPER_H
