package com.wifihifi.app.audio

import com.wifihifi.app.protocol.WiFiHiFiProtocol
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.random.Random

/**
 * Packs continuous PCM audio stream into framed binary UDP packets.
 * Zero object allocations during streaming.
 */
class PcmPacketizer(
    private val onPacketReady: (buffer: ByteBuffer, length: Int) -> Unit
) {
    // 10ms chunk at 44.1kHz 16-bit stereo: 441 frames * 4 bytes/frame = 1764 bytes
    companion object {
        const val SAMPLES_PER_PACKET = 441
        const val PAYLOAD_BYTES_PER_PACKET = SAMPLES_PER_PACKET * WiFiHiFiProtocol.BYTES_PER_FRAME // 1764 bytes
        const val TOTAL_PACKET_CAPACITY = WiFiHiFiProtocol.HEADER_SIZE + PAYLOAD_BYTES_PER_PACKET // 1792 bytes
    }

    private val sendBuffer = ByteBuffer.allocateDirect(TOTAL_PACKET_CAPACITY).apply {
        order(ByteOrder.LITTLE_ENDIAN)
    }

    private var streamId: Short = Random.nextInt(1, 0x7FFF).toShort()
    private var sequenceNumber: Int = 0
    private var cumulativeFramePosition: Long = 0L
    private var latencyMode: Byte = WiFiHiFiProtocol.LATENCY_BALANCED

    // Internal accumulation buffer for streaming partial PCM reads
    private val accumulator = ByteBuffer.allocateDirect(PAYLOAD_BYTES_PER_PACKET * 2).apply {
        order(ByteOrder.LITTLE_ENDIAN)
    }

    fun startNewStream(mode: Byte = WiFiHiFiProtocol.LATENCY_BALANCED) {
        streamId = Random.nextInt(1, 0x7FFF).toShort()
        sequenceNumber = 0
        cumulativeFramePosition = 0L
        latencyMode = mode
        accumulator.clear()
    }

    fun setLatencyMode(mode: Byte) {
        latencyMode = mode
    }

    /**
     * Feeds incoming PCM bytes into packetizer.
     */
    fun feedPcm(data: ByteArray, offset: Int, length: Int) {
        var bytesProcessed = 0

        while (bytesProcessed < length) {
            val toCopy = minOf(length - bytesProcessed, accumulator.remaining())
            accumulator.put(data, offset + bytesProcessed, toCopy)
            bytesProcessed += toCopy

            // If we have enough bytes for a complete 10ms audio packet, flush it
            if (accumulator.position() >= PAYLOAD_BYTES_PER_PACKET) {
                accumulator.flip()

                // Pack header
                WiFiHiFiProtocol.packHeader(
                    buffer = sendBuffer,
                    packetType = WiFiHiFiProtocol.PKT_AUDIO_PCM,
                    streamId = streamId,
                    sequence = sequenceNumber++,
                    framePosition = cumulativeFramePosition,
                    sampleRate = WiFiHiFiProtocol.DEFAULT_SAMPLE_RATE,
                    channels = WiFiHiFiProtocol.DEFAULT_CHANNELS,
                    bitsPerSample = WiFiHiFiProtocol.DEFAULT_BITS_PER_SAMPLE,
                    latencyMode = latencyMode,
                    payloadLength = PAYLOAD_BYTES_PER_PACKET.toShort(),
                    checksum = 0
                )

                // Copy payload
                sendBuffer.position(WiFiHiFiProtocol.HEADER_SIZE)
                val payloadSlice = accumulator.slice().apply { limit(PAYLOAD_BYTES_PER_PACKET) }
                sendBuffer.put(payloadSlice)

                // Advance accumulator
                accumulator.position(accumulator.position() + PAYLOAD_BYTES_PER_PACKET)
                accumulator.compact()

                cumulativeFramePosition += SAMPLES_PER_PACKET

                // Send
                sendBuffer.position(0)
                onPacketReady(sendBuffer, TOTAL_PACKET_CAPACITY)
            }
        }
    }

    /**
     * Builds and sends a stream stop packet.
     */
    fun sendStreamStop() {
        WiFiHiFiProtocol.packHeader(
            buffer = sendBuffer,
            packetType = WiFiHiFiProtocol.PKT_STREAM_STOP,
            streamId = streamId,
            sequence = sequenceNumber++,
            framePosition = cumulativeFramePosition,
            sampleRate = WiFiHiFiProtocol.DEFAULT_SAMPLE_RATE,
            channels = WiFiHiFiProtocol.DEFAULT_CHANNELS,
            bitsPerSample = WiFiHiFiProtocol.DEFAULT_BITS_PER_SAMPLE,
            latencyMode = latencyMode,
            payloadLength = 0,
            checksum = 0
        )
        sendBuffer.position(0)
        onPacketReady(sendBuffer, WiFiHiFiProtocol.HEADER_SIZE)
    }
}
