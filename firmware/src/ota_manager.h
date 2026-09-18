/**
 * @file ota_manager.h
 * @brief Over-The-Air (OTA) firmware upgrade manager with dual partition rollback.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <WString.h>
#include <Update.h>

class OtaManager {
public:
    OtaManager();
    bool beginUpdate(size_t expectedSize);
    size_t writeChunk(const uint8_t* data, size_t length);
    bool finishUpdate();
    void abortUpdate();

    bool isUpdating() const { return isUpdating_; }
    size_t getBytesWritten() const { return bytesWritten_; }
    size_t getTotalBytes() const { return totalBytes_; }
    float getProgressPct() const;
    String getLastError() const { return lastError_; }

private:
    bool isUpdating_;
    size_t bytesWritten_;
    size_t totalBytes_;
    String lastError_;
};

extern OtaManager g_ota_manager;
