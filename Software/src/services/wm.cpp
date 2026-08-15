#include "wm.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <sys/_timeval.h>
#include <sys/select.h>
#include <cstdint>
#include "WiFi.h"
#include "WiFiType.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_sntp.h"

#include "freertos/projdefs.h"
#include "lwip/apps/sntp.h"
#include "services/communication/nimble.h"
#include "constants/Credentials.h"


Preferences wifiPrefs;

// Check if any credentials have been saved
bool hasSavedCredentials() {
    wifi_config_t conf;
    // Fetch the current station configuration from NVS/flash
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
        // Check if the stored SSID is not empty
        if (strlen((char*)conf.sta.ssid) > 0) {
            return true;
        }
    }
    return false;
}

wl_status_t waitForConnection(int maxLoops = 15){
    // Wait for connection or timeout
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_NO_SSID_AVAIL && i < maxLoops) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        i++;
        ESP_LOGD("WM", "Connecting... waiting %d/15", i);
    }

    return WiFi.status();
}

// Initilize wifi
void initWM() {
    WiFi.useStaticBuffers(true);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    if (!hasSavedCredentials() && strcmp(Credentials::WiFi::ssid, "wifi_ssid") != 0) {
        WiFi.begin(Credentials::WiFi::ssid, Credentials::WiFi::password);
    } else {
        WiFi.begin();
    }

    ESP_LOGI("WM", "WiFi initialized");

    // Wait for WiFi to connect
    wl_status_t wifiState = waitForConnection();

    // Configure NTP
    sntp_servermode_dhcp(1);
    sntp_set_sync_interval(300000);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    configTime(0, 0, "pool.ntp.org");

    if (wifiState == WL_CONNECTED) {
        // Wait for NTP if connection success
        struct timeval tv;
        int i = 0;
        while (tv.tv_sec < 1700000000 && i < 15) { // Ensures time is past year 2023
            vTaskDelay(pdMS_TO_TICKS(1000));
            gettimeofday(&tv, NULL);
        }

        if (i < 15) {
            ESP_LOGI("WM", "Time synced");
        }
    }

}

bool setWiFiCredentials(const String& ssid, const String& password) {
    // Open preferences in read/write mode
    if (!wifiPrefs.begin("wifi", false)) {
        ESP_LOGE("WM", "Failed to open preferences for writing");
        return false;
    }

    // Save credentials to NVS
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("password", password);
    wifiPrefs.end();

    ESP_LOGI("WM", "WiFi credentials saved to NVS");
    return true;
}

bool connectWiFi() {
    int i = 0;

    // Check saved credentials
    if (!hasSavedCredentials()) {
        ESP_LOGW("WM", "No SSID found in NVS");
        return false;
    }

    ESP_LOGI("WM", "Attempting to connect to saved WiFi");

    // Disconnect if already connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();

        // Loop untill disconnected or timeout
        i = 0;
        while (WiFi.status() != WL_DISCONNECTED && i < 10) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            i ++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            ESP_LOGD("WM", "Could not disconnect from existing connection");
            return false;
        }
    }

    // Begin connection with saved credentials
    WiFi.begin();

    // Wait for WiFi to connect
    wl_status_t wifiState = waitForConnection();

    if (wifiState == WL_CONNECTED) {
        ESP_LOGI("WM", "Connected to WiFi. IP: %s", WiFi.localIP().toString().c_str());
        return true;
    } else if ( wifiState == WL_NO_SSID_AVAIL ) {
        ESP_LOGW("WM", "SSID not found, check spelling and try again");
        return false;
    } else {
        ESP_LOGW("WM", "Failed to connect to WiFi. Status: %d", wifiState);
        return false;
    }
}

String getWiFiStatus() {
    JsonDocument doc;

    bool connected = (WiFi.status() == WL_CONNECTED);
    doc["connected"] = connected;

    if (connected) {
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
    } else {
        // Try to read saved SSID from NVS
        if (wifiPrefs.begin("wifi", true)) {
            String savedSSID = wifiPrefs.getString("ssid", "");
            if (savedSSID.length() > 0) {
                doc["ssid"] = savedSSID;
            }
            wifiPrefs.end();
        }
        doc["ip"] = "";
        doc["rssi"] = 0;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

BleState getBleState() {
    if (pServer == nullptr) {
        return BleState::DISCONNECTED;
    }

    if (pServer->getAdvertising() && pServer->getConnectedCount() == 0) {
        return BleState::ADVERTISING;
    }

    if (pServer->getConnectedCount() > 0) {
        return BleState::CONNECTED;
    }

    if (pServer->getAdvertising()) {
        return BleState::CONNECTING;
    }

    return BleState::DISCONNECTED;
}

