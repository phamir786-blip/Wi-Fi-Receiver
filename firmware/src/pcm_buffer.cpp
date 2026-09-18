/**
 * @file pcm_buffer.cpp
 * @brief Thread-safe PCM Ring Buffer implementation with jitter absorption and underrun protection.
 */

#include "pcm_buffer.h"
#include <string.h>
#include <esp_log.h>

static const char* TAG = "[PCM_BUF]";

PcmBuffer g_pcm_buffer;

PcmBuffer::PcmBuffer()
    : buffer_(nullptr),
      capacity_(0),
      readPos_(0),
      writePos_(0),
      count_(0),
      latencyMode_(WFHF_LATENCY_BALANCED),
      prerollThresholdBytes_(LATENCY_BALANCED_THRESHOLD_BYTES),
      isPrerolling_(true),
      underrunCount_(0),
      overrunCount_(0),
      mutex_(nullptr) {
}

PcmBuffer::~PcmBuffer() {
    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;
    }
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

bool PcmBuffer::init(size_t capacity) {
    capacity_ = capacity;
    buffer_ = (uint8_t*)malloc(capacity_);
    if (!buffer_) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for PCM ring buffer", (unsigned int)capacity_);
        return false;
    }

    memset(buffer_, 0, capacity_);
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create buffer mutex");
        free(buffer_);
        buffer_ = nullptr;
        return false;
    }

    reset();
    setLatencyMode(WFHF_LATENCY_BALANCED);
    ESP_LOGI(TAG, "PCM Buffer initialized: %u bytes (%u ms at 44.1k/16/stereo)", 
             (unsigned int)capacity_, (unsigned int)(capacity_ * 1000 / (44100 * 4)));
    return true;
}

void PcmBuffer::reset() {
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        readPos_ = 0;
        writePos_ = 0;
        count_ = 0;
        isPrerolling_ = true;
        if (buffer_) {
            memset(buffer_, 0, capacity_);
        }
        xSemaphoreGive(mutex_);
    }
}

void PcmBuffer::setLatencyMode(WfhfLatencyMode mode) {
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        latencyMode_ = mode;
        switch (mode) {
            case WFHF_LATENCY_LOW:
                prerollThresholdBytes_ = LATENCY_LOW_THRESHOLD_BYTES;
                break;
            case WFHF_LATENCY_STABLE:
                prerollThresholdBytes_ = LATENCY_STABLE_THRESHOLD_BYTES;
                break;
            case WFHF_LATENCY_BALANCED:
            default:
                prerollThresholdBytes_ = LATENCY_BALANCED_THRESHOLD_BYTES;
                break;
        }
        // If current count is less than threshold, enter preroll
        if (count_ < prerollThresholdBytes_) {
            isPrerolling_ = true;
        }
        xSemaphoreGive(mutex_);
    }
    ESP_LOGI(TAG, "Latency mode set to %d (Threshold: %u bytes)", (int)mode, (unsigned int)prerollThresholdBytes_);
}

size_t PcmBuffer::write(const uint8_t* data, size_t length) {
    if (!buffer_ || !data || length == 0 || !mutex_) {
        return 0;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0; // Don't block network task if mutex contention
    }

    size_t freeBytes = capacity_ - count_;
    if (length > freeBytes) {
        // Buffer overrun: discard oldest data by advancing readPos
        size_t overflow = length - freeBytes;
        readPos_ = (readPos_ + overflow) % capacity_;
        count_ -= overflow;
        overrunCount_++;
    }

    // Write chunk 1: from writePos_ to end of buffer
    size_t firstPart = capacity_ - writePos_;
    if (firstPart >= length) {
        memcpy(buffer_ + writePos_, data, length);
        writePos_ = (writePos_ + length) % capacity_;
    } else {
        memcpy(buffer_ + writePos_, data, firstPart);
        memcpy(buffer_, data + firstPart, length - firstPart);
        writePos_ = length - firstPart;
    }

    count_ += length;

    // Check if preroll threshold has been reached
    if (isPrerolling_ && count_ >= prerollThresholdBytes_) {
        isPrerolling_ = false;
        ESP_LOGI(TAG, "Preroll complete! Buffered %u bytes. Starting playback.", (unsigned int)count_);
    }

    xSemaphoreGive(mutex_);
    return length;
}

size_t PcmBuffer::read(uint8_t* destination, size_t length) {
    if (!buffer_ || !destination || length == 0 || !mutex_) {
        return 0;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
        memset(destination, 0, length);
        return 0;
    }

    // While in preroll mode, output silence to let buffer fill up smoothly
    if (isPrerolling_) {
        memset(destination, 0, length);
        xSemaphoreGive(mutex_);
        return length;
    }

    if (count_ == 0) {
        // Buffer underrun! Fill with silence and trigger preroll
        underrunCount_++;
        isPrerolling_ = true;
        memset(destination, 0, length);
        xSemaphoreGive(mutex_);
        return length;
    }

    size_t bytesToRead = (length < count_) ? length : count_;

    // Read chunk 1: from readPos_ to end of buffer
    size_t firstPart = capacity_ - readPos_;
    if (firstPart >= bytesToRead) {
        memcpy(destination, buffer_ + readPos_, bytesToRead);
        readPos_ = (readPos_ + bytesToRead) % capacity_;
    } else {
        memcpy(destination, buffer_ + readPos_, firstPart);
        memcpy(destination + firstPart, buffer_, bytesToRead - firstPart);
        readPos_ = bytesToRead - firstPart;
    }

    count_ -= bytesToRead;

    // If we could not satisfy full length, fill remainder with silence
    if (bytesToRead < length) {
        memset(destination + bytesToRead, 0, length - bytesToRead);
        underrunCount_++;
        isPrerolling_ = true;
    }

    xSemaphoreGive(mutex_);
    return length;
}

size_t PcmBuffer::getAvailableBytes() {
    size_t ret = 0;
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
        ret = count_;
        xSemaphoreGive(mutex_);
    }
    return ret;
}

size_t PcmBuffer::getFreeBytes() {
    size_t ret = 0;
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
        ret = capacity_ - count_;
        xSemaphoreGive(mutex_);
    }
    return ret;
}

float PcmBuffer::getFillPercentage() {
    size_t c = getAvailableBytes();
    if (capacity_ == 0) return 0.0f;
    return (float)c / (float)capacity_ * 100.0f;
}

bool PcmBuffer::isPrerollComplete() {
    return !isPrerolling_;
}
