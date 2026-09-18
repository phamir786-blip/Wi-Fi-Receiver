/**
 * @file discovery.h
 * @brief Automatic network discovery service (mDNS and UDP broadcast beacon).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <WiFiUdp.h>
#include "config.h"

class DiscoveryService {
public:
    DiscoveryService();
    ~DiscoveryService();

    bool begin();
    void update();
    void stop();

private:
    WiFiUDP udp_;
    bool running_;
    uint32_t lastBeaconMs_;

    void handleUdpPacket();
    void sendDiscoveryResponse(IPAddress remoteIp, uint16_t remotePort);
};

extern DiscoveryService g_discovery;
