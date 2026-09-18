package com.wifihifi.app.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Binary protocol definitions matching ESP32-C3 firmware and WIFI_HIFI_PROTOCOL.md.
 *
 * Header layout is 32 bytes:
 * 4 + 1 + 1 + 2 + 4 + 8 + 4 + 1 + 1 + 1 + 1 + 2 + 2 = 32.
 */
object WiFiHiFiProtocol {
    const val MAGIC_U32: Int = 0x46484657 // "WFHF" in Little-Endian
    const val PROTOCOL_VERSION: Byte = 0x01

    const val PKT_AUDIO_PCM: Byte = 0x01
    const val PKT_STREAM_SYNC: Byte = 0x02
    const val PKT_STREAM_STOP: Byte = 0x03
    const val PKT_PING: Byte = 0x04

    const val LATENCY_LOW: Byte = 0x00
    const val LATENCY_BALANCED: Byte = 0x01
    const val LATENCY_STABLE: Byte = 0x02

    const val HEADER_SIZE = 32

    const val DEFAULT_AUDIO_PORT = 50005
    const val DEFAULT_DISCOVERY_PORT = 50006
    const val DEFAULT_HTTP_PORT = 80

    const val DEFAULT_SAMPLE_RATE = 44100
    const val DEFAULT_CHANNELS: Byte = 2
    const val DEFAULT_BITS_PER_SAMPLE: Byte = 16
    const val BYTES_PER_FRAME = 4 // 2 channels * 2 bytes

    /**
     * Packs the 32-byte binary header into a pre-allocated ByteBuffer at position 0.
     */
    fun packHeader(
        buffer: ByteBuffer,
        packetType: Byte,
        streamId: Short,
        sequence: Int,
        framePosition: Long,
        sampleRate: Int,
        channels: Byte,
        bitsPerSample: Byte,
        latencyMode: Byte,
        payloadLength: Short,
        checksum: Short = 0
    ) {
        buffer.order(ByteOrder.LITTLE_ENDIAN)
        buffer.position(0)
        buffer.putInt(MAGIC_U32)
        buffer.put(PROTOCOL_VERSION)
        buffer.put(packetType)
        buffer.putShort(streamId)
        buffer.putInt(sequence)
        buffer.putLong(framePosition)
        buffer.putInt(sampleRate)
        buffer.put(channels)
        buffer.put(bitsPerSample)
        buffer.put(latencyMode)
        buffer.put(0.toByte()) // Reserved
        buffer.putShort(payloadLength)
        buffer.putShort(checksum)
        check(buffer.position() == HEADER_SIZE) { "WiFi-HiFi header size mismatch" }
    }
}
