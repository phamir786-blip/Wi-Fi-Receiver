/**
 * @file system_manager.cpp
 * @brief System manager implementation with clean reboot handling.
 */

#include "system_manager.h"
#include "settings.h"
#include "wifi_manager.h"
#include "pcm_receiver.h"
#include "config.h"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_system.h>

static const char* TAG = "[SYSTEM]";

SystemManager g_system_manager;

SystemManager::SystemManager()
    : bootTimestampMs_(0),
      pendingReboot_(false),
      rebootTriggerMs_(0),
      pendingReset_(false),
      lastLedToggleMs_(0),
      ledState_(false) {}

bool SystemManager::init() {
    bootTimestampMs_ = millis();
    
    // Initialize onboard LED (GPIO 8 on ESP32-C3 Super Mini)
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE); // Solid ON at boot
    ledState_ = true;

    ESP_LOGI(TAG, "WiFi-HiFi System Manager initialized (ESP32-C3 RISC-V, Free Heap: %u bytes)", 
             (unsigned int)ESP.getFreeHeap());
    return true;
}

void SystemManager::update() {
    updateLed();

    if (pendingReset_) {
        ESP_LOGW(TAG, "Executing factory reset of settings...");
        g_settings.factoryReset();
        pendingReset_ = false;
        requestReboot(500);
    }

    if (pendingReboot_) {
        if (millis() >= rebootTriggerMs_) {
            ESP_LOGW(TAG, "Rebooting system now...");
            delay(100);
            ESP.restart();
        }
    }
}

void SystemManager::updateLed() {
    uint32_t now = millis();
    uint32_t intervalMs = 1000;

    if (g_wifi_manager.isApMode()) {
        // Fast flashing in AP provisioning mode (150ms)
        intervalMs = 150;
    } else if (g_wifi_manager.getState() == WIFI_STATE_CONNECTING) {
        // Medium flashing while connecting (500ms)
        intervalMs = 500;
    } else if (g_wifi_manager.isConnected()) {
        if (g_pcm_receiver.isStreaming()) {
            // Solid ON when actively receiving audio stream
            digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE);
            return;
        } else {
            // Calm heartbeat when idle connected (brief 80ms blink every 2000ms)
            uint32_t cycle = now % 2000;
            digitalWrite(STATUS_LED_PIN, (cycle < 80) ? STATUS_LED_ACTIVE : STATUS_LED_INACTIVE);
            return;
        }
    } else {
        // Disconnected: 800ms blink
        intervalMs = 800;
    }

    if (now - lastLedToggleMs_ >= intervalMs) {
        lastLedToggleMs_ = now;
        ledState_ = !ledState_;
        digitalWrite(STATUS_LED_PIN, ledState_ ? STATUS_LED_ACTIVE : STATUS_LED_INACTIVE);
    }
}

void SystemManager::requestReboot(uint32_t delayMs) {
    pendingReboot_ = true;
    rebootTriggerMs_ = millis() + delayMs;
    ESP_LOGI(TAG, "Reboot requested in %u ms", (unsigned int)delayMs);
}

void SystemManager::requestFactoryReset() {
    pendingReset_ = true;
}

uint32_t SystemManager::getUptimeSeconds() const {
    return (millis() - bootTimestampMs_) / 1000;
}

uint32_t SystemManager::getFreeHeap() const {
    return ESP.getFreeHeap();
}
