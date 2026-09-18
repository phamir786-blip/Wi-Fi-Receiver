/**
 * @file audio_output.h
 * @brief Dedicated real-time I2S audio output driver for NXP UDA1334A DAC.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"

class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();

    bool init(uint32_t sampleRate = AUDIO_DEFAULT_SAMPLE_RATE,
              uint8_t bitsPerSample = AUDIO_DEFAULT_BITS_PER_SAMPLE,
              uint8_t channels = AUDIO_DEFAULT_CHANNELS);

    void setVolume(uint8_t volume); // 0 - 100
    uint8_t getVolume() const { return volume_; }

    void setMute(bool mute);
    bool isMuted() const { return muted_; }

    bool isRunning() const { return running_; }
    void stop();

    uint32_t getSampleRate() const { return sampleRate_; }
    uint8_t getBitsPerSample() const { return bitsPerSample_; }
    uint8_t getChannels() const { return channels_; }

    // Audio Output Task entry point
    static void audioTaskTrampoline(void* arg);
    void audioTaskLoop();

private:
    uint32_t sampleRate_;
    uint8_t bitsPerSample_;
    uint8_t channels_;
    uint8_t volume_; // 0 to 100
    bool muted_;
    bool running_;
    TaskHandle_t taskHandle_;

    // Fast integer-based volume scaling: 0 - 256
    uint16_t volumeScale_;

    // Static scratch buffer for I2S DMA chunk write (avoid malloc in audio loop!)
    static const size_t DMA_CHUNK_BYTES = 1024;
    int16_t dmaScratchBuffer_[DMA_CHUNK_BYTES / sizeof(int16_t)];
};

extern AudioOutput g_audio_output;
