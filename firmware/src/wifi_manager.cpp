/**
 * @file wifi_manager.cpp
 * @brief Wi-Fi Station and AP fallback implementation with graceful reconnection logic.
 */

#include "wifi_manager.h"
#include "settings.h"
#include "discovery.h"
#include <esp_log.h>
#include <esp_wifi.h>

static const char* TAG = "[WIFI]";

WifiManager g_wifi_manager;

WifiManager::WifiManager()
    : state_(WIFI_STATE_DISCONNECTED),
      lastReconnectAttemptMs_(0),
      reconnectAttempts_(0) {}

bool WifiManager::init() {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    delay(100);

    String hostname = g_settings.getHostname();
    WiFi.setHostname(hostname.c_str());

    if (g_settings.hasWifiCredentials()) {
        String ssid = g_settings.getWifiSsid();
        String pass = g_settings.getWifiPassword();
        ESP_LOGI(TAG, "Found stored Wi-Fi credentials for SSID: %s. Connecting...", ssid.c_str());
        return connectStation(ssid, pass);
    } else {
        ESP_LOGW(TAG, "No stored Wi-Fi credentials found. Starting SoftAP Provisioning Mode.");
        startApMode();
        return true;
    }
}

bool WifiManager::connectStation(const String& ssid, const String& password) {
    configuredSsid_ = ssid;
    state_ = WIFI_STATE_CONNECTING;
    reconnectAttempts_ = 0;

    // Keep the provisioning AP available while the STA connects. This makes
    // first-time/recovery setup reachable even when stale credentials exist.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_FALLBACK_IP, AP_FALLBACK_GATEWAY, AP_FALLBACK_SUBNET);
    WiFi.softAP(AP_FALLBACK_SSID, AP_FALLBACK_PASSWORD);
    WiFi.begin(ssid.c_str(), password.c_str());
    ESP_LOGI(TAG, "Provisioning AP started: SSID '%s', IP %s",
             AP_FALLBACK_SSID, WiFi.softAPIP().toString().c_str());
    ESP_LOGI(TAG, "Initiated Wi-Fi STA connection to '%s'", ssid.c_str());
    return true;
}

void WifiManager::startApMode() {
    state_ = WIFI_STATE_AP_MODE;
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_FALLBACK_IP, AP_FALLBACK_GATEWAY, AP_FALLBACK_SUBNET);
    WiFi.softAP(AP_FALLBACK_SSID, AP_FALLBACK_PASSWORD);

    ESP_LOGI(TAG, "SoftAP started: SSID '%s', IP %s", AP_FALLBACK_SSID, AP_FALLBACK_IP.toString().c_str());
    g_discovery.begin();
}

void WifiManager::update() {
    uint32_t now = millis();

    if (state_ == WIFI_STATE_AP_MODE) {
        // In AP mode, waiting for user to configure credentials via Web UI
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (state_ != WIFI_STATE_CONNECTED) {
            state_ = WIFI_STATE_CONNECTED;
            reconnectAttempts_ = 0;
            ESP_LOGI(TAG, "Wi-Fi Connected! IP: %s, RSSI: %d dBm, Hostname: %s",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.getHostname());
            g_discovery.begin();
        }
    } else {
        if (state_ == WIFI_STATE_CONNECTED) {
            ESP_LOGW(TAG, "Wi-Fi connection lost. Attempting reconnection...");
            state_ = WIFI_STATE_DISCONNECTED;
            lastReconnectAttemptMs_ = now;
        } else if (state_ == WIFI_STATE_CONNECTING || state_ == WIFI_STATE_DISCONNECTED) {
            // Attempt reconnect every 5 seconds
            if (now - lastReconnectAttemptMs_ > 5000) {
                lastReconnectAttemptMs_ = now;
                reconnectAttempts_++;
                ESP_LOGI(TAG, "Reconnecting to Wi-Fi '%s' (Attempt %d)...", 
                         configuredSsid_.c_str(), reconnectAttempts_);
                WiFi.reconnect();

                // If unable to reconnect for > 15 attempts (~75s), fallback to AP mode for recovery
                if (reconnectAttempts_ >= 15) {
                    ESP_LOGW(TAG, "Failed to connect to Wi-Fi after 15 attempts. Starting fallback AP mode.");
                    startApMode();
                }
            }
        }
    }
}

String WifiManager::getSsid() const {
    if (state_ == WIFI_STATE_AP_MODE) {
        return String(AP_FALLBACK_SSID);
    }
    return WiFi.SSID();
}

int8_t WifiManager::getRssi() const {
    if (state_ != WIFI_STATE_CONNECTED) return 0;
    return WiFi.RSSI();
}

IPAddress WifiManager::getIpAddress() const {
    if (state_ == WIFI_STATE_AP_MODE) {
        return WiFi.softAPIP();
    }
    return WiFi.localIP();
}

String WifiManager::getMacAddress() const {
    return WiFi.macAddress();
}

String WifiManager::getHostname() const {
    return String(WiFi.getHostname());
}
