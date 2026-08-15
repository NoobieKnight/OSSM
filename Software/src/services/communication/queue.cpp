#include "queue.h"

std::queue<String> messageQueue = {};

QueueHandle_t rawQueue = xQueueCreate(20, sizeof(PositionTime));
QueueHandle_t motionQueue = xQueueCreate(20, sizeof(MotionCommand));
