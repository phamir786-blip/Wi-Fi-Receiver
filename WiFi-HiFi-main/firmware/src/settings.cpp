/**
 * @file settings.cpp
 * @brief NVS Preferences implementation for WiFi-HiFi settings.
 */

#include "settings.h"
#include <esp_log.h>
#include <nvs_flash.h>

static const char* TAG = "[SETTINGS]";

Settings g_settings;

Settings::Settings() : initialized_(false) {}

bool Settings::begin() {
    if (initialized_) return true;

    // Ensure NVS flash is initialized and handle unformatted/corrupted flash gracefully
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated/corrupt. Erasing and re-initializing...");
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (!prefs_.begin("wifihifi", false)) {
        ESP_LOGE(TAG, "Failed to initialize NVS wifihifi namespace");
        return false;
    }
    initialized_ = true;
    ESP_LOGI(TAG, "NVS Settings initialized successfully");
    return true;
}

bool Settings::hasWifiCredentials() {
    if (!initialized_) begin();
    String ssid = prefs_.getString("wifi_ssid", "");
    return (ssid.length() > 0);
}

String Settings::getWifiSsid() {
    if (!initialized_) begin();
    return prefs_.getString("wifi_ssid", "");
}

String Settings::getWifiPassword() {
    if (!initialized_) begin();
    return prefs_.getString("wifi_pass", "");
}

bool Settings::setWifiCredentials(const String& ssid, const String& password) {
    if (!initialized_) begin();
    if (ssid.length() == 0) return false;
    prefs_.putString("wifi_ssid", ssid);
    prefs_.putString("wifi_pass", password);
    ESP_LOGI(TAG, "Saved new Wi-Fi credentials for SSID: %s", ssid.c_str());
    return true;
}

void Settings::clearWifiCredentials() {
    if (!initialized_) begin();
    prefs_.remove("wifi_ssid");
    prefs_.remove("wifi_pass");
    ESP_LOGI(TAG, "Cleared Wi-Fi credentials");
}

String Settings::getDeviceName() {
    if (!initialized_) begin();
    return prefs_.getString("dev_name", WIFI_HIFI_DEVICE_NAME);
}

bool Settings::setDeviceName(const String& name) {
    if (!initialized_) begin();
    if (name.length() == 0) return false;
    prefs_.putString("dev_name", name);
    return true;
}

String Settings::getHostname() {
    if (!initialized_) begin();
    return prefs_.getString("hostname", WIFI_HIFI_HOSTNAME);
}

bool Settings::setHostname(const String& hostname) {
    if (!initialized_) begin();
    if (hostname.length() == 0) return false;
    prefs_.putString("hostname", hostname);
    return true;
}

uint8_t Settings::getVolume() {
    if (!initialized_) begin();
    return prefs_.getUChar("volume", 85); // Default 85%
}

bool Settings::setVolume(uint8_t volume) {
    if (!initialized_) begin();
    if (volume > 100) volume = 100;
    prefs_.putUChar("volume", volume);
    return true;
}

bool Settings::getMute() {
    if (!initialized_) begin();
    return prefs_.getBool("mute", false);
}

bool Settings::setMute(bool mute) {
    if (!initialized_) begin();
    prefs_.putBool("mute", mute);
    return true;
}

WfhfLatencyMode Settings::getLatencyMode() {
    if (!initialized_) begin();
    uint8_t mode = prefs_.getUChar("latency", (uint8_t)WFHF_LATENCY_BALANCED);
    if (mode > (uint8_t)WFHF_LATENCY_STABLE) {
        mode = (uint8_t)WFHF_LATENCY_BALANCED;
    }
    return (WfhfLatencyMode)mode;
}

bool Settings::setLatencyMode(WfhfLatencyMode mode) {
    if (!initialized_) begin();
    prefs_.putUChar("latency", (uint8_t)mode);
    return true;
}

void Settings::factoryReset() {
    if (!initialized_) begin();
    prefs_.clear();
    ESP_LOGW(TAG, "Factory reset performed. All NVS settings cleared.");
}
