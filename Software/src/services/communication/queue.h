#ifndef OSSM_COMMUNICATION_QUEUE_H
#define OSSM_COMMUNICATION_QUEUE_H

#include <Arduino.h>
#include <chrono>
#include <cstdint>
#include <queue>

struct PositionTime {
    uint8_t position; // Setpoint position in % 0-100
    uint16_t inTime;     // When should the position be reached (ms)
    std::chrono::steady_clock::time_point setTime; // When the command was received by the OSSM (ms)
};

struct MotionCommand {
    uint32_t position; // Setpoint position in % 0-100
    uint16_t duration;     // When should the position be reached (s)
    std::chrono::steady_clock::time_point finishMove; // When the command should be finished (with compensation)
    uint16_t age_ms; // Age of the package when sent to motion queue
};

extern std::queue<String> messageQueue;

extern QueueHandle_t rawQueue;
extern QueueHandle_t motionQueue;

#endif  // OSSM_COMMUNICATION_QUEUE_H
