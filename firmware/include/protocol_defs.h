/**
 * @file protocol_defs.h
 * @brief Binary protocol structures shared between ESP32 receiver and Android sender.
 * 
 * Must match protocol/WIFI_HIFI_PROTOCOL.md exactly.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WFHF_MAGIC_U32           0x46484657 // 'W', 'F', 'H', 'F' in Little-Endian
#define WFHF_MAGIC_STR           "WFHF"
#define WFHF_PROTOCOL_VERSION    1

enum WfhfPacketType : uint8_t {
    WFHF_PKT_AUDIO_PCM    = 0x01,
    WFHF_PKT_STREAM_SYNC  = 0x02,
    WFHF_PKT_STREAM_STOP  = 0x03,
    WFHF_PKT_PING         = 0x04
};

enum WfhfLatencyMode : uint8_t {
    WFHF_LATENCY_LOW      = 0x00,
    WFHF_LATENCY_BALANCED = 0x01,
    WFHF_LATENCY_STABLE   = 0x02
};

#pragma pack(push, 1)
/**
 * @brief 28-byte binary header preceding raw PCM audio payload.
 */
typedef struct {
    uint32_t magic;              // "WFHF" (0x46484657)
    uint8_t  version;            // Protocol version (0x01)
    uint8_t  packet_type;        // WfhfPacketType (0x01 for PCM audio)
    uint16_t stream_id;          // Stream session ID
    uint32_t sequence;           // Monotonically increasing sequence number
    uint64_t frame_position;     // Cumulative frame timestamp / sample index
    uint32_t sample_rate;        // Sample rate (e.g., 44100)
    uint8_t  channels;           // Channel count (2 = Stereo)
    uint8_t  bits_per_sample;    // Bits per sample (16)
    uint8_t  latency_mode;       // WfhfLatencyMode
    uint8_t  reserved;           // Padding / alignment (0)
    uint16_t payload_length;     // Number of PCM payload bytes following
    uint16_t checksum;           // Checksum or 0 if disabled
} wfhf_header_t;
#pragma pack(pop)

#define WFHF_HEADER_SIZE sizeof(wfhf_header_t) // Exactly 28 bytes

/**
 * @brief Real-time audio pipeline metrics
 */
typedef struct {
    uint64_t packets_received;
    uint64_t packets_lost;
    uint64_t packets_out_of_order;
    uint64_t packets_duplicate;
    uint64_t packets_malformed;
    uint32_t buffer_underruns;
    uint32_t buffer_overruns;
    uint32_t current_buffer_bytes;
    uint32_t buffer_capacity_bytes;
    float    buffer_fill_pct;
    float    packet_loss_pct;
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bits_per_sample;
    uint8_t  latency_mode;
    bool     is_streaming;
    char     connected_sender_ip[32];
} wfhf_audio_stats_t;

#ifdef __cplusplus
}
#endif
