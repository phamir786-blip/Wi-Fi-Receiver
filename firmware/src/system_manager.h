/**
 * @file system_manager.h
 * @brief System supervisor, watchdog feeder, and reboot/reset coordinator.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

class SystemManager {
public:
    SystemManager();
    bool init();
    void update();

    void requestReboot(uint32_t delayMs = 500);
    void requestFactoryReset();

    uint32_t getUptimeSeconds() const;
    uint32_t getFreeHeap() const;

private:
    void updateLed();

    uint32_t bootTimestampMs_;
    bool pendingReboot_;
    uint32_t rebootTriggerMs_;
    bool pendingReset_;
    uint32_t lastLedToggleMs_;
    bool ledState_;
};

extern SystemManager g_system_manager;
