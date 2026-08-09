#include "HeaderBar.h"

#include <WiFi.h>
#include <esp_log.h>
#include <services/board.h>

#include "constants/LogTags.h"
#include "services/communication/nimble.h"
#include "services/led.h"
#include "services/wm.h"

TaskHandle_t headerBarTaskHandle = nullptr;

WifiStatus getWifiStatus() {
    wl_status_t wifiStatus = WiFiClass::status();

    switch (wifiStatus) {
        case WL_CONNECTED:
            return WifiStatus::CONNECTED;
        case WL_IDLE_STATUS:
            return WifiStatus::CONNECTING;
        case WL_NO_SSID_AVAIL:
        case WL_CONNECT_FAILED:
        case WL_DISCONNECTED:
        default:
            return WifiStatus::DISCONNECTED;
    }
}

BleStatus getBleStatus() {
    if (pServer == nullptr) {
        return BleStatus::DISCONNECTED;
    }

    if (pServer->getAdvertising() && pServer->getConnectedCount() == 0) {
        return BleStatus::ADVERTISING;
    }

    if (pServer->getConnectedCount() > 0) {
        return BleStatus::CONNECTED;
    }

    if (pServer->getAdvertising()) {
        return BleStatus::CONNECTING;
    }

    return BleStatus::DISCONNECTED;
}

[[noreturn]] void headerBarTask(void* pvParameters) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(HEADERBAR_TAG, "Header bar task started");


    while (true) {

        updateLEDForMachineStatus();

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void initHeaderBar() {
    ESP_LOGI(HEADERBAR_TAG, "Initializing header bar task");

    BaseType_t result =
        xTaskCreatePinnedToCore(headerBarTask, "headerBar",
                                4 * configMINIMAL_STACK_SIZE,
                                nullptr,
                                tskIDLE_PRIORITY + 1,
                                &headerBarTaskHandle,
                                0);

    if (result != pdPASS) {
        ESP_LOGE(HEADERBAR_TAG, "Failed to create header bar task");
    } else {
        ESP_LOGI(HEADERBAR_TAG, "Header bar task created successfully");
    }
}
