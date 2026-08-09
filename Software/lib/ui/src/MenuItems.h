#ifndef UI_MENU_ITEMS_H
#define UI_MENU_ITEMS_H

#include "Strings.h"

enum Menu {
    StrokeEngine,
    Streaming,
    Pairing,
    WiFiSetup,
    Help,
    Restart,
    NUM_OPTIONS
};

static const char* menuStrings[Menu::NUM_OPTIONS] = {
    ui::strings::strokeEngine,
    ui::strings::streaming,         ui::strings::pairing,
    ui::strings::wifiSetup,
    ui::strings::helpTitle,         ui::strings::restart,
};

#endif  // UI_MENU_ITEMS_H
