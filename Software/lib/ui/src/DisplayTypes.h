#ifndef UI_DISPLAY_TYPES_H
#define UI_DISPLAY_TYPES_H

#include <stdint.h>

namespace ui {

enum class PlayControl { STROKE, DEPTH, SENSATION, BUFFER };

struct PlayControlsData {
    float speed;
    float stroke;
    float sensation;
    float depth;
    float buffer;
    PlayControl activeControl;
    int strokeCount;
    float distanceMeters;
    unsigned long elapsedMs;
    int pattern;
    bool isStrokeEngine;
    bool isStreaming;
    const char* headerText;
    const char* speedLabel;
    const char* strokeLabel;
    const char* distanceStr;
    const char* timeStr;
};

struct MenuData {
    const char* const* items;
    int numItems;
    int selectedIndex;
};

enum class WifiStatus { DISCONNECTED, CONNECTING, CONNECTED, ERROR };
enum class BleStatus { DISCONNECTED, CONNECTING, CONNECTED, ADVERTISING, ERROR };

inline int scrollPercent(int index, int count) {
    return (count > 1) ? (100 * index / (count - 1)) : 0;
}

}  // namespace ui

#endif  // UI_DISPLAY_TYPES_H
