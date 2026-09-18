/**
 * @file ota_manager.cpp
 * @brief OTA update implementation leveraging ESP32 Flash partition verification.
 */

#include "ota_manager.h"
#include <esp_log.h>
#include <esp_ota_ops.h>

static const char* TAG = "[OTA]";

OtaManager g_ota_manager;

OtaManager::OtaManager()
    : isUpdating_(false),
      bytesWritten_(0),
      totalBytes_(0),
      lastError_("") {}

bool OtaManager::beginUpdate(size_t expectedSize) {
    totalBytes_ = expectedSize;
    bytesWritten_ = 0;
    lastError_ = "";

    ESP_LOGI(TAG, "Starting OTA update. Expected image size: %u bytes", (unsigned int)expectedSize);

    if (expectedSize == 0) {
        lastError_ = "Firmware image size cannot be 0";
        ESP_LOGE(TAG, "%s", lastError_.c_str());
        return false;
    }

    if (!Update.begin(expectedSize, U_FLASH)) {
        lastError_ = "Update.begin failed (Error: " + String(Update.getError()) + ")";
        ESP_LOGE(TAG, "%s", lastError_.c_str());
        return false;
    }

    isUpdating_ = true;
    return true;
}

size_t OtaManager::writeChunk(const uint8_t* data, size_t length) {
    if (!isUpdating_) {
        lastError_ = "OTA not started";
        return 0;
    }

    size_t written = Update.write(const_cast<uint8_t*>(data), length);
    if (written != length) {
        lastError_ = "Update.write failed (Error: " + String(Update.getError()) + ")";
        ESP_LOGE(TAG, "%s", lastError_.c_str());
        abortUpdate();
        return 0;
    }

    bytesWritten_ += written;
    return written;
}

bool OtaManager::finishUpdate() {
    if (!isUpdating_) return false;

    if (!Update.end(true)) {
        lastError_ = "Update.end failed (Error: " + String(Update.getError()) + ")";
        ESP_LOGE(TAG, "%s", lastError_.c_str());
        isUpdating_ = false;
        return false;
    }

    if (!Update.isFinished()) {
        lastError_ = "Update not completed successfully";
        ESP_LOGE(TAG, "%s", lastError_.c_str());
        isUpdating_ = false;
        return false;
    }

    ESP_LOGI(TAG, "OTA update successfully validated! Device will reboot into new firmware.");
    isUpdating_ = false;
    return true;
}

void OtaManager::abortUpdate() {
    if (isUpdating_) {
        Update.abort();
        isUpdating_ = false;
        ESP_LOGW(TAG, "OTA update aborted");
    }
}

float OtaManager::getProgressPct() const {
    if (totalBytes_ == 0) return 0.0f;
    return ((float)bytesWritten_ / (float)totalBytes_) * 100.0f;
}
