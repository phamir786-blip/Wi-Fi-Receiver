/**
 * @file pcm_buffer.h
 * @brief High-performance FreeRTOS thread-safe PCM ring/jitter buffer.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"
#include "protocol_defs.h"

class PcmBuffer {
public:
    PcmBuffer();
    ~PcmBuffer();

    bool init(size_t capacity = PCM_RING_BUFFER_SIZE);
    void reset();

    // Write PCM data into jitter buffer (Called by UDP receiver task)
    // Returns number of bytes written
    size_t write(const uint8_t* data, size_t length);

    // Read PCM data from jitter buffer (Called by I2S audio output task)
    // If buffer has less than length bytes, fills remaining with silence and increments underrun count
    size_t read(uint8_t* destination, size_t length);

    // Latency mode configuration
    void setLatencyMode(WfhfLatencyMode mode);
    WfhfLatencyMode getLatencyMode() const { return latencyMode_; }

    // Buffer state queries
    size_t getAvailableBytes();
    size_t getFreeBytes();
    size_t getCapacity() const { return capacity_; }
    float getFillPercentage();
    bool isPrerollComplete();

    // Underrun & Overrun counters
    uint32_t getUnderrunCount() const { return underrunCount_; }
    uint32_t getOverrunCount() const { return overrunCount_; }

private:
    uint8_t* buffer_;
    size_t capacity_;
    size_t readPos_;
    size_t writePos_;
    size_t count_;

    WfhfLatencyMode latencyMode_;
    size_t prerollThresholdBytes_;
    bool isPrerolling_;

    uint32_t underrunCount_;
    uint32_t overrunCount_;

    SemaphoreHandle_t mutex_;
};

extern PcmBuffer g_pcm_buffer;
