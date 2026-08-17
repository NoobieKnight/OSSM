#include "stepper.h"
#include <cstdint>

#include "constants/Config.h"
#include "ossm/state/settings.h"
#include "ossm/state/calibration.h"


FastAccelStepperEngine stepperEngine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;
class StrokeEngine Stroker;


// --- Safety & Hardware Limits ---
const int32_t MIN_POSITION_LIMIT = -10000;
const int32_t MAX_POSITION_LIMIT = 50000;
const uint32_t MAX_HARDWARE_SPEED = 20000;
const uint32_t MAX_HARDWARE_ACCEL = 60000;
const uint32_t LINEAR_ACCEL_STEPS = 150;

// --- Trajectory Data ---
struct Waypoint {
  int32_t targetPos; // Target position for the move
  uint32_t timeMs; // Time for the complete move. From last move to this target
  bool moveNow; // User override to skip buffer wait
};

QueueHandle_t waypointQueue;

// --- State Tracking & Debt Compensation ---
float lastMoveSpeed = 0.0;
float lastMovePos = 0.0;
float accumulatedTimeDebtMs = 0.0;

// Scale a % variable (int 0-100)
inline uint32_t scaleIntPercent(int32_t value, int16_t percent) {
    return static_cast<uint32_t>(value * (percent / 100.0f));
}

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

// --- Helper to Push to FreeRTOS Queue ---
bool addWaypoint(int32_t pos, uint32_t timeMs, bool moveNow = false) {
  if (timeMs == 0) return false;
  Waypoint wp = {pos, timeMs, moveNow};
  return (xQueueSend(waypointQueue, &wp, pdMS_TO_TICKS(10)) == pdTRUE);
}

// Calculate target positon from a percent of stroke and depth. And add position to waypointQueue
bool addStrokeDepthWaypoint(float posPercent, float stroke, float depth, uint32_t timeMs, bool moveNow = false) {
    if (timeMs == 0) return false;
    uint32_t maxPosition = scaleIntPercent( calibration.measuredStrokeSteps,
        settings.depth ); // Max position in steps
    uint32_t strokeLength = min(scaleIntPercent( calibration.measuredStrokeSteps,
        settings.stroke), maxPosition); // Stroke length in steps
    uint32_t minPosition = maxPosition - strokeLength; // Min position in steps
    int32_t targetPosition = minPosition + (strokeLength * posPercent); // Target position in steps


    Waypoint wp = {targetPosition, timeMs, moveNow};
    return (xQueueSend(waypointQueue, &wp, pdMS_TO_TICKS(10)) == pdTRUE);
}

// Check the direction of two upcoming moves
bool sameDir(int32_t startPos, int32_t targetPos,int32_t nextTargetPos) {
        // Evaluate vectors
        bool dir1 = (targetPos - startPos) >= 0;
        bool dir2 = (nextTargetPos - targetPos) >= 0;
        if (dir1 != dir2) {
            return false;
        } else {
            return true;
        }
}

// Calculate speed, acceleration and stop position and execute move
uint32_t calculateAndExecute(Waypoint* waypointQueue, int itemsInBuffert) {

    // Calculate adjustment time.
    // If a move had to be slower than requested then this will speed up movements after to compensate
    float adjustedTimeMs = (float)waypointQueue[0].timeMs - accumulatedTimeDebtMs;
    if (adjustedTimeMs < 10.0) {
        adjustedTimeMs = 10.0;
        accumulatedTimeDebtMs -= ((float)waypointQueue[0].timeMs - adjustedTimeMs);
    } else {
        accumulatedTimeDebtMs = 0.0;
    }

    // Limit position
    // ToDo move this to trasition from xQueue to array. It should also have a failsafe if two positions a
    if (waypointQueue[0].targetPos > MAX_POSITION_LIMIT || waypointQueue[0].targetPos < MIN_POSITION_LIMIT) {
        ESP_LOGI("Stepper", "Target position limited");
        constrain(waypointQueue[0].targetPos,MIN_POSITION_LIMIT,MAX_POSITION_LIMIT);
    }

    float timeSec = adjustedTimeMs / 1000.0;
    float displacment = (float)waypointQueue[0].targetPos - lastMovePos;

    // Planned stop logic
    // Change to a MoveTo if a stop at this position will be required
    bool plannedStop = false;
    if (itemsInBuffert == 1) {
        // This is the only position in the queue
        plannedStop = true;
    } else {
        // Check that all moves are in the same direction
        plannedStop = !sameDir(lastMovePos,waypointQueue[0].targetPos,waypointQueue[1].targetPos);
    }

    // Calculate acceleration and speed
    float accNominal = 0; // Nominal acceleration in steps/s²
    float speedTarget = 0; // Speed target in steps/s
    if (!plannedStop) {
        // Flyby position
        // Calculate acceleration and speed with no stop at next position
        accNominal = 2.0 * (displacment - (lastMoveSpeed * timeSec)) / (timeSec * timeSec);
        speedTarget = lastMoveSpeed + (accNominal * timeSec);
    } else {
        // Stop position
        // Calculate speed required for a move that ends with a stop
        speedTarget = 0.0;
        float requiredAvgSpeed = abs(displacment) / timeSec;
        float peakSpeedEstimate = max(abs(lastMoveSpeed), requiredAvgSpeed * 2.0f);

        accNominal = peakSpeedEstimate / (timeSec * 0.5f);
    }

    // Calculate compensation for LinearAcceleration
    float accAdjusted = accNominal;
    if (LINEAR_ACCEL_STEPS > 0 && abs(accNominal) > 1.0) {
        float timePenaltyRatio = 1.0 + ((float)LINEAR_ACCEL_STEPS / (abs(displacment) + 1.0));
        accAdjusted *= timePenaltyRatio;
    }

    // Select speed depending on plannedStop
    float profileSpeed = plannedStop ? (abs(displacment) * 2.0 / timeSec) : abs(speedTarget);

    // Limit speed
    // ToDo: This will not calculate the correct timeDebt. Instead, calculate debt based on the time required for the whole move including acceleration
    if (profileSpeed > MAX_HARDWARE_SPEED) {
        profileSpeed = MAX_HARDWARE_SPEED;
        float expectedTime = (abs(displacment) / (float)MAX_HARDWARE_SPEED) * 1000.0;
        accumulatedTimeDebtMs += (expectedTime - adjustedTimeMs);
    }

    // Limit acceleration
    if (abs(accAdjusted) > MAX_HARDWARE_ACCEL) {
        accAdjusted = (accAdjusted > 0) ? MAX_HARDWARE_ACCEL : -MAX_HARDWARE_ACCEL;
    }

    // Set acceleration and speed
    stepper->setAcceleration(accAdjusted);
    stepper->setSpeedInHz(profileSpeed);

    if (plannedStop) {
        // Handoff to library distance planner
        stepper->moveTo(waypointQueue[0].targetPos);
    } else {
        // Continuous velocity pass-through
        stepper->applySpeedAcceleration();
        if (speedTarget >= 0) stepper->runForward();
        else stepper->runBackward();
    }

    // Update states for the next loop
    lastMovePos = waypointQueue[0].targetPos;
    lastMoveSpeed = speedTarget;
    return (uint32_t)adjustedTimeMs;
}

// --- High-Speed Trajectory Task ---
void trajectoryTask(void *pvParameters) {
    Waypoint waypointArrayQueue[5];
    int numBuffered = 0;
    bool isMoving = stepper->isRunning();
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(true) {
        // Move xQueue items to array
        Waypoint wpTemp;
        while (numBuffered < 5 && xQueueReceive(waypointQueue, &wpTemp, 0) == pdTRUE) {
            waypointArrayQueue[numBuffered++] = wpTemp;
        }

        if (!isMoving) {
            // 2. IDLE STATE: Check the Start Gates
            bool startMove = waypointArrayQueue[0].moveNow;

            // ToDo: Add a timeout, the function should only wait for a queue for a set amount of time
            if (numBuffered >= 3) {
                // Enough moves in buffert to start now
                startMove = true;
            }
            else if (numBuffered >= 2 && !sameDir(stepper->getCurrentPosition(),
                                            waypointArrayQueue[0].targetPos,
                                            waypointArrayQueue[1].targetPos)) {
                // Change in direction required
                startMove = true;
            }

            // Start of movement requested
            if (startMove) {
                lastMovePos = stepper->getCurrentPosition();
                lastMoveSpeed = stepper->getCurrentSpeedInMilliHz() / 1000.0;
                accumulatedTimeDebtMs = 0.0;
                xLastWakeTime = xTaskGetTickCount();

                // Execute movement
                uint32_t effTime = calculateAndExecute(waypointArrayQueue, numBuffered);
                isMoving = true;
                // Shift buffer down
                for(int i=1; i<numBuffered; i++) waypointArrayQueue[i-1] = waypointArrayQueue[i];
                numBuffered--;

                vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(effTime));
            } else {
                // Waiting for more points to clear start gates
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        else {
            // 3. MOVING STATE: Previous segment time just expired, process handoff
            if (numBuffered > 0) {

                // Execute movement
                uint32_t effTime = calculateAndExecute(waypointArrayQueue, numBuffered);
                // Shift buffer down
                for(int i=1; i<numBuffered; i++) waypointArrayQueue[i-1] = waypointArrayQueue[i];
                numBuffered--;

                vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(effTime));
            } else {
                stepper->stopMove();
                isMoving = false;
                numBuffered = 0;
                xQueueReset(waypointQueue);
                }
            } else {
                // Queue empty. Because items_in_buffer was 1 on the previous loop,
                // calculateAndExecute automatically used moveTo().
                // Therefore, the stepper is naturally arriving perfectly at its target.
                isMoving = false;
                Serial.println("Queue empty. Natural arrival at target via moveTo().");
            }
        }
    }
}

void setup() {
  waypointQueue = xQueueCreate(20, sizeof(Waypoint));


  xTaskCreatePinnedToCore(trajectoryTask, "TrajTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}

