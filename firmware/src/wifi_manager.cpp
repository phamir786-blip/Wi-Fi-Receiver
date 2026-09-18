/**
 * @file wifi_manager.cpp
 * @brief Wi-Fi Station and AP fallback implementation with explicit AP verification.
 */

#include "wifi_manager.h"
#include "settings.h"
#include <esp_log.h>
#include <esp_wifi.h>

static const char* TAG = "[WIFI]";

WifiManager g_wifi_manager;

WifiManager::WifiManager()
    : state_(WIFI_STATE_DISCONNECTED),
      lastReconnectAttemptMs_(0),
      reconnectAttempts_(0) {}

static bool startProvisioningAp() {
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.setSleep(false);
    WiFi.mode(WIFI_AP);
    delay(100);

    if (!WiFi.softAPConfig(AP_FALLBACK_IP, AP_FALLBACK_GATEWAY, AP_FALLBACK_SUBNET)) {
        ESP_LOGE(TAG, "softAPConfig() failed");
        return false;
    }

    for (uint8_t attempt = 1; attempt <= 3; ++attempt) {
        if (WiFi.softAP(AP_FALLBACK_SSID, AP_FALLBACK_PASSWORD, 1, false, 4)) {
            delay(100);
            if (WiFi.softAPIP() == AP_FALLBACK_IP) {
                ESP_LOGI(TAG, "Provisioning AP VERIFIED: SSID='%s' IP=%s channel=1",
                         AP_FALLBACK_SSID, WiFi.softAPIP().toString().c_str());
                return true;
            }
        }

        ESP_LOGW(TAG, "Provisioning AP start attempt %u failed", attempt);
        WiFi.softAPdisconnect(true);
        delay(150);
    }

    ESP_LOGE(TAG, "Provisioning AP FAILED after 3 attempts");
    return false;
}

bool WifiManager::init() {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    delay(100);

    String hostname = g_settings.getHostname();
    WiFi.setHostname(hostname.c_str());

    // Always create the recovery AP first. Stored/stale STA credentials must
    // never prevent the provisioning network from being created.
    bool apReady = startProvisioningAp();

    if (g_settings.hasWifiCredentials()) {
        String ssid = g_settings.getWifiSsid();
        String pass = g_settings.getWifiPassword();
        configuredSsid_ = ssid;
        ESP_LOGI(TAG, "Stored Wi-Fi credentials found for SSID: %s", ssid.c_str());

        if (apReady) {
            // Keep the provisioning AP available until the saved STA connection
            // succeeds. There is no arbitrary provisioning timeout.
            state_ = WIFI_STATE_AP_MODE;
                    reconnectAttempts_ = 0;
            ESP_LOGI(TAG, "Provisioning AP remains available until saved STA connection succeeds");
            // Do not enter AP+STA automatically here. The AP must remain
            // standalone and reliable until the user provisions/starts STA.
            return true;
        }

        return connectStation(ssid, pass);
    }

    state_ = WIFI_STATE_AP_MODE;
    reconnectAttempts_ = 0;
    ESP_LOGI(TAG, "No stored Wi-Fi credentials. Provisioning AP is active.");
    return apReady;
}

bool WifiManager::connectStation(const String& ssid, const String& password) {
    configuredSsid_ = ssid;
    provisioningGraceUntilMs_ = 0;
    state_ = WIFI_STATE_CONNECTING;
    reconnectAttempts_ = 0;

    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    if (WiFi.softAPIP() != AP_FALLBACK_IP) {
        WiFi.softAPConfig(AP_FALLBACK_IP, AP_FALLBACK_GATEWAY, AP_FALLBACK_SUBNET);
        WiFi.softAP(AP_FALLBACK_SSID, AP_FALLBACK_PASSWORD, 1, false, 4);
    }

    WiFi.begin(ssid.c_str(), password.c_str());
    lastReconnectAttemptMs_ = millis();

    ESP_LOGI(TAG, "Provisioning AP: SSID='%s' IP=%s",
             AP_FALLBACK_SSID, WiFi.softAPIP().toString().c_str());
    ESP_LOGI(TAG, "Initiated Wi-Fi STA connection to '%s'", ssid.c_str());
    return true;
}

void WifiManager::startApMode() {
    state_ = WIFI_STATE_AP_MODE;
    reconnectAttempts_ = 0;
    provisioningGraceUntilMs_ = 0;
    startProvisioningAp();

    ESP_LOGI(TAG, "Provisioning web interface: http://%s/", AP_FALLBACK_IP.toString().c_str());
    ESP_LOGI(TAG, "Provisioning AP credentials: SSID='%s', password='%s'",
             AP_FALLBACK_SSID, AP_FALLBACK_PASSWORD);
}

void WifiManager::update() {
    uint32_t now = millis();

    if (state_ == WIFI_STATE_AP_MODE) {
        // Provisioning AP is intentionally persistent. A saved STA profile is
        // only started when the user submits/starts provisioning via the web UI,
        // so AP+STA can never take the radio away from the setup network merely
        // because credentials happen to exist in NVS.
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (state_ != WIFI_STATE_CONNECTED) {
            state_ = WIFI_STATE_CONNECTED;
            reconnectAttempts_ = 0;
            ESP_LOGI(TAG, "Wi-Fi Connected! IP: %s, RSSI: %d dBm, Hostname: %s",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.getHostname());
        }
    } else if (state_ == WIFI_STATE_CONNECTING || state_ == WIFI_STATE_DISCONNECTED) {
        if (now - lastReconnectAttemptMs_ > 5000) {
            lastReconnectAttemptMs_ = now;
            reconnectAttempts_++;
            ESP_LOGI(TAG, "Reconnecting to Wi-Fi '%s' (Attempt %d)...",
                     configuredSsid_.c_str(), reconnectAttempts_);
            WiFi.reconnect();

            if (reconnectAttempts_ >= 15) {
                ESP_LOGW(TAG, "STA failed after 15 attempts; switching to standalone provisioning AP.");
                startApMode();
            }
        }
    }
}

String WifiManager::getSsid() const {
    if (state_ == WIFI_STATE_AP_MODE) return String(AP_FALLBACK_SSID);
    return WiFi.SSID();
}

int8_t WifiManager::getRssi() const {
    if (state_ != WIFI_STATE_CONNECTED) return 0;
    return WiFi.RSSI();
}

IPAddress WifiManager::getIpAddress() const {
    if (state_ == WIFI_STATE_AP_MODE || WiFi.localIP() == INADDR_NONE) return WiFi.softAPIP();
    return WiFi.localIP();
}

String WifiManager::getMacAddress() const {
    return WiFi.macAddress();
}

String WifiManager::getHostname() const {
    return String(WiFi.getHostname());
}
