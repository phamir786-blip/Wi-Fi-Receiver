/**
 * @file pcm_receiver.h
 * @brief Real-time UDP network audio packet receiver and validator.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "protocol_defs.h"
#include "config.h"

class PcmReceiver {
public:
    PcmReceiver();
    ~PcmReceiver();

    bool init(uint16_t port = NETWORK_AUDIO_UDP_PORT);
    void stop();

    void getStats(wfhf_audio_stats_t* outStats);
    void resetStats();

    bool isStreaming() const { return isStreaming_; }
    const char* getConnectedSenderIp() const { return connectedSenderIp_; }

    static void receiverTaskTrampoline(void* arg);
    void receiverTaskLoop();

private:
    uint16_t port_;
    int socketFd_;
    bool running_;
    TaskHandle_t taskHandle_;

    // Stream state tracking
    bool isStreaming_;
    uint16_t currentStreamId_;
    uint32_t expectedSequence_;
    bool firstPacketInStream_;
    uint32_t lastPacketTimestampMs_;
    char connectedSenderIp_[32];

    // Real Statistics
    uint64_t packetsReceived_;
    uint64_t packetsLost_;
    uint64_t packetsOutOfOrder_;
    uint64_t packetsDuplicate_;
    uint64_t packetsMalformed_;

    // Pre-allocated static UDP packet buffer (header + up to 4096 bytes PCM)
    static const size_t MAX_PACKET_SIZE = WFHF_HEADER_SIZE + 4096;
    uint8_t packetBuffer_[MAX_PACKET_SIZE];
};

extern PcmReceiver g_pcm_receiver;
