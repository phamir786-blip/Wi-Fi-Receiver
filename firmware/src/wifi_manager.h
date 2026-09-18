/**
 * @file wifi_manager.h
 * @brief Wi-Fi Station & SoftAP provisioning manager with auto-reconnect.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <WiFi.h>
#include "config.h"

enum WifiState {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE
};

class WifiManager {
public:
    WifiManager();
    bool init();
    void update();

    WifiState getState() const { return state_; }
    bool isConnected() const { return (state_ == WIFI_STATE_CONNECTED); }
    bool isApMode() const { return (state_ == WIFI_STATE_AP_MODE); }

    String getSsid() const;
    int8_t getRssi() const;
    IPAddress getIpAddress() const;
    String getMacAddress() const;
    String getHostname() const;

    bool connectStation(const String& ssid, const String& password);
    void startApMode();

private:
    WifiState state_;
    uint32_t lastReconnectAttemptMs_;
    uint8_t reconnectAttempts_;
    String configuredSsid_;
};

extern WifiManager g_wifi_manager;
