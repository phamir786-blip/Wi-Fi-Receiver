/**
 * @file pcm_receiver.cpp
 * @brief UDP audio receiver implementation with packet validation and statistics tracking.
 */

#include <Arduino.h>
#ifdef INADDR_NONE
#undef INADDR_NONE
#endif

#include "pcm_receiver.h"
#include "pcm_buffer.h"
#include "audio_output.h"
#include <esp_log.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <string.h>

static const char* TAG = "[PCM_RX]";

PcmReceiver g_pcm_receiver;

PcmReceiver::PcmReceiver()
    : port_(NETWORK_AUDIO_UDP_PORT),
      socketFd_(-1),
      running_(false),
      taskHandle_(nullptr),
      isStreaming_(false),
      currentStreamId_(0),
      expectedSequence_(0),
      firstPacketInStream_(true),
      lastPacketTimestampMs_(0),
      packetsReceived_(0),
      packetsLost_(0),
      packetsOutOfOrder_(0),
      packetsDuplicate_(0),
      packetsMalformed_(0) {
    memset(connectedSenderIp_, 0, sizeof(connectedSenderIp_));
    memset(packetBuffer_, 0, sizeof(packetBuffer_));
}

PcmReceiver::~PcmReceiver() {
    stop();
}

bool PcmReceiver::init(uint16_t port) {
    port_ = port;

    socketFd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketFd_ < 0) {
        ESP_LOGE(TAG, "Unable to create UDP socket: errno %d", errno);
        return false;
    }

    int enable = 1;
    setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    // Set socket receive buffer to 16KB (preserves heap on single-core 384KB SRAM ESP32-C3)
    int rcvbuf = 16 * 1024;
    setsockopt(socketFd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    // Set receive timeout to 500ms so receiver task can periodically check streaming timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(socketFd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_);

    int err = bind(socketFd_, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    running_ = true;
    BaseType_t res = xTaskCreatePinnedToCore(
        receiverTaskTrampoline,
        "wfhf_pcm_rx",
        TASK_UDP_STACK_SIZE,
        this,
        TASK_UDP_PRIORITY,
        &taskHandle_,
        0
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP receiver task");
        close(socketFd_);
        socketFd_ = -1;
        running_ = false;
        return false;
    }

    ESP_LOGI(TAG, "UDP PCM Audio Receiver listening on port %u (priority %d)", port_, TASK_UDP_PRIORITY);
    return true;
}

void PcmReceiver::stop() {
    running_ = false;
    if (socketFd_ >= 0) {
        close(socketFd_);
        socketFd_ = -1;
    }
    if (taskHandle_) {
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }
}

void PcmReceiver::resetStats() {
    packetsReceived_ = 0;
    packetsLost_ = 0;
    packetsOutOfOrder_ = 0;
    packetsDuplicate_ = 0;
    packetsMalformed_ = 0;
}

void PcmReceiver::getStats(wfhf_audio_stats_t* outStats) {
    if (!outStats) return;

    outStats->packets_received = packetsReceived_;
    outStats->packets_lost = packetsLost_;
    outStats->packets_out_of_order = packetsOutOfOrder_;
    outStats->packets_duplicate = packetsDuplicate_;
    outStats->packets_malformed = packetsMalformed_;

    outStats->buffer_underruns = g_pcm_buffer.getUnderrunCount();
    outStats->buffer_overruns = g_pcm_buffer.getOverrunCount();
    outStats->current_buffer_bytes = g_pcm_buffer.getAvailableBytes();
    outStats->buffer_capacity_bytes = g_pcm_buffer.getCapacity();
    outStats->buffer_fill_pct = g_pcm_buffer.getFillPercentage();

    uint64_t totalExpected = packetsReceived_ + packetsLost_;
    if (totalExpected > 0) {
        outStats->packet_loss_pct = ((float)packetsLost_ / (float)totalExpected) * 100.0f;
    } else {
        outStats->packet_loss_pct = 0.0f;
    }

    outStats->sample_rate = g_audio_output.getSampleRate();
    outStats->channels = g_audio_output.getChannels();
    outStats->bits_per_sample = g_audio_output.getBitsPerSample();
    outStats->latency_mode = (uint8_t)g_pcm_buffer.getLatencyMode();
    outStats->is_streaming = isStreaming_;
    strncpy(outStats->connected_sender_ip, connectedSenderIp_, sizeof(outStats->connected_sender_ip) - 1);
}

void PcmReceiver::receiverTaskTrampoline(void* arg) {
    PcmReceiver* instance = static_cast<PcmReceiver*>(arg);
    instance->receiverTaskLoop();
}

void PcmReceiver::receiverTaskLoop() {
    struct sockaddr_in sourceAddr;
    socklen_t socklen = sizeof(sourceAddr);

    while (running_) {
        int len = recvfrom(socketFd_, packetBuffer_, sizeof(packetBuffer_), 0, 
                           (struct sockaddr*)&sourceAddr, &socklen);

        uint32_t now = millis();

        if (len < 0) {
            // Timeout or error
            if (isStreaming_ && (now - lastPacketTimestampMs_ > 3000)) {
                // Sender silent for > 3 seconds, set stream state to idle
                isStreaming_ = false;
                ESP_LOGI(TAG, "Audio stream timed out (sender idle). Ready for new stream.");
            }
            continue;
        }

        // Validate minimum size
        if (len < (int)WFHF_HEADER_SIZE) {
            packetsMalformed_++;
            continue;
        }

        const wfhf_header_t* hdr = reinterpret_cast<const wfhf_header_t*>(packetBuffer_);

        // 1. Validate Magic
        if (hdr->magic != WFHF_MAGIC_U32) {
            packetsMalformed_++;
            continue;
        }

        // 2. Validate Protocol Version
        if (hdr->version != WFHF_PROTOCOL_VERSION) {
            packetsMalformed_++;
            continue;
        }

        // Handle Stream Stop packet
        if (hdr->packet_type == WFHF_PKT_STREAM_STOP) {
            isStreaming_ = false;
            g_pcm_buffer.reset();
            ESP_LOGI(TAG, "Stream stopped explicitly by sender.");
            continue;
        }

        // Validate Audio Format
        if (hdr->channels != 2 || hdr->bits_per_sample != 16 || hdr->sample_rate != 44100) {
            // Milestone 1 only accepts 44.1kHz 16-bit Stereo
            packetsMalformed_++;
            continue;
        }

        // Validate payload length
        size_t expectedPayload = hdr->payload_length;
        if ((size_t)len != WFHF_HEADER_SIZE + expectedPayload) {
            packetsMalformed_++;
            continue;
        }

        // Detect new stream or reconnect
        if (!isStreaming_ || hdr->stream_id != currentStreamId_) {
            currentStreamId_ = hdr->stream_id;
            expectedSequence_ = hdr->sequence;
            firstPacketInStream_ = true;
            isStreaming_ = true;
            inet_ntoa_r(sourceAddr.sin_addr, connectedSenderIp_, sizeof(connectedSenderIp_));
            g_pcm_buffer.reset();
            ESP_LOGI(TAG, "New audio stream started from %s (Stream ID: 0x%04X, Seq: %u)",
                     connectedSenderIp_, currentStreamId_, (unsigned int)hdr->sequence);
        }

        // Sequence number validation
        if (firstPacketInStream_) {
            expectedSequence_ = hdr->sequence + 1;
            firstPacketInStream_ = false;
        } else {
            if (hdr->sequence == expectedSequence_) {
                expectedSequence_++;
            } else if (hdr->sequence < expectedSequence_) {
                // Old or duplicate packet
                if (expectedSequence_ - hdr->sequence < 500) {
                    packetsDuplicate_++;
                } else {
                    packetsOutOfOrder_++;
                }
            } else {
                // Gap in sequence = Packet loss
                uint32_t lost = hdr->sequence - expectedSequence_;
                packetsLost_ += lost;
                expectedSequence_ = hdr->sequence + 1;
            }
        }

        packetsReceived_++;
        lastPacketTimestampMs_ = now;

        // Write PCM payload into ring buffer
        const uint8_t* pcmPayload = packetBuffer_ + WFHF_HEADER_SIZE;
        g_pcm_buffer.write(pcmPayload, expectedPayload);
    }

    vTaskDelete(NULL);
}
