/**
 * @file settings.h
 * @brief Non-volatile persistent configuration manager using ESP32 NVS.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <Preferences.h>
#include "protocol_defs.h"
#include "config.h"

class Settings {
public:
    Settings();
    bool begin();

    // Wi-Fi credentials
    bool hasWifiCredentials();
    String getWifiSsid();
    String getWifiPassword();
    bool setWifiCredentials(const String& ssid, const String& password);
    void clearWifiCredentials();

    // Device identification
    String getDeviceName();
    bool setDeviceName(const String& name);
    String getHostname();
    bool setHostname(const String& hostname);

    // Audio preferences
    uint8_t getVolume();
    bool setVolume(uint8_t volume);
    bool getMute();
    bool setMute(bool mute);
    WfhfLatencyMode getLatencyMode();
    bool setLatencyMode(WfhfLatencyMode mode);

    // Factory Reset
    void factoryReset();

private:
    Preferences prefs_;
    bool initialized_;
};

extern Settings g_settings;
